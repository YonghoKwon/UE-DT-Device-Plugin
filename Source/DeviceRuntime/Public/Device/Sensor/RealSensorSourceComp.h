#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Device/Sensor/VirtualLidarSensorTypes.h"
#include "RealSensorSourceComp.generated.h"

class UVirtualLidarScanComp;
class AVirtualSensorActorBase;

UENUM(BlueprintType)
enum class ERealSensorSourceKind : uint8
{
    FileReplay UMETA(DisplayName = "File Replay"),
    JsonLiveBridge UMETA(DisplayName = "JSON Live Bridge"),
    Ros2Bridge UMETA(DisplayName = "ROS2 Bridge"),
    LivoxLidar UMETA(DisplayName = "Livox LiDAR"),
    RealSenseCamera UMETA(DisplayName = "RealSense Camera"),
    Custom UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class ERealSensorSourceConnectionState : uint8
{
    Stopped UMETA(DisplayName = "Stopped"),
    Starting UMETA(DisplayName = "Starting"),
    Running UMETA(DisplayName = "Running"),
    Error UMETA(DisplayName = "Error")
};

UCLASS(Abstract, BlueprintType, ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class DEVICERUNTIME_API URealSensorSourceComp : public USceneComponent
{
    GENERATED_BODY()

public:
    URealSensorSourceComp();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorSource")
    virtual bool StartSource();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorSource")
    virtual void StopSource();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorSource")
    virtual bool PushFrameOnce(bool bSendTransport = true);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource")
    bool IsSourceRunning() const { return ConnectionState == ERealSensorSourceConnectionState::Running; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource")
    ERealSensorSourceConnectionState GetConnectionState() const { return ConnectionState; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource")
    FString GetLastSourceMessage() const { return LastSourceMessage; }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource|Deployment")
    FString GetDeploymentReadinessSummaryText() const;

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorSource|Deployment")
    bool RequiresExternalDeploymentEvidence() const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    ERealSensorSourceKind SourceKind = ERealSensorSourceKind::Custom;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    FString SourceId = TEXT("RealSensorSource");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    TObjectPtr<UVirtualLidarScanComp> TargetLidarComp;

    /** Preferred V2 target. TargetLidar remains as a component-level compatibility fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    TObjectPtr<AVirtualSensorActorBase> TargetSensorActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    bool bAutoStartSource = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorSource")
    bool bSendTransportByDefault = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    ERealSensorSourceConnectionState ConnectionState = ERealSensorSourceConnectionState::Stopped;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    int64 LastSourceFrameId = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    int32 LastSourcePointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorSource|Status")
    FString LastSourceMessage;

protected:
    AVirtualSensorActorBase* ResolveTargetSensorActor() const;
    UVirtualLidarScanComp* ResolveTargetLidar() const;
    bool PushPointFrameToTarget(const TArray<FVirtualLidarPoint>& Points, bool bSendTransport, const FString& SuccessMessage, int32 Rows = 0, int32 Cols = 0, bool bUpdateLidarDimensions = false);
    void SetSourceState(ERealSensorSourceConnectionState NewState, const FString& Message);
    void MarkFramePushed(int32 PointCount, const FString& Message);
};
