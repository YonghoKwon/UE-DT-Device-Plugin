#include "Device/Sensor/DeviceSensorCoordinator.h"

#include "Device/Camera/VirtualCameraCaptureComp.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Device/Sensor/RealSensorSourceComp.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarVisualizationComp.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "Device/Sensor/VirtualSensorRecorderComp.h"
#include "Device/Sensor/VirtualSensorActorBase.h"
#include "Device/Sensor/VirtualSensorMonitorView.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"




void ADeviceSensorCoordinator::SaveLidarViewState(UVirtualLidarScanComp* LidarComp)
{
    if (!LidarComp || LidarPointCloudOnlyStates.Contains(LidarComp))
    {
        return;
    }

    FDeviceLidarPointCloudOnlyState State;
    State.bPointCloudPreviewEnabled = LidarComp->IsPointCloudPreviewEnabled();
    State.bDrawDebugRays = LidarComp->bDrawDebugRays;
    State.bDrawPointCloudPreviewDebugPoints = LidarComp->bDrawPointCloudPreviewDebugPoints;
    State.PointCloudPreviewStride = LidarComp->PointCloudPreviewStride;
    State.MaxPointCloudPreviewInstances = LidarComp->MaxPointCloudPreviewInstances;
    State.PreviewPointStride = LidarComp->PreviewPointStride;
    State.MaxPreviewPoints = LidarComp->MaxPreviewPoints;
    if (const AVirtualLidarSensorActor* LidarActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
    {
        if (const UVirtualLidarVisualizationComp* Visualization = LidarActor->VisualizationComp)
        {
            State.bHadVisualizationComponent = true;
            State.bShowWorldPointCloud = Visualization->GetVisualizationSettings().bShowWorldPointCloud;
        }
    }
    LidarPointCloudOnlyStates.Add(LidarComp, State);
}

void ADeviceSensorCoordinator::ApplyPointCloudOnlyToLidar(UVirtualLidarScanComp* LidarComp, bool bSelected)
{
    if (!LidarComp)
    {
        return;
    }

    SaveLidarViewState(LidarComp);
    LidarComp->bDrawDebugRays = false;
    LidarComp->bDrawPointCloudPreviewDebugPoints = false;
    const int32 PreviewStride = FMath::Max(2, LidarComp->PreviewPointStride);
    const int32 PreviewMaxPoints = LidarComp->MaxPreviewPoints > 0
        ? FMath::Min(LidarComp->MaxPreviewPoints, 3000)
        : 3000;
	// Point-cloud-only changes visibility and sampling budget only. The user's
	// hit/miss filter remains authoritative and can be toggled while this mode is active.
    LidarComp->SetPreviewPolicy(PreviewStride, PreviewMaxPoints, LidarComp->bPointCloudPreviewHitOnly);
    LidarComp->SetPointCloudPreviewEnabled(bSelected);
    if (AVirtualLidarSensorActor* LidarActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
    {
        if (UVirtualLidarVisualizationComp* Visualization = LidarActor->VisualizationComp)
        {
            // The V2 visualization owns the final renderer choice. Keeping its
            // setting in sync prevents the next completed scan from disabling
            // the point-cloud-only preview again.
            Visualization->SetWorldPointCloudEnabled(bSelected);
        }
    }
}

void ADeviceSensorCoordinator::RestoreLidarViewState(UVirtualLidarScanComp* LidarComp)
{
    if (!LidarComp)
    {
        return;
    }

    if (FDeviceLidarPointCloudOnlyState* State = LidarPointCloudOnlyStates.Find(LidarComp))
    {
        LidarComp->bDrawDebugRays = State->bDrawDebugRays;
        LidarComp->bDrawPointCloudPreviewDebugPoints = State->bDrawPointCloudPreviewDebugPoints;
        LidarComp->PointCloudPreviewStride = State->PointCloudPreviewStride;
        LidarComp->MaxPointCloudPreviewInstances = State->MaxPointCloudPreviewInstances;
		LidarComp->SetPreviewPolicy(State->PreviewPointStride, State->MaxPreviewPoints, LidarComp->bPointCloudPreviewHitOnly);
        if (State->bHadVisualizationComponent)
        {
            if (AVirtualLidarSensorActor* LidarActor = Cast<AVirtualLidarSensorActor>(LidarComp->GetOwner()))
            {
                if (UVirtualLidarVisualizationComp* Visualization = LidarActor->VisualizationComp)
                {
                    Visualization->SetWorldPointCloudEnabled(State->bShowWorldPointCloud);
                }
            }
        }
        LidarComp->SetPointCloudPreviewEnabled(State->bPointCloudPreviewEnabled);
        LidarPointCloudOnlyStates.Remove(LidarComp);
    }
    else
    {
        LidarComp->SetPointCloudPreviewEnabled(false);
    }
}

ADeviceSensorCoordinator::ADeviceSensorCoordinator()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADeviceSensorCoordinator::BeginPlay()
{
    Super::BeginPlay();

    if (GetDeviceCoordinatorConfig().bDiscoverOnBeginPlay)
    {
        DiscoverSensorsInLevel();
    }

    ApplyWidgetBinding();

    if (GetDeviceCoordinatorConfig().bStartSensorsOnBeginPlay)
    {
        StartAllSensors();
    }

    if (GetDeviceCoordinatorConfig().bUseSynchronizedCapture && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(SynchronizedTimerHandle, this, &ADeviceSensorCoordinator::RunSynchronizedCapture, GetDeviceCoordinatorConfig().SynchronizedInterval, true, 0.0f);
    }
}

void ADeviceSensorCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RestorePointCloudOnlyVisibility();
    for(AVirtualSensorActorBase* A:SensorActors)if(IsValid(A))A->OnSensorActivated.RemoveAll(this);
    for (UVirtualLidarScanComp* LidarComp : Lidars)
    {
        RestoreLidarViewState(LidarComp);
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SynchronizedTimerHandle);
    }
    LidarPointCloudOnlyStates.Reset();
    MonitorOwner.Reset();
    SensorActors.Reset(); Cameras.Reset(); Lidars.Reset(); RealSensorSources.Reset();
    Super::EndPlay(EndPlayReason);
}

void ADeviceSensorCoordinator::DiscoverSensorsInLevel()
{
    for(AVirtualSensorActorBase* A:SensorActors)if(IsValid(A))A->OnSensorActivated.RemoveAll(this);
    Cameras.Reset();
    Lidars.Reset();
    RealSensorSources.Reset();
    SensorActors.Reset();
    UWorld* World = GetWorld();
    if (!World) return;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        if (AVirtualSensorActorBase* SensorActor = Cast<AVirtualSensorActorBase>(Actor))
        {
            RegisterSensorActor(SensorActor);
        }
        TArray<UVirtualCameraCaptureComp*> FoundCameras;
        Actor->GetComponents<UVirtualCameraCaptureComp>(FoundCameras);
        for (UVirtualCameraCaptureComp* CameraComp : FoundCameras)
        {
            RegisterCamera(CameraComp);
        }
        TArray<UVirtualLidarScanComp*> FoundLidars;
        Actor->GetComponents<UVirtualLidarScanComp>(FoundLidars);
        for (UVirtualLidarScanComp* LidarComp : FoundLidars)
        {
            RegisterLidar(LidarComp);
        }
        TArray<URealSensorSourceComp*> FoundRealSensorSources;
        Actor->GetComponents<URealSensorSourceComp>(FoundRealSensorSources);
        for (URealSensorSourceComp* RealSensorSourceComp : FoundRealSensorSources)
        {
            RegisterRealSensorSource(RealSensorSourceComp);
        }
    }
}

void ADeviceSensorCoordinator::RegisterCamera(UVirtualCameraCaptureComp* CameraComp)
{
    if (CameraComp && !Cameras.Contains(CameraComp))
    {
        Cameras.Add(CameraComp);
        if (!Cast<AVirtualSensorActorBase>(CameraComp->GetOwner()))
        {
            AssignSharedServicesIfPossible(CameraComp);
        }
        ApplyWidgetBinding();
    }
}

void ADeviceSensorCoordinator::RegisterLidar(UVirtualLidarScanComp* LidarComp)
{
    if (LidarComp && !Lidars.Contains(LidarComp))
    {
        Lidars.Add(LidarComp);
        if (!Cast<AVirtualSensorActorBase>(LidarComp->GetOwner()))
        {
            AssignSharedServicesIfPossible(LidarComp);
        }
        if (bPointCloudOnlyModeEnabled)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }
        ApplyWidgetBinding();
    }
}

void ADeviceSensorCoordinator::RegisterRealSensorSource(URealSensorSourceComp* RealSensorSourceComp)
{
    if (RealSensorSourceComp && !RealSensorSources.Contains(RealSensorSourceComp))
    {
        RealSensorSources.Add(RealSensorSourceComp);
        ApplyWidgetBinding();
    }
}

void ADeviceSensorCoordinator::SelectCameraByIndex(int32 Index)
{
    if (Cameras.IsValidIndex(Index))
    {
        SelectedCameraIndex = Index;
        ApplyWidgetBinding();
    }
}

void ADeviceSensorCoordinator::SelectLidarByIndex(int32 Index)
{
    if (Lidars.IsValidIndex(Index))
    {
        SelectedLidarIndex = Index;
        if (bPointCloudOnlyModeEnabled)
        {
            for (UVirtualLidarScanComp* LidarComp : Lidars)
            {
                ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
            }
        }
        ApplyWidgetBinding();
    }
}

void ADeviceSensorCoordinator::SelectRealSensorSourceByIndex(int32 Index)
{
    if (RealSensorSources.IsValidIndex(Index))
    {
        SelectedRealSensorSourceIndex = Index;
        ApplyWidgetBinding();
    }
}

void ADeviceSensorCoordinator::SelectNextCamera()
{
    if (Cameras.Num() <= 0)
    {
        return;
    }
    SelectedCameraIndex = (SelectedCameraIndex + 1) % Cameras.Num();
    ApplyWidgetBinding();
}

void ADeviceSensorCoordinator::SelectNextLidar()
{
    if (Lidars.Num() <= 0)
    {
        return;
    }
    SelectedLidarIndex = (SelectedLidarIndex + 1) % Lidars.Num();
    if (bPointCloudOnlyModeEnabled)
    {
        for (UVirtualLidarScanComp* LidarComp : Lidars)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }
    }
    ApplyWidgetBinding();
}

void ADeviceSensorCoordinator::SetViewMode(EVirtualSensorViewMode NewMode)
{
    if (NewMode == EVirtualSensorViewMode::PointCloudOnly)
    {
        SetPointCloudOnlyMode(true);
        return;
    }

    if (bPointCloudOnlyModeEnabled)
    {
        SetPointCloudOnlyMode(false);
    }

    CurrentViewMode = NewMode;
    if (GetMonitorView())
    {
        if (CurrentViewMode == EVirtualSensorViewMode::Camera) GetMonitorView()->ShowMonitorCamera();
        if (CurrentViewMode == EVirtualSensorViewMode::Lidar) GetMonitorView()->ShowMonitorLidar();
    }
    NotifyDeviceViewModeChanged(CurrentViewMode);
}

void ADeviceSensorCoordinator::SetPointCloudOnlyMode(bool bEnabled)
{
    if (bPointCloudOnlyModeEnabled == bEnabled)
    {
        return;
    }

    if (bEnabled)
    {
        PreviousViewModeBeforePointCloudOnly = CurrentViewMode == EVirtualSensorViewMode::PointCloudOnly
            ? EVirtualSensorViewMode::Lidar
            : CurrentViewMode;
        bPointCloudOnlyModeEnabled = true;
        CurrentViewMode = EVirtualSensorViewMode::PointCloudOnly;

        if (GetDeviceCoordinatorConfig().bPointCloudOnlyHideWorld)
        {
            ApplyPointCloudOnlyVisibility();
        }

        for (UVirtualLidarScanComp* LidarComp : Lidars)
        {
            ApplyPointCloudOnlyToLidar(LidarComp, LidarComp == GetSelectedLidar());
        }

        if (GetDeviceCoordinatorConfig().bPointCloudOnlyAutoSelectLidarView && GetMonitorView())
        {
            GetMonitorView()->ShowMonitorLidar();
        }
    }
    else
    {
        bPointCloudOnlyModeEnabled = false;
        RestorePointCloudOnlyVisibility();
        for (UVirtualLidarScanComp* LidarComp : Lidars)
        {
            RestoreLidarViewState(LidarComp);
        }
        CurrentViewMode = PreviousViewModeBeforePointCloudOnly;
        if (GetMonitorView())
        {
            if (CurrentViewMode == EVirtualSensorViewMode::Camera) GetMonitorView()->ShowMonitorCamera();
            if (CurrentViewMode == EVirtualSensorViewMode::Lidar) GetMonitorView()->ShowMonitorLidar();
        }
    }

    NotifyDeviceViewModeChanged(CurrentViewMode);
}

void ADeviceSensorCoordinator::ToggleSensorView()
{
    SetViewMode(CurrentViewMode == EVirtualSensorViewMode::Camera ? EVirtualSensorViewMode::Lidar : EVirtualSensorViewMode::Camera);
}

void ADeviceSensorCoordinator::TogglePointCloudOnlyView()
{
    SetPointCloudOnlyMode(!bPointCloudOnlyModeEnabled);
}

void ADeviceSensorCoordinator::StartAllSensors()
{
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) SensorActor->StartSensor();
    }
    for (UVirtualCameraCaptureComp* CameraComp : Cameras)
    {
        if (CameraComp && !Cast<AVirtualSensorActorBase>(CameraComp->GetOwner())) CameraComp->StartCapture();
    }
    for (UVirtualLidarScanComp* LidarComp : Lidars)
    {
        if (LidarComp && !Cast<AVirtualSensorActorBase>(LidarComp->GetOwner())) LidarComp->StartScan();
    }
}

void ADeviceSensorCoordinator::StopAllSensors()
{
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) SensorActor->StopSensor();
    }
    for (UVirtualCameraCaptureComp* CameraComp : Cameras)
    {
        if (CameraComp && !Cast<AVirtualSensorActorBase>(CameraComp->GetOwner())) CameraComp->StopCapture();
    }
    for (UVirtualLidarScanComp* LidarComp : Lidars)
    {
        if (LidarComp && !Cast<AVirtualSensorActorBase>(LidarComp->GetOwner())) LidarComp->StopScan();
    }
}

int32 ADeviceSensorCoordinator::StartAllRealSensorSources()
{
    int32 StartedCount = 0;
    for (URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
    {
        if (RealSensorSourceComp && RealSensorSourceComp->StartSource())
        {
            ++StartedCount;
        }
    }
    return StartedCount;
}

void ADeviceSensorCoordinator::StopAllRealSensorSources()
{
    for (URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
    {
        if (RealSensorSourceComp)
        {
            RealSensorSourceComp->StopSource();
        }
    }
}

bool ADeviceSensorCoordinator::PushSelectedRealSensorSourceOnce(bool bSendTransport)
{
    URealSensorSourceComp* RealSensorSourceComp = GetSelectedRealSensorSource();
    return RealSensorSourceComp && RealSensorSourceComp->PushFrameOnce(bSendTransport);
}

void ADeviceSensorCoordinator::CaptureAllOnce()
{
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) SensorActor->CaptureSensorOnce();
    }
    for (UVirtualCameraCaptureComp* CameraComp : Cameras)
    {
        if (CameraComp && !Cast<AVirtualSensorActorBase>(CameraComp->GetOwner())) CameraComp->CaptureAndSendImage();
    }
    for (UVirtualLidarScanComp* LidarComp : Lidars)
    {
        if (LidarComp && !Cast<AVirtualSensorActorBase>(LidarComp->GetOwner())) LidarComp->ScanAndSend();
    }
}

void ADeviceSensorCoordinator::CaptureSelectedOnce()
{
    if (UVirtualCameraCaptureComp* CameraComp = GetSelectedCamera())
    {
        CameraComp->CaptureAndSendImage();
    }
    if (UVirtualLidarScanComp* LidarComp = GetSelectedLidar())
    {
        LidarComp->ScanAndSend();
    }
}

void ADeviceSensorCoordinator::RefreshSelectedSensorOnce(bool bLidar)
{
    if (bLidar)
    {
        if (UVirtualLidarScanComp* LidarComp = GetSelectedLidar())
        {
            LidarComp->ScanAndSend();
        }
        return;
    }

    if (UVirtualCameraCaptureComp* CameraComp = GetSelectedCamera())
    {
        CameraComp->CaptureAndSendImage();
    }
}

void ADeviceSensorCoordinator::RegisterSensorActor(AVirtualSensorActorBase* SensorActor)
{
    if (!IsValid(SensorActor) || SensorActor->GetWorld()!=GetWorld() || SensorActors.Contains(SensorActor)) return;
    SensorActors.Add(SensorActor);
    SensorActor->OnSensorActivated.RemoveAll(this);
    SensorActor->OnSensorActivated.AddUObject(this,&ThisClass::HandleSensorActivated);
    SensorActor->SetSharedOutputServices(GetDeviceCoordinatorConfig().SharedTransportComp, GetDeviceCoordinatorConfig().SharedRecorderComp, GetDeviceCoordinatorConfig().StreamPublisherComp);
    if(auto* C=SensorActor->FindComponentByClass<UVirtualCameraCaptureComp>())RegisterCamera(C);
    if(auto* L=SensorActor->FindComponentByClass<UVirtualLidarScanComp>())RegisterLidar(L);
    SensorActor->OnEndPlay.AddUniqueDynamic(this,&ThisClass::HandleSensorEndPlay);
    OnSensorInventoryChanged.Broadcast();
}

void ADeviceSensorCoordinator::HandleSensorEndPlay(AActor* Actor,EEndPlayReason::Type)
{UnregisterSensorActor(Cast<AVirtualSensorActorBase>(Actor));}
void ADeviceSensorCoordinator::UnregisterSensorActor(AVirtualSensorActorBase* Sensor)
{
    if(!Sensor)return;
    auto* SelectedCamera=GetSelectedCamera();auto* SelectedLidar=GetSelectedLidar();
    if(auto* L=Sensor->FindComponentByClass<UVirtualLidarScanComp>())RestoreLidarViewState(L);
    const int32 Removed=SensorActors.Remove(Sensor);
    Cameras.RemoveAll([&](const auto& C){return !IsValid(C)||C->GetOwner()==Sensor;});
    Lidars.RemoveAll([&](const auto& L){return !IsValid(L)||L->GetOwner()==Sensor;});
    SelectedCameraIndex=Cameras.IndexOfByKey(SelectedCamera);SelectedLidarIndex=Lidars.IndexOfByKey(SelectedLidar);
    if(SelectedCameraIndex==INDEX_NONE)SelectedCameraIndex=0;if(SelectedLidarIndex==INDEX_NONE)SelectedLidarIndex=0;
    Sensor->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleSensorEndPlay);
    Sensor->OnSensorActivated.RemoveAll(this);
    ApplyWidgetBinding();if(Removed)OnSensorInventoryChanged.Broadcast();
}
bool ADeviceSensorCoordinator::SelectSensorActor(AVirtualSensorActorBase* Sensor)
{
    if(!IsValid(Sensor)||!SensorActors.Contains(Sensor))return false;
    if(auto* C=Sensor->FindComponentByClass<UVirtualCameraCaptureComp>()){const int32 I=Cameras.IndexOfByKey(C);if(I==INDEX_NONE)return false;SelectCameraByIndex(I);SetViewMode(EVirtualSensorViewMode::Camera);}
    else if(auto* L=Sensor->FindComponentByClass<UVirtualLidarScanComp>()){const int32 I=Lidars.IndexOfByKey(L);if(I==INDEX_NONE)return false;SelectLidarByIndex(I);SetViewMode(EVirtualSensorViewMode::Lidar);}else return false;
    return true;
}

void ADeviceSensorCoordinator::SetSelectedLidarPreviewPolicy(int32 InStride, int32 InMaxPoints, bool bInHitOnly)
{
    if (UVirtualLidarScanComp* LidarComp = GetSelectedLidar())
    {
        LidarComp->SetPreviewPolicy(InStride, InMaxPoints, bInHitOnly);
    }
}

void ADeviceSensorCoordinator::AdjustSelectedLidarPreviewBudget(int32 StrideDelta, int32 MaxPointsDelta)
{
    if (UVirtualLidarScanComp* LidarComp = GetSelectedLidar())
    {
        const int32 NextStride = FMath::Clamp(LidarComp->PreviewPointStride + StrideDelta, 1, 100);
        const int32 NextMaxPoints = FMath::Clamp(LidarComp->MaxPreviewPoints + MaxPointsDelta, 0, 1000000);
        LidarComp->SetPreviewPolicy(NextStride, NextMaxPoints, LidarComp->bPointCloudPreviewHitOnly);
    }
}

UVirtualCameraCaptureComp* ADeviceSensorCoordinator::GetSelectedCamera() const
{
    return Cameras.IsValidIndex(SelectedCameraIndex) ? Cameras[SelectedCameraIndex] : nullptr;
}

UVirtualCameraCaptureComp* ADeviceSensorCoordinator::GetCameraByIndex(int32 Index) const
{
	return Cameras.IsValidIndex(Index) ? Cameras[Index].Get() : nullptr;
}

int32 ADeviceSensorCoordinator::FindCameraIndexBySensorId(const FString& SensorId) const
{
	for (int32 Index = 0; Index < Cameras.Num(); ++Index)
	{
		if (Cameras[Index] && Cameras[Index]->SensorId.Equals(SensorId, ESearchCase::CaseSensitive)) return Index;
	}
	return INDEX_NONE;
}

UVirtualLidarScanComp* ADeviceSensorCoordinator::GetSelectedLidar() const
{
    return Lidars.IsValidIndex(SelectedLidarIndex) ? Lidars[SelectedLidarIndex] : nullptr;
}

URealSensorSourceComp* ADeviceSensorCoordinator::GetSelectedRealSensorSource() const
{
    if (UVirtualLidarScanComp* SelectedLidar = GetSelectedLidar())
    {
        for (URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
        {
            if (RealSensorSourceComp && RealSensorSourceComp->TargetLidarComp == SelectedLidar)
            {
                return RealSensorSourceComp;
            }
        }
    }
    return RealSensorSources.IsValidIndex(SelectedRealSensorSourceIndex) ? RealSensorSources[SelectedRealSensorSourceIndex] : nullptr;
}

AVirtualSensorActorBase* ADeviceSensorCoordinator::GetSelectedSensorActor() const
{
    const EVirtualSensorKind TargetKind = CurrentViewMode == EVirtualSensorViewMode::Lidar ||
        CurrentViewMode == EVirtualSensorViewMode::PointCloudOnly
        ? EVirtualSensorKind::Lidar
        : EVirtualSensorKind::Camera;
    return GetSelectedSensorActorByKind(TargetKind);
}

AVirtualSensorActorBase* ADeviceSensorCoordinator::GetSelectedSensorActorByKind(EVirtualSensorKind SensorKind) const
{
    if(SensorKind==EVirtualSensorKind::Camera)if(auto* C=GetSelectedCamera())return Cast<AVirtualSensorActorBase>(C->GetOwner());
    if(SensorKind==EVirtualSensorKind::Lidar)if(auto* L=GetSelectedLidar())return Cast<AVirtualSensorActorBase>(L->GetOwner());
    const int32 TargetIndex = SensorKind == EVirtualSensorKind::Camera ? SelectedCameraIndex : SelectedLidarIndex;
    int32 MatchingIndex = 0;
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (!SensorActor || SensorActor->GetSensorKind() != SensorKind) continue;
        if (MatchingIndex++ == TargetIndex) return SensorActor;
    }
    return nullptr;
}

TArray<AVirtualSensorActorBase*> ADeviceSensorCoordinator::GetSensorActors() const
{
    TArray<AVirtualSensorActorBase*> Result;
    Result.Reserve(SensorActors.Num());
    for (AVirtualSensorActorBase* SensorActor : SensorActors)
    {
        if (SensorActor) Result.Add(SensorActor);
    }
    return Result;
}

TArray<FVirtualSensorSummary> ADeviceSensorCoordinator::GetCameraSummaries() const
{
    TArray<FVirtualSensorSummary> Result;
    for (int32 Index = 0; Index < Cameras.Num(); ++Index)
    {
        FVirtualSensorSummary Summary;
        Summary.Index = Index;
        Summary.SensorType = TEXT("virtual_camera");
        Summary.bValid = Cameras[Index] != nullptr;
        Summary.SensorId = Cameras[Index] ? Cameras[Index]->SensorId : TEXT("");
        Result.Add(Summary);
    }
    return Result;
}

TArray<FVirtualSensorSummary> ADeviceSensorCoordinator::GetLidarSummaries() const
{
    TArray<FVirtualSensorSummary> Result;
    for (int32 Index = 0; Index < Lidars.Num(); ++Index)
    {
        FVirtualSensorSummary Summary;
        Summary.Index = Index;
        Summary.SensorType = TEXT("virtual_lidar");
        Summary.bValid = Lidars[Index] != nullptr;
        Summary.SensorId = Lidars[Index] ? Lidars[Index]->SensorId : TEXT("");
        Result.Add(Summary);
    }
    return Result;
}

TArray<FVirtualSensorSummary> ADeviceSensorCoordinator::GetRealSensorSourceSummaries() const
{
    TArray<FVirtualSensorSummary> Result;
    for (int32 Index = 0; Index < RealSensorSources.Num(); ++Index)
    {
        FVirtualSensorSummary Summary;
        Summary.Index = Index;
        Summary.SensorType = TEXT("real_sensor_source");
        Summary.bValid = RealSensorSources[Index] != nullptr;
        Summary.SensorId = RealSensorSources[Index] ? RealSensorSources[Index]->SourceId : TEXT("");
        Result.Add(Summary);
    }
    return Result;
}

FVirtualSensorHealthSummary ADeviceSensorCoordinator::GetHealthSummary() const
{
    FVirtualSensorHealthSummary Health;
    Health.CameraCount = Cameras.Num();
    Health.LidarCount = Lidars.Num();
    Health.RealSensorSourceCount = RealSensorSources.Num();

    const FDateTime NowUtc = FDateTime::UtcNow();
    auto CountIfStale = [&NowUtc, this](const FVirtualSensorRuntimeStatus& Status) -> bool
    {
        if (Status.FrameId <= 0)
        {
            return true;
        }
        const FTimespan Age = NowUtc - Status.LastUpdateUtc;
        return Age.GetTotalSeconds() > GetDeviceCoordinatorConfig().StaleSensorSeconds;
    };

    for (const UVirtualCameraCaptureComp* CameraComp : Cameras)
    {
        if (!CameraComp || CountIfStale(CameraComp->GetRuntimeStatus()))
        {
            ++Health.StaleSensorCount;
        }
    }

    for (const UVirtualLidarScanComp* LidarComp : Lidars)
    {
        if (!LidarComp || CountIfStale(LidarComp->GetRuntimeStatus()))
        {
            ++Health.StaleSensorCount;
        }
    }

    for (const URealSensorSourceComp* RealSensorSourceComp : RealSensorSources)
    {
        if (!RealSensorSourceComp)
        {
            ++Health.ErrorRealSensorSourceCount;
            continue;
        }

        if (RealSensorSourceComp->GetConnectionState() == ERealSensorSourceConnectionState::Running)
        {
            ++Health.RunningRealSensorSourceCount;
        }
        else if (RealSensorSourceComp->GetConnectionState() == ERealSensorSourceConnectionState::Error)
        {
            ++Health.ErrorRealSensorSourceCount;
        }

        if (RealSensorSourceComp->RequiresExternalDeploymentEvidence())
        {
            ++Health.ExternalEvidenceRequiredRealSensorSourceCount;
        }
    }

    Health.bHealthy = Health.StaleSensorCount == 0 && Health.ErrorRealSensorSourceCount == 0;
    Health.Summary = FString::Printf(TEXT("Cameras=%d Lidars=%d RealSources=%d RunningRealSources=%d ErrorRealSources=%d ExternalEvidenceRequired=%d Stale=%d Healthy=%s PointCloudOnly=%s"),
        Health.CameraCount,
        Health.LidarCount,
        Health.RealSensorSourceCount,
        Health.RunningRealSensorSourceCount,
        Health.ErrorRealSensorSourceCount,
        Health.ExternalEvidenceRequiredRealSensorSourceCount,
        Health.StaleSensorCount,
        Health.bHealthy ? TEXT("true") : TEXT("false"),
        bPointCloudOnlyModeEnabled ? TEXT("true") : TEXT("false"));
    return Health;
}

void ADeviceSensorCoordinator::ApplyWidgetBinding()
{
    if (!GetMonitorView()) return;
    GetMonitorView()->BindMonitorCamera(GetSelectedCamera());
    GetMonitorView()->BindMonitorLidar(GetSelectedLidar());
    GetMonitorView()->BindMonitorExternalSource(GetSelectedRealSensorSource());
    if (CurrentViewMode == EVirtualSensorViewMode::PointCloudOnly)
    {
        GetMonitorView()->ShowMonitorLidar();
    }
    else
    {
        SetViewMode(CurrentViewMode);
    }
}

void ADeviceSensorCoordinator::RunSynchronizedCapture()
{
    CaptureAllOnce();
}

void ADeviceSensorCoordinator::AssignSharedServicesIfPossible(UActorComponent* SensorComp)
{
    if (!SensorComp)
    {
        return;
    }

    if (UVirtualCameraCaptureComp* CameraComp = Cast<UVirtualCameraCaptureComp>(SensorComp))
    {
        if (GetDeviceCoordinatorConfig().SharedTransportComp)
        {
            CameraComp->SetTransportComponent(GetDeviceCoordinatorConfig().SharedTransportComp);
        }
        if (GetDeviceCoordinatorConfig().SharedRecorderComp)
        {
            CameraComp->SetRecorderComponent(GetDeviceCoordinatorConfig().SharedRecorderComp);
        }
    }
    else if (UVirtualLidarScanComp* LidarComp = Cast<UVirtualLidarScanComp>(SensorComp))
    {
        if (GetDeviceCoordinatorConfig().SharedTransportComp)
        {
            LidarComp->SetTransportComponent(GetDeviceCoordinatorConfig().SharedTransportComp);
        }
        if (GetDeviceCoordinatorConfig().SharedRecorderComp)
        {
            LidarComp->SetRecorderComponent(GetDeviceCoordinatorConfig().SharedRecorderComp);
        }
    }
}

void ADeviceSensorCoordinator::ApplyPointCloudOnlyVisibility()
{
    RestorePointCloudOnlyVisibility();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    APlayerController* Controller = GetMonitorView() ? GetMonitorView()->GetMonitorViewController() : nullptr;
    if (!Controller) Controller = World->GetFirstPlayerController();
    if (!Controller || Controller->GetWorld() != World) return;
    PointCloudOnlyViewController = Controller;
    TSet<TWeakObjectPtr<UPrimitiveComponent>> AlreadyHidden;
    AlreadyHidden.Reserve(Controller->HiddenPrimitiveComponents.Num());
    for (const auto& Component : Controller->HiddenPrimitiveComponents) AlreadyHidden.Add(Component);

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || ShouldKeepActorVisibleInPointCloudOnly(Actor))
        {
            continue;
        }

        TArray<UPrimitiveComponent*> PrimitiveComponents;
        Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
        for (UPrimitiveComponent* PrimitiveComp : PrimitiveComponents)
        {
            if (!PrimitiveComp)
            {
                continue;
            }

            const TWeakObjectPtr<UPrimitiveComponent> Component(PrimitiveComp);
            // Preserve somebody else's pre-existing hidden entry. SceneCaptures do
            // not consume this PlayerController list, so GPU depth remains unchanged.
            if (!AlreadyHidden.Contains(Component))
            {
                AlreadyHidden.Add(Component);
                Controller->HiddenPrimitiveComponents.Add(Component);
                OwnedViewHiddenComponents.Add(Component);
            }
        }
    }
}

void ADeviceSensorCoordinator::RestorePointCloudOnlyVisibility()
{
    if (APlayerController* Controller = PointCloudOnlyViewController.Get())
    {
        // Remove one entry we inserted, not every matching/third-party entry.
        // Compact once: repeated RemoveSingle would be quadratic in a large scene.
        TSet<TWeakObjectPtr<UPrimitiveComponent>> RemainingOwned;
        RemainingOwned.Reserve(OwnedViewHiddenComponents.Num());
        for (const auto& Component : OwnedViewHiddenComponents) RemainingOwned.Add(Component);
        int32 WriteIndex = 0;
        for (const auto& Component : Controller->HiddenPrimitiveComponents)
        {
            if (RemainingOwned.Remove(Component) > 0) continue;
            Controller->HiddenPrimitiveComponents[WriteIndex++] = Component;
        }
        Controller->HiddenPrimitiveComponents.SetNum(WriteIndex, false);
    }
    OwnedViewHiddenComponents.Reset();
    PointCloudOnlyViewController.Reset();
    // Retained reflected legacy field for compatibility, no longer populated.
    HiddenComponentStates.Reset();
}

bool ADeviceSensorCoordinator::ShouldKeepActorVisibleInPointCloudOnly(const AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    if (Actor == this)
    {
        return true;
    }

    if (const UVirtualLidarScanComp* SelectedLidar = GetSelectedLidar())
    {
        if (Actor == SelectedLidar->GetOwner())
        {
            return true;
        }
    }

    if (const UVirtualCameraCaptureComp* SelectedCamera = GetSelectedCamera())
    {
        if (Actor == SelectedCamera->GetOwner())
        {
            return true;
        }
    }

    for (const FName& Tag : GetDeviceCoordinatorConfig().PointCloudOnlyKeepActorTags)
    {
        if (!Tag.IsNone() && Actor->ActorHasTag(Tag))
        {
            return true;
        }
    }

    return false;
}

int32 ADeviceSensorCoordinator::SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend Backend)
{
    int32 Applied=0;
    for(auto* Actor:GetSensorActors())if(auto* Lidar=Cast<AVirtualLidarSensorActor>(Actor))
    {FString Reason;if(Lidar->SetLidarAcquisitionBackend(Backend,Reason))++Applied;}
    return Applied;
}

bool ADeviceSensorCoordinator::SetPcdBandwidthOptions(const FString& Id,const FVirtualPcdSendOptions& Options,bool All,FString& Error)
{
    if(!GetDeviceCoordinatorConfig().StreamPublisherComp){Error=TEXT("PCD publisher is unavailable");return false;}
    return GetDeviceCoordinatorConfig().StreamPublisherComp->SetPcdSendOptions(Id,Options,All,Error);
}

void ADeviceSensorCoordinator::BindDeviceMonitorView(UObject* ViewOwner)
{
    MonitorOwner = IsValid(ViewOwner) && Cast<IVirtualSensorMonitorView>(ViewOwner) ? ViewOwner : nullptr;
    ApplyWidgetBinding();
}

IVirtualSensorMonitorView* ADeviceSensorCoordinator::GetMonitorView() const
{
    return Cast<IVirtualSensorMonitorView>(MonitorOwner.Get());
}
