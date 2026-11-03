#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Contracts/DeviceIdentityPolicy.h"
#include "Device/Core/VirtualSensorHitchDiagnostics.h"

#include "Components/ArrowComponent.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Sensor/VirtualLidarAnalysisComp.h"
#include "Device/Sensor/VirtualLidarExportComp.h"
#include "Device/Sensor/VirtualLidarGpuDepthProjectionComponent.h"
#include "Device/Sensor/VirtualLidarVisualizationComp.h"
#include "Device/Sensor/VirtualSensorOutputComp.h"
#include "Device/Core/VirtualSensorSchedulerSubsystem.h"
#include "Device/Sensor/VirtualSensorEditableTypes.h"

AVirtualLidarSensorActor::AVirtualLidarSensorActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ScanComp = CreateDefaultSubobject<UVirtualLidarScanComp>(TEXT("LidarScanComponent"));
    RootComponent = ScanComp;
    AnalysisComp = CreateDefaultSubobject<UVirtualLidarAnalysisComp>(TEXT("LidarAnalysisComponent"));
    VisualizationComp = CreateDefaultSubobject<UVirtualLidarVisualizationComp>(TEXT("LidarVisualizationComponent"));
    ExportComp = CreateDefaultSubobject<UVirtualLidarExportComp>(TEXT("LidarExportComponent"));
    GpuDepthProjectionComponent = CreateDefaultSubobject<UVirtualLidarGpuDepthProjectionComponent>(TEXT("LidarGpuDepthProjectionComponent"));
    GpuDepthProjectionComponent->SetupAttachment(ScanComp);
    ScanComp->BindGpuDepthProjectionComponent(GpuDepthProjectionComponent);

    EditorForwardArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("EditorForwardArrowComp"));
    EditorForwardArrowComp->SetupAttachment(RootComponent);
    EditorForwardArrowComp->ArrowSize = 2.0f;
    EditorForwardArrowComp->ArrowLength = 250.0f;
    EditorForwardArrowComp->bIsScreenSizeScaled = true;
}

void AVirtualLidarSensorActor::BeginPlay()
{
    Super::BeginPlay();
    if (ScanComp)
    {
        ScanComp->SetTransportComponent(nullptr);
        ScanComp->SetRecorderComponent(nullptr);
        ScanComp->OutputMode = EVirtualLidarOutputMode::None;
        ScanComp->OnFrameAcquired.AddDynamic(this, &AVirtualLidarSensorActor::HandleLidarFrameAcquired);
        ScanComp->OnDerivedFrameCompletedNative.AddUObject(this,&AVirtualLidarSensorActor::HandleDerivedLidarFrame);
        if (AnalysisComp) AnalysisComp->BindScanComponent(ScanComp);
        if (VisualizationComp) VisualizationComp->BindScanComponent(ScanComp);
        if (ExportComp) ExportComp->BindScanComponent(ScanComp);
    }
}

FString AVirtualLidarSensorActor::GetSensorId() const { return ScanComp ? ScanComp->SensorId : FString(); }
EVirtualSensorKind AVirtualLidarSensorActor::GetSensorKind() const { return EVirtualSensorKind::Lidar; }
bool AVirtualLidarSensorActor::IsSensorRunning() const { return ScanComp && ScanComp->IsScanRunning(); }
void AVirtualLidarSensorActor::StartSensor() { ++SensorRunControlRevision; if (ScanComp) ScanComp->StartScan(); }
void AVirtualLidarSensorActor::StopSensor() { ++SensorRunControlRevision; if (ScanComp) ScanComp->StopScan(); }
void AVirtualLidarSensorActor::CaptureSensorOnce() { if (ScanComp) ScanComp->ScanAndSend(); }
UTexture* AVirtualLidarSensorActor::GetSensorPreviewTexture() const { return ScanComp ? ScanComp->GetLidarViewTexture() : nullptr; }
FVirtualSensorRuntimeStatus AVirtualLidarSensorActor::GetSensorRuntimeStatus() const { return ScanComp ? ScanComp->GetRuntimeStatus() : FVirtualSensorRuntimeStatus(); }

bool AVirtualLidarSensorActor::SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport)
{
    if (!ScanComp || Frame.SensorKind != EVirtualSensorKind::Lidar || !Frame.PointSnapshot.IsValid()) return false;
    PendingExternalSendTransport = bSendTransport;
    ScanComp->InjectPointCloudFrame(*Frame.PointSnapshot, false);
    return true;
}

bool AVirtualLidarSensorActor::ReadEditableState(FVirtualSensorEditableState& OutState) const
{
    if (!ScanComp) return false;
    OutState.TargetKind = EVirtualSensorTargetKind::Lidar;
    OutState.OriginalSensorId = ScanComp->SensorId;
    OutState.SensorId = ScanComp->SensorId;
    OutState.ActorTransform = GetActorTransform();
    OutState.SimulationQuality = ScanComp->SimulationQuality;
    OutState.LidarProfile = ScanComp->DeviceProfile;
    OutState.LidarScanInterval = ScanComp->ScanInterval;
    OutState.LidarMaxDistance = ScanComp->MaxDistance;
    OutState.LidarHorizontalSamples = ScanComp->HorizontalSamples;
    OutState.LidarVerticalChannels = ScanComp->VerticalChannels;
    OutState.LidarHorizontalFov = ScanComp->HorizontalFov;
    OutState.LidarMinVerticalAngle = ScanComp->MinVerticalAngle;
    OutState.LidarMaxVerticalAngle = ScanComp->MaxVerticalAngle;
    OutState.ServerPayloadStride = ScanComp->ServerPayloadStride;
    OutState.CompatibilityOutputPolicy=ScanComp->CompatibilityOutputPolicy;
    OutState.MaxServerPayloadPoints = ScanComp->MaxServerPayloadPoints;
    OutState.bIncludeMissPointsInServerPayload = ScanComp->bIncludeMissPointsInServerPayload;
    OutState.PreviewPointStride = ScanComp->PreviewPointStride;
    OutState.MaxPreviewPoints = ScanComp->MaxPreviewPoints;
    OutState.bPreviewHitOnly = ScanComp->bPointCloudPreviewHitOnly;
    OutState.bUseMultiHit = ScanComp->bUseMultiHit;
    OutState.bLidarPointTime = ScanComp->bUseRollingPointTimestamps;
    OutState.bLidarMotionDistortion = ScanComp->bEnableMotionDistortion;
    OutState.bLidarMaterialResponse = ScanComp->bEnableMaterialResponse;
    OutState.MaxHitsPerRay = ScanComp->MaxHitsPerRay;
    OutState.bExportCsvOnScan = ScanComp->bExportCsvOnScan;
    OutState.bExportJsonLinesOnScan = ScanComp->bExportJsonLinesOnScan;
    OutState.bExportPcdOnScan = ScanComp->bExportPcdOnScan;
    return true;
}

#if WITH_EDITOR
void AVirtualLidarSensorActor::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    FVirtualSensorHitchScope Scope(EVirtualSensorHitchSection::EditorProperty);
    Super::PostEditChangeProperty(Event);
}
#endif
bool AVirtualLidarSensorActor::ApplyEditableState(const FVirtualSensorEditableState& RequestedState, FString& OutError)
{
    FVirtualSensorEditableState State=RequestedState;
    State.SensorId=DeviceIdentityPolicy::Canonical(State.SensorId);
    State.OriginalSensorId=DeviceIdentityPolicy::Canonical(State.OriginalSensorId);
    FVirtualSensorHitchScope HitchScope(EVirtualSensorHitchSection::SensorApply);
    if (!ValidateEditableState(State, OutError))
    {
        return false;
    }
    FVirtualSensorEditableState Current;ReadEditableState(Current);
    Current.CompatibilityOutputPolicy=State.CompatibilityOutputPolicy;
    Current.PersistentActorTag=State.PersistentActorTag;Current.OriginalSensorId=State.OriginalSensorId;
    if(FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Current,&State,PPF_None))
    {
        if(ScanComp->CompatibilityOutputPolicy!=State.CompatibilityOutputPolicy){ScanComp->SetCompatibilityOutputPolicy(State.CompatibilityOutputPolicy);++ConfigurationRevision;}
        return true;
    }
    const bool bWasRunning=bInteractiveManipulationActive?bWasRunningBeforeInteraction:ScanComp->IsScanRunning();
    TGuardValue<bool> Transaction(bSettingsTransaction,true);if(bInteractiveManipulationActive)EndInteractiveManipulation();
    SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
    if(ScanComp->IsScanRunning())ScanComp->StopScan();
    const auto PreservedBackend=ScanComp->AcquisitionBackend;
    ScanComp->ApplyDeviceProfile(State.LidarProfile);
    ScanComp->ApplySimulationQuality(State.SimulationQuality);
    ScanComp->AcquisitionBackend=PreservedBackend;
    ScanComp->SensorId = State.SensorId;
    ScanComp->ScanInterval = State.LidarScanInterval;
    ScanComp->MaxDistance = State.LidarMaxDistance;
    ScanComp->HorizontalSamples = State.LidarHorizontalSamples;
    ScanComp->VerticalChannels = State.LidarVerticalChannels;
    ScanComp->HorizontalFov = State.LidarHorizontalFov;
    ScanComp->MinVerticalAngle = State.LidarMinVerticalAngle;
    ScanComp->MaxVerticalAngle = State.LidarMaxVerticalAngle;
    ScanComp->SetServerPayloadPolicy(State.ServerPayloadStride, State.MaxServerPayloadPoints, State.bIncludeMissPointsInServerPayload);
    ScanComp->SetCompatibilityOutputPolicy(State.CompatibilityOutputPolicy);
    ScanComp->SetPreviewPolicy(State.PreviewPointStride, State.MaxPreviewPoints, State.bPreviewHitOnly);
    ScanComp->bUseMultiHit = State.bUseMultiHit;
    ScanComp->bUseRollingPointTimestamps = State.bLidarPointTime;
    ScanComp->bEnableMotionDistortion = State.bLidarMotionDistortion;
    ScanComp->bEnableMaterialResponse = State.bLidarMaterialResponse;
    ScanComp->InvalidateSurfaceResponseCache();
    ScanComp->MaxHitsPerRay = State.MaxHitsPerRay;
    ScanComp->bExportCsvOnScan = State.bExportCsvOnScan;
    ScanComp->bExportJsonLinesOnScan = State.bExportJsonLinesOnScan;
    ScanComp->bExportPcdOnScan = State.bExportPcdOnScan;
    if(bWasRunning) ScanComp->StartScan();
    ++ConfigurationRevision;
    return true;
}

void AVirtualLidarSensorActor::HandleLidarFrameAcquired(int64 FrameId)
{
    const TSharedPtr<const FVirtualLidarFrameSnapshot, ESPMode::ThreadSafe> Snapshot = ScanComp
        ? ScanComp->GetLastFrameSnapshot()
        : nullptr;
    if (!Snapshot.IsValid() || FrameId != Snapshot->FrameId)
    {
        return;
    }

    if (OutputComp)
    {
        FVirtualSensorFrameEnvelope AcquiredFrame;
        AcquiredFrame.SensorId = ScanComp->SensorId;
        AcquiredFrame.SensorKind = EVirtualSensorKind::Lidar;
        AcquiredFrame.FrameId = Snapshot->FrameId;
		AcquiredFrame.RequestedAcquisitionIntervalSeconds = ScanComp->ScanInterval;
		AcquiredFrame.SlabContext = Snapshot->SlabContext;
        AcquiredFrame.TimestampUtc = Snapshot->GetAcquisitionStartUtc();
        if(AcquiredFrame.TimestampUtc.GetTicks()<=0)AcquiredFrame.TimestampUtc=FDateTime::UtcNow();
        AcquiredFrame.SchemaVersion = TEXT("virtual-lidar.v2");
        AcquiredFrame.PointSnapshot = Snapshot->Points;
        AcquiredFrame.LidarFrameSnapshot = Snapshot;
        AcquiredFrame.bSendTransport = false;
        AcquiredFrame.bRecord = false;
        OutputComp->RouteAcquiredFrame(AcquiredFrame);
    }

    if (AnalysisComp)
    {
        AnalysisComp->ApplyPrecomputedStatistics(ScanComp->GetLastHitPointCount(), ScanComp->GetLastSemanticCounts());
    }
    if (VisualizationComp)
    {
        UVirtualSensorSchedulerSubsystem* Scheduler = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>() : nullptr;
        // Measurement and transport stay at the configured sensor frequency.
        // Rendering the same 32k-point snapshot at 20 Hz is optional derived
        // work, so keep the selected monitor responsive at 10 Hz without
        // delaying the next ML-X acquisition.
        if (!Scheduler || Scheduler->ConsumeLidarPreviewRefresh(ScanComp, 10.0f))
        {
            VisualizationComp->RefreshLatestFrame();
        }
    }
}
bool AVirtualLidarSensorActor::ApplyDatasetState(const FVirtualSensorEditableState& State,FString& Error)
{
    if(IsSensorRunning()||!ValidateEditableState(State,Error)||!CanEditSensorPlacement(Error))return false;
    FVirtualSensorEditableState Allowed;ReadEditableState(Allowed);
    Allowed.ActorTransform=State.ActorTransform;Allowed.bLidarPointTime=State.bLidarPointTime;Allowed.bLidarMotionDistortion=State.bLidarMotionDistortion;Allowed.bLidarMaterialResponse=State.bLidarMaterialResponse;
    Allowed.CompatibilityOutputPolicy=State.CompatibilityOutputPolicy;Allowed.bExportCsvOnScan=State.bExportCsvOnScan;Allowed.bExportJsonLinesOnScan=State.bExportJsonLinesOnScan;Allowed.bExportPcdOnScan=State.bExportPcdOnScan;
    if(!FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Allowed,&State,PPF_None)){Error=TEXT("데이터셋 중 센서 규격이 변경되었습니다");return false;}
    SetActorTransform(State.ActorTransform,false,nullptr,ETeleportType::TeleportPhysics);
    ScanComp->bUseRollingPointTimestamps=State.bLidarPointTime;ScanComp->bEnableMotionDistortion=State.bLidarMotionDistortion;ScanComp->bEnableMaterialResponse=State.bLidarMaterialResponse;
    ScanComp->SetCompatibilityOutputPolicy(State.CompatibilityOutputPolicy);ScanComp->bExportCsvOnScan=State.bExportCsvOnScan;ScanComp->bExportJsonLinesOnScan=State.bExportJsonLinesOnScan;ScanComp->bExportPcdOnScan=State.bExportPcdOnScan;
    ++ConfigurationRevision;return true;
}

bool AVirtualLidarSensorActor::BeginInteractiveManipulation(const FVirtualSensorInteractionRequest& Request)
{
	if (!ScanComp || bInteractiveManipulationActive) return ScanComp != nullptr;
    if(!Super::BeginInteractiveManipulation(Request))return false;
	bWasRunningBeforeInteraction = ScanComp->IsScanRunning();
	SavedInteractionHorizontalSamples = ScanComp->HorizontalSamples;
	SavedInteractionVerticalChannels = ScanComp->VerticalChannels;
	SavedInteractionInterval = ScanComp->ScanInterval;
	ScanComp->StopScan();
	ScanComp->HorizontalSamples = Request.LidarHorizontalSamples;
	ScanComp->VerticalChannels = Request.LidarVerticalChannels;
	ScanComp->ScanInterval = Request.LidarPreviewInterval;
	ScanComp->SetInteractivePreviewMode(true, Request.bSuppressDerivedOutput);
	if (bWasRunningBeforeInteraction) ScanComp->StartScan();
	return true;
}

void AVirtualLidarSensorActor::EndInteractiveManipulation()
{
	if (!ScanComp || !bInteractiveManipulationActive) return;
	ScanComp->StopScan();
	ScanComp->HorizontalSamples = SavedInteractionHorizontalSamples;
	ScanComp->VerticalChannels = SavedInteractionVerticalChannels;
	ScanComp->ScanInterval = SavedInteractionInterval;
	ScanComp->SetInteractivePreviewMode(false);
	if (bWasRunningBeforeInteraction&&!bSettingsTransaction)
	{
		ScanComp->StartScan();
		ScanComp->RequestImmediateScheduledScan();
	}
	Super::EndInteractiveManipulation();
}

bool AVirtualLidarSensorActor::ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError)
{
    if(!CanEditSensorConfiguration(OutError))return false;
    if (!ScanComp || RequestedState.TargetKind != EVirtualSensorTargetKind::Lidar)
    {
        OutError = TEXT("선택한 LiDAR 센서를 사용할 수 없습니다.");
        return false;
    }
    const bool bWasRunning = bInteractiveManipulationActive?bWasRunningBeforeInteraction:ScanComp->IsScanRunning();
    TGuardValue<bool> Transaction(bSettingsTransaction,true);if(bInteractiveManipulationActive)EndInteractiveManipulation();
    if(ScanComp->IsScanRunning())ScanComp->StopScan();
    const auto PreservedBackend=ScanComp->AcquisitionBackend;
    ScanComp->ApplyDeviceProfile(RequestedState.LidarProfile);
    ScanComp->ApplySimulationQuality(RequestedState.SimulationQuality);
    ScanComp->AcquisitionBackend=PreservedBackend;
    if (bWasRunning) ScanComp->StartScan();
    ++ConfigurationRevision;
    return ReadEditableState(OutAppliedState);
}

bool AVirtualLidarSensorActor::ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const
{
    if (State.bLidarMotionDistortion && !State.bLidarPointTime)
    { OutError=TEXT("움직임 왜곡에는 점별 시간차가 필요합니다."); return false; }
    if(!CanEditSensorConfiguration(OutError))return false;
    if((State.SensorId!=GetSensorId()||!State.ActorTransform.Equals(GetActorTransform()))&&!CanEditSensorPlacement(OutError))return false;
    if (!ScanComp || State.TargetKind != EVirtualSensorTargetKind::Lidar)
    {
        OutError = TEXT("선택한 LiDAR 센서를 사용할 수 없습니다.");
        return false;
    }
    if (State.SensorId.TrimStartAndEnd().IsEmpty() || State.ActorTransform.ContainsNaN())
    {
        OutError = TEXT("SensorId 또는 Transform 값이 올바르지 않습니다.");
        return false;
    }
    if (!FMath::IsFinite(State.LidarScanInterval) || !FMath::IsFinite(State.LidarMaxDistance) ||
        !FMath::IsFinite(State.LidarHorizontalFov) || !FMath::IsFinite(State.LidarMinVerticalAngle) || !FMath::IsFinite(State.LidarMaxVerticalAngle) ||
        State.LidarScanInterval < 0.033f || State.LidarScanInterval > 60.0f ||
        State.LidarMaxDistance < 10.0f || State.LidarMaxDistance > 20000.0f ||
        State.LidarHorizontalSamples < 1 || State.LidarHorizontalSamples > 1440 ||
        State.LidarVerticalChannels < 1 || State.LidarVerticalChannels > 256 ||
        State.LidarHorizontalFov < 1.0f || State.LidarHorizontalFov > 360.0f ||
        State.LidarMinVerticalAngle < -90.0f || State.LidarMaxVerticalAngle > 90.0f ||
        State.LidarMinVerticalAngle >= State.LidarMaxVerticalAngle ||
        (State.CompatibilityOutputPolicy!=EVirtualLidarCompatibilityOutputPolicy::LegacyAlways&&State.CompatibilityOutputPolicy!=EVirtualLidarCompatibilityOutputPolicy::WhenRequested) ||
        State.ServerPayloadStride < 1 || State.ServerPayloadStride > 100 ||
        State.MaxServerPayloadPoints < 0 || State.MaxServerPayloadPoints > 1000000 ||
        State.PreviewPointStride < 1 || State.PreviewPointStride > 100 ||
        State.MaxPreviewPoints < 0 || State.MaxPreviewPoints > 1000000 ||
        State.MaxHitsPerRay < 1 || State.MaxHitsPerRay > 16)
    {
        OutError = TEXT("LiDAR 설정이 지원 범위를 벗어났습니다.");
        return false;
    }
    return true;
}

void AVirtualLidarSensorActor::HandleLidarFrame(const FString& JsonPayload, UTexture2D* ViewTexture)
{
    if(!ScanComp)return;const auto& Frame=ScanComp->GetLastCompletedDerivedFrame();
    if(Frame.HasJsonPayload()&&*Frame.JsonPayload==JsonPayload)HandleDerivedLidarFrame(Frame);
}

bool AVirtualLidarSensorActor::ReadCollectionState(FVirtualSensorEditableState& State,bool& Preview) const
{
    if(!ReadEditableState(State))return false;
    Preview=bInteractiveManipulationActive?bWasRunningBeforeInteraction:IsSensorRunning();
    if(bInteractiveManipulationActive){State.LidarHorizontalSamples=SavedInteractionHorizontalSamples;State.LidarVerticalChannels=SavedInteractionVerticalChannels;State.LidarScanInterval=SavedInteractionInterval;}
    return true;
}
void AVirtualLidarSensorActor::HandleDerivedLidarFrame(const FVirtualSensorFrameEnvelope& Captured)
{
    if(!OutputComp||!Captured.HasJsonPayload())return;
    auto Frame=Captured;Frame.bSendTransport=PendingExternalSendTransport.IsSet()?PendingExternalSendTransport.GetValue():Captured.bSendTransport;
    PendingExternalSendTransport.Reset();OutputComp->RouteFrame(Frame);
}

bool AVirtualLidarSensorActor::SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend Backend,FString& OutReason)
{
    if(!ScanComp||uint8(Backend)>uint8(EVirtualLidarAcquisitionBackend::ReplayOrExternal)){OutReason=TEXT("Invalid acquisition backend");return false;}
    if(!CanEditSensorConfiguration(OutReason))return false;
    if(ScanComp->AcquisitionBackend==Backend){OutReason.Reset();return true;}
    const bool Running=ScanComp->IsScanRunning();ScanComp->StopScan();ScanComp->AcquisitionBackend=Backend;++ConfigurationRevision;
    if(Running)ScanComp->StartScan();OutReason.Reset();return true;
}
