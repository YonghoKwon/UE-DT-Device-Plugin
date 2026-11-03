#include "DeviceUI/DeviceSensorMonitorHostActor.h"
#include "DeviceUI/DeviceSensorMonitorPanel.h"
#include "Core/DxWidgetSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
ADeviceSensorMonitorHostActor::ADeviceSensorMonitorHostActor()
{ PrimaryActorTick.bCanEverTick=false; PanelClass=UDeviceSensorMonitorPanel::StaticClass(); }
void ADeviceSensorMonitorHostActor::BeginPlay()
{ Super::BeginPlay(); if (bShowOnBeginPlay) ShowMonitor(); }
void ADeviceSensorMonitorHostActor::ShowMonitor()
{
    if (IsRunningDedicatedServer() || !GetGameInstance()) return;
    auto* PC=UGameplayStatics::GetPlayerController(this,0); if (!PC || !PC->IsLocalController()) return;
    if (!Panel) Panel=CreateWidget<UDeviceSensorMonitorPanel>(PC,PanelClass?PanelClass.Get():UDeviceSensorMonitorPanel::StaticClass());
    if (!Panel) return;
    if (!Panel->IsInViewport()) { Panel->AddToViewport(); Panel->SetDesiredSizeInViewport(FVector2D(480,650)); Panel->SetPositionInViewport(FVector2D(18,18)); }
    Panel->SetVisibility(ESlateVisibility::Visible);
    if (auto* Widgets=GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>())
    { Widgets->RegisterWidget(Panel); Widgets->RegisterExternalInputBlocker(Panel); Widgets->BringToFront(Panel); }
}
void ADeviceSensorMonitorHostActor::HideMonitor()
{
    if (!Panel) return;
    if (GetGameInstance()) if (auto* Widgets=GetGameInstance()->GetSubsystem<UDxWidgetSubsystem>())
    { Widgets->UnregisterExternalInputBlocker(Panel); Widgets->UnregisterWidget(Panel); }
    Panel->RemoveFromParent();
}
void ADeviceSensorMonitorHostActor::EndPlay(const EEndPlayReason::Type Reason)
{ HideMonitor(); Panel=nullptr; Super::EndPlay(Reason); }
