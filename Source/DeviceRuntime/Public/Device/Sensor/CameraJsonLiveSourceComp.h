#pragma once

#include "CoreMinimal.h"
#include "Device/Sensor/RealSensorSourceComp.h"
#include "CameraJsonLiveSourceComp.generated.h"

class UVirtualCameraCaptureComponent;
class AVirtualSensorActorBase;

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class DEVICERUNTIME_API UCameraJsonLiveSourceComp : public URealSensorSourceComp
{
    GENERATED_BODY()

public:
    UCameraJsonLiveSourceComp();

    virtual bool StartSource() override;
    virtual void StopSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|Camera")
    bool AppendLivePayloadJson(const FString& PayloadJson);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|Camera")
    void ClearBufferedFrame();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|Camera")
    TObjectPtr<UVirtualCameraCaptureComponent> TargetCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|Camera")
    bool bClearBufferAfterPush = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Camera|Status")
    bool bHasBufferedPayload = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Camera|Status")
    int32 LastPayloadLength = 0;

private:
    AVirtualSensorActorBase* ResolveTargetSensorActor() const;
    UVirtualCameraCaptureComponent* ResolveTargetCamera() const;

private:
    UPROPERTY()
    FString BufferedPayloadJson;
};
