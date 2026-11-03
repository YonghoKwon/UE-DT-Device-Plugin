#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VirtualSensorMonitorView.generated.h"

class APlayerController;
class UVirtualCameraCaptureComponent;
class UVirtualLidarScanComp;
class URealSensorSourceComp;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UVirtualSensorMonitorView : public UInterface
{
    GENERATED_BODY()
};

/** Native presentation boundary; no widget types or measurement side effects. */
class DEVICERUNTIME_API IVirtualSensorMonitorView
{
    GENERATED_BODY()
public:
    virtual void BindMonitorCamera(UVirtualCameraCaptureComponent* Camera) = 0;
    virtual void BindMonitorLidar(UVirtualLidarScanComp* Lidar) = 0;
    virtual void BindMonitorExternalSource(URealSensorSourceComp* Source) = 0;
    virtual void ShowMonitorCamera() = 0;
    virtual void ShowMonitorLidar() = 0;
    virtual APlayerController* GetMonitorViewController() const = 0;
};
