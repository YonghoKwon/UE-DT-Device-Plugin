#include "DeviceUI/DeviceSensorMonitorPanel.h"
#include "DeviceUI/DeviceSensorMonitorBody.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Device/Camera/VirtualCameraSensorActor.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualSensorEditableTypes.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void UDeviceSensorMonitorPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return; // A BP may supply its own presentation through the public API.
    auto* Border=WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(.025f,.035f,.05f,.95f));
    Border->SetPadding(FMargin(14));
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();
    Border->SetContent(Box); WidgetTree->RootWidget=Border;
    auto* Title=WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("DTDevice — 센서 모니터"))); Box->AddChildToVerticalBox(Title);
    SensorList=WidgetTree->ConstructWidget<UComboBoxString>();
    SensorList->OnSelectionChanged.AddDynamic(this,&UDeviceSensorMonitorPanel::HandleSelection);
    Box->AddChildToVerticalBox(SensorList);
    auto AddButton=[&](const TCHAR* Caption)
    {
        auto* Button=WidgetTree->ConstructWidget<UButton>();
        auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(Caption));
        Button->SetContent(Label); Box->AddChildToVerticalBox(Button); return Button;
    };
    AddButton(TEXT("센서 목록 새로 고침"))->OnClicked.AddDynamic(this,&UDeviceSensorMonitorPanel::RefreshSensors);
    AddButton(TEXT("1회 캡처"))->OnClicked.AddDynamic(this,&UDeviceSensorMonitorPanel::CaptureOnce);
    AddButton(TEXT("미리보기 점 늘리기"))->OnClicked.AddDynamic(this,&UDeviceSensorMonitorPanel::MorePreview);
    AddButton(TEXT("미리보기 점 줄이기"))->OnClicked.AddDynamic(this,&UDeviceSensorMonitorPanel::LessPreview);
    Summary=WidgetTree->ConstructWidget<UTextBlock>(); Summary->SetAutoWrapText(true); Box->AddChildToVerticalBox(Summary);
    auto* PreviewSize=WidgetTree->ConstructWidget<USizeBox>();
    PreviewSize->SetWidthOverride(440); PreviewSize->SetHeightOverride(220);
    Preview=WidgetTree->ConstructWidget<UImage>(); PreviewSize->SetContent(Preview); Box->AddChildToVerticalBox(PreviewSize);
}

void UDeviceSensorMonitorPanel::NativeConstruct()
{
    Super::NativeConstruct(); RefreshSensors();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&UDeviceSensorMonitorPanel::RefreshStatus,.2f,true);
}
void UDeviceSensorMonitorPanel::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    SelectedSensor.Reset(); Sensors.Reset(); LidarComp=nullptr; CameraComp=nullptr;
    Super::NativeDestruct();
}
void UDeviceSensorMonitorPanel::RefreshSensors()
{
    auto* Previous=SelectedSensor.Get(); Sensors.Reset(); bRefreshingList=true;
    if (SensorList) SensorList->ClearOptions();
    if (auto* World=GetWorld()) for (TActorIterator<AVirtualSensorActorBase> It(World); It; ++It)
    {
        Sensors.Add(*It);
        if (SensorList) SensorList->AddOption(FString::Printf(TEXT("%s [%s]"),*It->GetSensorId(),*It->GetName()));
    }
    bRefreshingList=false;
    SelectSensor(Sensors.Contains(Previous)?Previous:(Sensors.Num()?Sensors[0].Get():nullptr));
}
void UDeviceSensorMonitorPanel::SelectSensor(AVirtualSensorActorBase* Sensor)
{
    if (Sensor && Sensor->GetWorld()!=GetWorld()) return;
    SelectedSensor=Sensor;
    auto* Lidar=Cast<AVirtualLidarSensorActor>(Sensor); auto* Camera=Cast<AVirtualCameraSensorActor>(Sensor);
    LidarComp=Lidar?Lidar->ScanComponent.Get():nullptr; CameraComp=Camera?Camera->CaptureComponent.Get():nullptr;
    bShowingLidar=LidarComp!=nullptr;
    const int32 Index=Sensors.IndexOfByKey(Sensor);
    if (SensorList && Index!=INDEX_NONE && SensorList->GetSelectedIndex()!=Index)
    { bRefreshingList=true; SensorList->SetSelectedIndex(Index); bRefreshingList=false; }
    RefreshStatus();
}
void UDeviceSensorMonitorPanel::HandleSelection(FString Item,ESelectInfo::Type)
{
    if (bRefreshingList || !SensorList) return;
    const int32 Index=SensorList->FindOptionIndex(Item); SelectSensor(Sensors.IsValidIndex(Index)?Sensors[Index].Get():nullptr);
}
void UDeviceSensorMonitorPanel::RefreshStatus()
{
    if (!IsVisible()) return;
    auto* Sensor=SelectedSensor.Get();
    if (!Sensor) { LidarComp=nullptr; CameraComp=nullptr; }
    FString NewText=TEXT("센서 없음 — 맵에 LiDAR 또는 카메라를 배치하고 목록을 새로 고치세요.");
    if (Sensor)
    {
        const auto State=Sensor->GetSensorRuntimeStatus();
        NewText=Sensor->GetSensorId()+TEXT("\n")+FDeviceSensorMonitorBody::GetFrameSummaryText(*this)+TEXT("\n")
            +FDeviceSensorMonitorBody::GetMeasurementSummaryText(*this)+TEXT("\n")
            +FDeviceSensorMonitorBody::GetPreviewPolicySummaryText(*this)
            +FString::Printf(TEXT("\nServer points: %d | Preview points: %d\n%s"),
                LidarComp?LidarComp->GetLastServerPayloadPointCount():0,State.PreviewPointCount,*(State.PerformanceWarning.IsEmpty()?State.LastMessage:State.PerformanceWarning));
    }
    if (NewText!=StatusText) { StatusText=MoveTemp(NewText); if (Summary) Summary->SetText(FText::FromString(StatusText)); }
    if (Preview)
    {
        auto* Texture=Sensor?Sensor->GetSensorPreviewTexture():nullptr;
        if (Preview->GetBrush().GetResourceObject()!=Texture)
        { FSlateBrush Brush=Preview->GetBrush(); Brush.SetResourceObject(Texture); Preview->SetBrush(Brush); }
    }
}
void UDeviceSensorMonitorPanel::CaptureOnce()
{ if (auto* Sensor=SelectedSensor.Get()) { Sensor->CaptureSensorOnce(); RefreshStatus(); } }
bool UDeviceSensorMonitorPanel::AdjustPreview(int32 Direction)
{
    auto* Sensor=SelectedSensor.Get(); if (!LidarComp || !Sensor || !Direction) return false;
    FVirtualSensorEditableState State; FString Error;
    if (!Sensor->ReadEditableState(State)) return false;
    State.MaxPreviewPoints=FMath::Clamp(State.MaxPreviewPoints+(Direction>0?1000:-1000),1000,100000);
    const bool Applied=Sensor->ApplyEditableState(State,Error); RefreshStatus(); return Applied;
}
void UDeviceSensorMonitorPanel::MorePreview() { AdjustPreview(1); }
void UDeviceSensorMonitorPanel::LessPreview() { AdjustPreview(-1); }
