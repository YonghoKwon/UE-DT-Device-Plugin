#pragma once
#include "CoreMinimal.h"
#include "DeviceUI/DxDeviceSensorPanelBase.h"
#include "DxDeviceSensorMonitorPanel.generated.h"

class AVirtualSensorActorBase;
class UVirtualLidarScanComp;
class UVirtualCameraCaptureComp;
class UComboBoxString;
class UImage;
class UTextBlock;

/** Standalone DTCore panel. Preview controls never change acquisition or server output. */
UCLASS(Blueprintable)
class DEVICEUI_API UDxDeviceSensorMonitorPanel : public UDxDeviceSensorPanelBase
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") void RefreshSensors();
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") void SelectSensor(AVirtualSensorActorBase* Sensor);
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") void RefreshStatus();
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") void CaptureOnce();
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") bool AdjustPreview(int32 Direction);
    UFUNCTION(BlueprintPure, Category="Device|Monitor") FString GetStatusText() const { return StatusText; }
    AVirtualSensorActorBase* GetSelectedSensor() const { return SelectedSensor.Get(); }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    friend struct FDeviceSensorMonitorBody;
    bool bShowingLidar=false;
    UPROPERTY(Transient) TObjectPtr<UVirtualLidarScanComp> LidarComp;
    UPROPERTY(Transient) TObjectPtr<UVirtualCameraCaptureComp> CameraComp;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> SensorList;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Summary;
    UPROPERTY(Transient) TObjectPtr<UImage> Preview;
    TArray<TWeakObjectPtr<AVirtualSensorActorBase>> Sensors;
    TWeakObjectPtr<AVirtualSensorActorBase> SelectedSensor;
    FTimerHandle RefreshTimer;
    FString StatusText;
    bool bRefreshingList=false;
    const UVirtualLidarScanComp* GetTargetLidarForPreview() const { return LidarComp; }
    UFUNCTION() void HandleSelection(FString Item, ESelectInfo::Type Type);
    UFUNCTION() void MorePreview();
    UFUNCTION() void LessPreview();
};
