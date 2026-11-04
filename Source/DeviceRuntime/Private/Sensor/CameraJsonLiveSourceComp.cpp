#include "Device/Sensor/CameraJsonLiveSourceComp.h"

#include "GameFramework/Actor.h"
#include "Device/Camera/VirtualCameraCaptureComp.h"
#include "Device/Sensor/VirtualSensorActorBase.h"

UCameraJsonLiveSourceComp::UCameraJsonLiveSourceComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    SourceKind = ERealSensorSourceKind::JsonLiveBridge;
    SourceId = TEXT("CameraJsonLiveBridge");
    LastSourceMessage = TEXT("Camera JSON live bridge is ready for virtual-camera.v1 payloads.");
}

bool UCameraJsonLiveSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("Camera JSON live bridge ready."));
    return true;
}

void UCameraJsonLiveSourceComp::StopSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("Camera JSON live bridge stopped."));
}

bool UCameraJsonLiveSourceComp::AppendLivePayloadJson(const FString& PayloadJson)
{
    FString Trimmed = PayloadJson;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Camera JSON live payload is empty."));
        return false;
    }

    BufferedPayloadJson = Trimmed;
    bHasBufferedPayload = true;
    LastPayloadLength = BufferedPayloadJson.Len();
    LastSourceMessage = FString::Printf(TEXT("Buffered camera JSON live payload. bytes=%d"), LastPayloadLength);
    return true;
}

void UCameraJsonLiveSourceComp::ClearBufferedFrame()
{
    BufferedPayloadJson.Reset();
    bHasBufferedPayload = false;
    LastPayloadLength = 0;
    LastSourceMessage = TEXT("Camera JSON live buffer cleared.");
}

bool UCameraJsonLiveSourceComp::PushFrameOnce(bool bSendTransport)
{
    UVirtualCameraCaptureComp* CameraComp = ResolveTargetCamera();
    AVirtualSensorActorBase* SensorActor = ResolveTargetSensorActor();
    if (!CameraComp && !SensorActor)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Target camera is not set."));
        return false;
    }
    if (!bHasBufferedPayload || BufferedPayloadJson.IsEmpty())
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Camera JSON live buffer is empty."));
        return false;
    }

    bool bAccepted = false;
    if (SensorActor)
    {
        FVirtualSensorFrameEnvelope Frame;
        Frame.SensorId = SensorActor->GetSensorId();
        Frame.SensorKind = EVirtualSensorKind::Camera;
        Frame.FrameId = LastSourceFrameId + 1;
        Frame.TimestampUtc = FDateTime::UtcNow();
        Frame.SchemaVersion = TEXT("virtual-camera.v1");
        Frame.JsonPayload = MakeShared<const FString, ESPMode::ThreadSafe>(BufferedPayloadJson);
        bAccepted = SensorActor->SubmitExternalFrame(Frame, bSendTransport);
    }
    else
    {
        bAccepted = CameraComp->InjectExternalJsonPayload(BufferedPayloadJson, bSendTransport);
    }

    if (!bAccepted)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Camera JSON live payload was rejected by target camera."));
        return false;
    }

    MarkFramePushed(1, FString::Printf(TEXT("Pushed camera JSON live frame. bytes=%d sendTransport=%s"),
        LastPayloadLength,
        bSendTransport ? TEXT("true") : TEXT("false")));
    if (bClearBufferAfterPush)
    {
        ClearBufferedFrame();
    }
    return true;
}

AVirtualSensorActorBase* UCameraJsonLiveSourceComp::ResolveTargetSensorActor() const
{
    if (TargetSensorActor)
    {
        return TargetSensorActor;
    }
    if (TargetCameraComp)
    {
        return Cast<AVirtualSensorActorBase>(TargetCameraComp->GetOwner());
    }
    return Cast<AVirtualSensorActorBase>(GetOwner());
}

UVirtualCameraCaptureComp* UCameraJsonLiveSourceComp::ResolveTargetCamera() const
{
    if (TargetCameraComp)
    {
        return TargetCameraComp;
    }
    return GetOwner() ? GetOwner()->FindComponentByClass<UVirtualCameraCaptureComp>() : nullptr;
}
