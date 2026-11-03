#include "Device/Sensor/VirtualLidarAnalysisComp.h"

#include "Device/Sensor/VirtualLidarScanComp.h"

UVirtualLidarAnalysisComp::UVirtualLidarAnalysisComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualLidarAnalysisComp::BindScanComponent(UVirtualLidarScanComp* InScanComponent)
{
	ScanComp = InScanComponent;
}

void UVirtualLidarAnalysisComp::AnalyzeLatestFrame()
{
	LatestHitPointCount = 0;
	LatestSemanticCounts.Reset();
	if (!ScanComp) return;

	for (const FVirtualLidarPoint& Point : ScanComp->GetLastPoints())
	{
		if (!Point.bHit) continue;
		++LatestHitPointCount;
		const FString Label = Point.SemanticLabel.IsNone() ? TEXT("Unclassified") : Point.SemanticLabel.ToString();
		LatestSemanticCounts.FindOrAdd(Label)++;
	}
}

void UVirtualLidarAnalysisComp::ApplyPrecomputedStatistics(int32 HitPointCount, const TMap<FString, int32>& SemanticCounts)
{
	LatestHitPointCount = HitPointCount;
	LatestSemanticCounts = SemanticCounts;
}
