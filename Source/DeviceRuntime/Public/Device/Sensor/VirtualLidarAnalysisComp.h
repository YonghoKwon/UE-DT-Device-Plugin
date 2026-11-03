#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualLidarAnalysisComp.generated.h"

class UVirtualLidarScanComp;

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class DEVICERUNTIME_API UVirtualLidarAnalysisComp : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarAnalysisComp();
	void BindScanComponent(UVirtualLidarScanComp* InScanComponent);
	void AnalyzeLatestFrame();
	void ApplyPrecomputedStatistics(int32 HitPointCount, const TMap<FString, int32>& SemanticCounts);

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Analysis")
	int32 GetLatestHitPointCount() const { return LatestHitPointCount; }

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Analysis")
	TMap<FString, int32> GetLatestSemanticCounts() const { return LatestSemanticCounts; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UVirtualLidarScanComp> ScanComp;

	UPROPERTY(Transient)
	TMap<FString, int32> LatestSemanticCounts;

	int32 LatestHitPointCount = 0;
};
