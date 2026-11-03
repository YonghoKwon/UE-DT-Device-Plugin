#pragma once

#include "CoreMinimal.h"
#include "Device/Contracts/VirtualSensorMotionProvider.h"
#include "Device/Sensor/VirtualSensorDeviceProfileTypes.h"

class AActor;
class UPrimitiveComponent;

/** Pure, SI-explicit approximations shared by CPU/GPU conversion and regression tests. */
namespace VirtualLidarFidelity
{
struct FSurface
{
    float Reflectivity = .2f;
    float IntensityGain = 1.f;
    float DetectionProbabilityScale = 1.f;
    bool bTwoSided = false;
    bool bAllowSaturation = true;
};
DEVICERUNTIME_API int64 PointOffset(EVirtualLidarDeviceProfile Profile, int32 RayIndex, int32 Width, int32 Height, double PeriodSeconds);
DEVICERUNTIME_API FVirtualSensorMotionSample CaptureMotion(const AActor* Actor);
DEVICERUNTIME_API float SemanticReflectivity(FName Label, bool bOxidized = false);
DEVICERUNTIME_API FSurface ResolveSurface(const UPrimitiveComponent* Primitive);
DEVICERUNTIME_API float Intensity(const FSurface& Surface, float RangeCm, float IncidenceCosine, float AmbientLux);
DEVICERUNTIME_API float DetectionProbability(float Intensity, const FSurface& Surface);
DEVICERUNTIME_API float NoiseScale(float RangeRatio, float Reflectivity, float IncidenceCosine);
/** Tangent-plane rigid-motion approximation. Output remains in the acquisition reference pose (no deskew). */
DEVICERUNTIME_API bool RollingRange(const FVector& HitWorldCm, const FVector& NormalWorld, const FVector& RayWorld,
    const FTransform& AcquisitionPose, const FVirtualSensorMotionSample& Target, const FVirtualSensorMotionSample& Sensor,
    double OffsetSeconds, float& OutRangeCm, float& OutIncidenceCosine);
}
