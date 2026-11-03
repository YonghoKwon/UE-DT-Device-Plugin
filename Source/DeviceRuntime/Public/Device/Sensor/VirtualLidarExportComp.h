#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Device/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualLidarExportComp.generated.h"

class UVirtualLidarScanComp;

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class DEVICERUNTIME_API UVirtualLidarExportComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarExportComp();
	void BindScanComponent(UVirtualLidarScanComp* InScanComponent);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportCsv(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportJsonLines(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportPcd(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportLas(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportLaz(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export")
	FString GetLastExportPath() const;
	/** Stateless worker-safe serializer; all inputs must be frozen before dispatch. */
	static bool SerializeImmutableFrame(const FVirtualSensorFrameEnvelope& Frame,const FVirtualSensorStreamConfig& Config,FString& Extension,TArray<uint8>& Bytes,int32& PointCount,FString& Error);

private:
	UPROPERTY(Transient)
	TObjectPtr<UVirtualLidarScanComp> ScanComp;
};
