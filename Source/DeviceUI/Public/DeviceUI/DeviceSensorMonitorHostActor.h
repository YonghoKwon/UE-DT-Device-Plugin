#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeviceSensorMonitorHostActor.generated.h"
class UDeviceSensorMonitorPanel;
/** Opt-in map host, registered with DTCore; never replaces a GameMode or creates sensors. */
UCLASS()
class DEVICEUI_API ADeviceSensorMonitorHostActor : public AActor
{
    GENERATED_BODY()
public:
    ADeviceSensorMonitorHostActor();
    UPROPERTY(EditAnywhere, Category="Device|Monitor") TSubclassOf<UDeviceSensorMonitorPanel> PanelClass;
    UPROPERTY(EditAnywhere, Category="Device|Monitor") bool bShowOnBeginPlay=true;
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") void ShowMonitor();
    UFUNCTION(BlueprintCallable, Category="Device|Monitor") void HideMonitor();
    UDeviceSensorMonitorPanel* GetPanel() const { return Panel; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UDeviceSensorMonitorPanel> Panel;
};
