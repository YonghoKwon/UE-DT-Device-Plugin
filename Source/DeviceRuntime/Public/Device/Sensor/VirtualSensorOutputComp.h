#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Device/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualSensorOutputComp.generated.h"

class UVirtualSensorRecorderComp;
class UVirtualSensorTransportComp;
class UVirtualSensorStreamPublisherComp;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVirtualSensorFrameRouted, const FString&, SensorId, int64, FrameId);

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class DEVICERUNTIME_API UVirtualSensorOutputComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualSensorOutputComp();

	void SetSharedServices(UVirtualSensorTransportComp* InTransport, UVirtualSensorRecorderComp* InRecorder, UVirtualSensorStreamPublisherComp* InStreamPublisher = nullptr);
	bool RouteFrame(const FVirtualSensorFrameEnvelope& Frame);
	/** Routes an immutable acquisition frame to bounded streams before optional legacy JSON is ready. */
	bool RouteAcquiredFrame(const FVirtualSensorFrameEnvelope& Frame);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorOutput")
	FString GetLastJsonPayload() const { return LastJsonPayload; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|SensorOutput")
	int64 GetLastRoutedFrameId() const { return LastRoutedFrameId; }

	UPROPERTY(BlueprintAssignable, Category = "DigitalTwin|SensorOutput")
	FOnVirtualSensorFrameRouted OnFrameRouted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorOutput")
	TObjectPtr<UVirtualSensorTransportComp> TransportComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|SensorOutput")
	TObjectPtr<UVirtualSensorRecorderComp> RecorderComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorOutput")
	TObjectPtr<UVirtualSensorStreamPublisherComp> StreamPublisherComp;

private:
	FString BuildSensorType(EVirtualSensorKind Kind) const;

	FString LastJsonPayload;
	FString LastRoutedSensorId;
	int64 LastRoutedFrameId = INDEX_NONE;
};
