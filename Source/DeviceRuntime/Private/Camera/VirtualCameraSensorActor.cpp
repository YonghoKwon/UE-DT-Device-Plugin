// Fill out your copyright notice in the Description page of Project Settings.

#include "Device/Camera/VirtualCameraSensorActor.h"

#include "Components/DrawFrustumComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Device/Camera/VirtualCameraCaptureComp.h"
#include "Device/Core/VirtualSensorFileSaveSubsystem.h"
#include "Device/Sensor/VirtualSensorOutputComp.h"
#include "Device/Sensor/VirtualSensorEditableTypes.h"

AVirtualCameraSensorActor::AVirtualCameraSensorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CaptureComp = CreateDefaultSubobject<UVirtualCameraCaptureComp>(TEXT("CameraCaptureComponent"));
	RootComponent = CaptureComp;

	EditorFrustumComp = CreateDefaultSubobject<UDrawFrustumComponent>(TEXT("EditorFrustumComp"));
	EditorFrustumComp->SetupAttachment(RootComponent);
	EditorFrustumComp->FrustumAngle = 87.0f;
	EditorFrustumComp->FrustumStartDist = 20.0f;
	EditorFrustumComp->FrustumEndDist = 600.0f;
	EditorFrustumComp->FrustumAspectRatio = 16.0f / 9.0f;
	EditorFrustumComp->SetHiddenInGame(true);
}

void AVirtualCameraSensorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EditorFrustumComp && CaptureComp)
	{
		EditorFrustumComp->FrustumAngle = CaptureComp->FOVAngle;
		EditorFrustumComp->FrustumEndDist = CaptureComp->GetDeviceSpec().TypicalRangeCm > 0.0f
		                                        ? CaptureComp->GetDeviceSpec().TypicalRangeCm
		                                        : 600.0f;
		EditorFrustumComp->FrustumAspectRatio = CaptureComp->CaptureResolution.Y > 0
		                                            ? static_cast<float>(CaptureComp->CaptureResolution.X) / static_cast<float>(CaptureComp->CaptureResolution.Y)
		                                            : 16.0f / 9.0f;
	}
}

void AVirtualCameraSensorActor::BeginPlay()
{
	Super::BeginPlay();
	if (EditorFrustumComp)
	{
		EditorFrustumComp->SetHiddenInGame(true);
	}
	if (CaptureComp)
	{
		CaptureComp->SetTransportComponent(nullptr);
		CaptureComp->SetRecorderComponent(nullptr);
		CaptureComp->OutputMode = EVirtualCameraOutputMode::None;
		CaptureComp->OnFrameCaptured.AddDynamic(this, &AVirtualCameraSensorActor::HandleCameraFrame);
	}
}

FString AVirtualCameraSensorActor::GetSensorId() const
{
	return CaptureComp ? CaptureComp->SensorId : FString();
}

EVirtualSensorKind AVirtualCameraSensorActor::GetSensorKind() const
{
	return EVirtualSensorKind::Camera;
}

bool AVirtualCameraSensorActor::IsSensorRunning() const
{
	return CaptureComp && CaptureComp->IsCaptureRunning();
}

void AVirtualCameraSensorActor::StartSensor()
{
	++SensorRunControlRevision;
	if (CaptureComp)
		CaptureComp->StartCapture();
}

void AVirtualCameraSensorActor::StopSensor()
{
	++SensorRunControlRevision;
	if (CaptureComp)
		CaptureComp->StopCapture();
}

void AVirtualCameraSensorActor::CaptureSensorOnce()
{
	if (CaptureComp)
		CaptureComp->CaptureAndSendImage();
}

UTexture* AVirtualCameraSensorActor::GetSensorPreviewTexture() const
{
	return CaptureComp ? CaptureComp->GetCameraRenderTarget() : nullptr;
}

FVirtualSensorRuntimeStatus AVirtualCameraSensorActor::GetSensorRuntimeStatus() const
{
	return CaptureComp ? CaptureComp->GetRuntimeStatus() : FVirtualSensorRuntimeStatus();
}

bool AVirtualCameraSensorActor::SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport)
{
	if (!CaptureComp || Frame.SensorKind != EVirtualSensorKind::Camera || !Frame.HasJsonPayload())
		return false;
	PendingExternalSendTransport = bSendTransport;
	const bool bAccepted = CaptureComp->InjectExternalJsonPayload(*Frame.JsonPayload, false);
	if (!bAccepted)
	{
		PendingExternalSendTransport.Reset();
	}
	return bAccepted;
}

bool AVirtualCameraSensorActor::ReadEditableState(FVirtualSensorEditableState& OutState) const
{
	if (!CaptureComp)
		return false;
	OutState.TargetKind = EVirtualSensorTargetKind::Camera;
	OutState.OriginalSensorId = CaptureComp->SensorId;
	OutState.SensorId = CaptureComp->SensorId;
	OutState.ActorTransform = GetActorTransform();
	OutState.SimulationQuality = CaptureComp->SimulationQuality;
	OutState.CameraProfile = CaptureComp->DeviceProfile;
	OutState.CameraResolution = CaptureComp->CaptureResolution;
	OutState.CameraCaptureInterval = CaptureComp->CaptureInterval;
	OutState.CameraFov = CaptureComp->FOVAngle;
	OutState.CameraJpegQuality = CaptureComp->JpegQuality;
	OutState.CameraCaptureMode = CaptureComp->CaptureMode;
	OutState.bCameraGroundTruth = CaptureComp->bGroundTruthEnabled;
	return true;
}

bool AVirtualCameraSensorActor::ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError)
{
	if (!ValidateEditableState(State, OutError))
	{
		return false;
	}
	const bool bWasRunning = bInteractiveManipulationActive ? bWasRunningBeforeInteraction : CaptureComp->IsCaptureRunning();
	TGuardValue<bool> Transaction(bSettingsTransaction, true);
	if (bInteractiveManipulationActive)
		EndInteractiveManipulation();
	SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (CaptureComp->IsCaptureRunning())
		CaptureComp->StopCapture();
	CaptureComp->ApplyDeviceProfile(State.CameraProfile);
	CaptureComp->ApplySimulationQuality(State.SimulationQuality);
	CaptureComp->SensorId = State.SensorId;
	CaptureComp->CaptureResolution = State.CameraResolution;
	CaptureComp->CaptureInterval = State.CameraCaptureInterval;
	CaptureComp->FOVAngle = State.CameraFov;
	CaptureComp->JpegQuality = State.CameraJpegQuality;
	CaptureComp->CaptureMode = State.CameraCaptureMode;
	CaptureComp->SetGroundTruthEnabled(State.bCameraGroundTruth);
	if (bWasRunning)
		CaptureComp->StartCapture();
	++ConfigurationRevision;
	return true;
}

bool AVirtualCameraSensorActor::ReadCollectionState(FVirtualSensorEditableState& State, bool& Preview) const
{
	if (!ReadEditableState(State))
		return false;
	Preview = bInteractiveManipulationActive ? bWasRunningBeforeInteraction : IsSensorRunning();
	if (bInteractiveManipulationActive)
	{
		State.CameraResolution = SavedInteractionResolution;
		State.CameraCaptureInterval = SavedInteractionInterval;
		State.CameraCaptureMode = static_cast<EVirtualCameraCaptureMode>(SavedInteractionCaptureMode);
	}
	return true;
}
bool AVirtualCameraSensorActor::ApplyDatasetState(const FVirtualSensorEditableState& State, FString& Error)
{
	if (IsSensorRunning() || !ValidateEditableState(State, Error) || !CanEditSensorPlacement(Error))
		return false;
	FVirtualSensorEditableState Allowed;
	ReadEditableState(Allowed);
	Allowed.ActorTransform = State.ActorTransform;
	Allowed.bCameraGroundTruth = State.bCameraGroundTruth;
	if (!FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Allowed, &State, PPF_None))
	{
		Error = TEXT("데이터셋 중 카메라 규격이 변경되었습니다");
		return false;
	}
	SetActorTransform(State.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	CaptureComp->SetGroundTruthEnabled(State.bCameraGroundTruth);
	++ConfigurationRevision;
	return true;
}

bool AVirtualCameraSensorActor::BeginInteractiveManipulation(const FVirtualSensorInteractionRequest& Request)
{
	if (!CaptureComp || bInteractiveManipulationActive)
		return CaptureComp != nullptr;
	if (!Super::BeginInteractiveManipulation(Request))
		return false;
	bWasRunningBeforeInteraction = CaptureComp->IsCaptureRunning();
	SavedInteractionResolution = CaptureComp->CaptureResolution;
	SavedInteractionInterval = CaptureComp->CaptureInterval;
	SavedInteractionCaptureMode = static_cast<uint8>(CaptureComp->CaptureMode);
	CaptureComp->StopCapture();
	CaptureComp->CaptureResolution = Request.CameraPreviewResolution;
	CaptureComp->CaptureInterval = Request.CameraPreviewInterval;
	CaptureComp->CaptureMode = EVirtualCameraCaptureMode::PreviewOnly;
	if (bWasRunningBeforeInteraction)
		CaptureComp->StartCapture();
	return true;
}

void AVirtualCameraSensorActor::EndInteractiveManipulation()
{
	if (!CaptureComp || !bInteractiveManipulationActive)
		return;
	CaptureComp->StopCapture();
	CaptureComp->CaptureResolution = SavedInteractionResolution;
	CaptureComp->CaptureInterval = SavedInteractionInterval;
	CaptureComp->CaptureMode = static_cast<EVirtualCameraCaptureMode>(SavedInteractionCaptureMode);
	if (bWasRunningBeforeInteraction && !bSettingsTransaction)
	{
		CaptureComp->StartCapture();
		CaptureComp->RequestImmediateScheduledCapture();
	}
	Super::EndInteractiveManipulation();
}

bool AVirtualCameraSensorActor::ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError)
{
	if (!CanEditSensorConfiguration(OutError))
		return false;
	if (!CaptureComp || RequestedState.TargetKind != EVirtualSensorTargetKind::Camera)
	{
		OutError = TEXT("선택한 Camera 센서를 사용할 수 없습니다.");
		return false;
	}
	const bool bWasRunning = bInteractiveManipulationActive ? bWasRunningBeforeInteraction : CaptureComp->IsCaptureRunning();
	TGuardValue<bool> Transaction(bSettingsTransaction, true);
	if (bInteractiveManipulationActive)
		EndInteractiveManipulation();
	if (CaptureComp->IsCaptureRunning())
		CaptureComp->StopCapture();
	CaptureComp->ApplyDeviceProfile(RequestedState.CameraProfile);
	CaptureComp->ApplySimulationQuality(RequestedState.SimulationQuality);
	if (bWasRunning)
		CaptureComp->StartCapture();
	++ConfigurationRevision;
	return ReadEditableState(OutAppliedState);
}

bool AVirtualCameraSensorActor::ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const
{
	if (!CanEditSensorConfiguration(OutError))
		return false;
	if ((State.SensorId != GetSensorId() || !State.ActorTransform.Equals(GetActorTransform())) && !CanEditSensorPlacement(OutError))
		return false;
	if (!CaptureComp || State.TargetKind != EVirtualSensorTargetKind::Camera)
	{
		OutError = TEXT("선택한 Camera 센서를 사용할 수 없습니다.");
		return false;
	}
	if (State.SensorId.TrimStartAndEnd().IsEmpty() || State.ActorTransform.ContainsNaN())
	{
		OutError = TEXT("SensorId 또는 Transform 값이 올바르지 않습니다.");
		return false;
	}
	if (!FMath::IsFinite(State.CameraCaptureInterval) || !FMath::IsFinite(State.CameraFov) ||
	    State.CameraResolution.X < 160 || State.CameraResolution.X > 4096 ||
	    State.CameraResolution.Y < 90 || State.CameraResolution.Y > 2160 ||
	    State.CameraCaptureInterval < 0.033f || State.CameraCaptureInterval > 60.0f ||
	    State.CameraFov < 5.0f || State.CameraFov > 170.0f ||
	    State.CameraJpegQuality < 1 || State.CameraJpegQuality > 100)
	{
		OutError = TEXT("Camera 설정이 지원 범위를 벗어났습니다.");
		return false;
	}
	return true;
}

void AVirtualCameraSensorActor::HandleCameraFrame(const FString& JsonPayload, UTextureRenderTarget2D* RenderTarget)
{
	if (!OutputComp || !CaptureComp)
		return;
	const TSharedPtr<const TArray64<uint8>, ESPMode::ThreadSafe> JpegSnapshot = CaptureComp->GetLastJpegSnapshot();
	if (JsonPayload.IsEmpty() && !JpegSnapshot.IsValid())
		return;
	FVirtualSensorFrameEnvelope Frame;
	const FVirtualSensorRuntimeStatus& Status = CaptureComp->GetRuntimeStatus();
	Frame.SensorId = PendingExternalSendTransport.IsSet() && !Status.SensorId.IsEmpty()
	                     ? Status.SensorId
	                     : CaptureComp->SensorId;
	Frame.SensorKind = EVirtualSensorKind::Camera;
	Frame.FrameId = Status.FrameId;
	Frame.SlabContext = CaptureComp->GetLastSlabContext();
	Frame.TimestampUtc = CaptureComp->GetRuntimeStatus().LastUpdateUtc;
	Frame.SchemaVersion = TEXT("virtual-camera.v1");
	if (!JsonPayload.IsEmpty())
		Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(JsonPayload);
	Frame.bSendTransport = PendingExternalSendTransport.IsSet()
	                           ? PendingExternalSendTransport.GetValue()
	                           : CaptureComp->CaptureMode == EVirtualCameraCaptureMode::PayloadAndOutput;
	Frame.bRecord = true;
	Frame.BinaryPayload = JpegSnapshot;
	Frame.CameraJpegMetadata = CaptureComp->GetLastJpegMetadata();
	PendingExternalSendTransport.Reset();
	if (Frame.HasJsonPayload())
		OutputComp->RouteFrame(Frame);
	else
		OutputComp->RouteAcquiredFrame(Frame);
}

void AVirtualCameraSensorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AVirtualCameraSensorActor::RequestGroundTruthSave()
{
	auto* Files = GetWorld() ? GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>() : nullptr;
	return !bInteractiveManipulationActive && Files && Files->RequestCameraGroundTruthSave(this);
}
void AVirtualCameraSensorActor::SetGroundTruthPreviewDemand(bool Enabled)
{
	if (CaptureComp)
		CaptureComp->SetGroundTruthPreviewDemand(Enabled && !bInteractiveManipulationActive);
}
