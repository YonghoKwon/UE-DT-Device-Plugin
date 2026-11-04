#pragma once
#include "CoreMinimal.h"
#include "Device/Sensor/VirtualLidarSensorTypes.h"

/** Immutable frame policy. Implementations may select business points; device math has no labels. */
class DEVICERUNTIME_API IDevicePointCloudAnalysisProvider
{
public:
	virtual ~IDevicePointCloudAnalysisProvider() = default;
	virtual FVirtualLidarSlabAnalysisResult Analyze(const TArray<FVirtualLidarPoint>& Points,
	                                                FName Label, float ReferenceYawDegrees, int32 MinPoints) const = 0;
};
namespace DevicePointCloudAnalysis
{
DEVICERUNTIME_API void Register(TSharedRef<const IDevicePointCloudAnalysisProvider, ESPMode::ThreadSafe> Provider);
DEVICERUNTIME_API void Unregister();
DEVICERUNTIME_API TSharedPtr<const IDevicePointCloudAnalysisProvider, ESPMode::ThreadSafe> Snapshot();
/** Pure XY PCA/bounds. Caller owns selection, minimum count and status wording. */
DEVICERUNTIME_API FVirtualLidarSlabAnalysisResult Geometry(TConstArrayView<FVector> SelectedPoints, float ReferenceYawDegrees);
}
