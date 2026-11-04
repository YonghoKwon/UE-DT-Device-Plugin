#pragma once

#include "CoreMinimal.h"
#include "Device/Contracts/VirtualSensorAcquisitionContext.h"
#include "Device/Sensor/VirtualLidarFidelity.h"
#include "Device/Sensor/VirtualSensorDeviceProfileTypes.h"

struct DEVICERUNTIME_API FVirtualLidarDepthAcquisitionRequest
{
	bool bRequestSemantics = false;
	bool bRequestFidelity = false;
	TArray<float> HorizontalAngles;
	TArray<float> VerticalAngles;
	FVirtualSensorAcquisitionContext SlabContext;
	FTransform AcquisitionTransform = FTransform::Identity;
	int64 FrameId = 0;
	int32 HorizontalSamples = 1;
	int32 VerticalChannels = 1;
	float HorizontalFovDegrees = 90.0f;
	float MinVerticalAngleDegrees = -10.0f;
	float MaxVerticalAngleDegrees = 10.0f;
	float MaxDistanceCm = 10000.0f;
	int64 AcquisitionStartUnixNanoseconds = 0;
};
struct FVirtualLidarGpuSemanticIdentity
{
	VirtualLidarFidelity::FSurface Surface;
	FVirtualSensorMotionSample Motion;
	FName Label;
	FName ActorName;
	FName ActorClass;
	TArray<FName> ActorTags;
	bool bHasOpticalOverride = false;
	float Reflectivity940Nm = .2f, IntensityGain = 1;
};
struct DEVICERUNTIME_API FVirtualLidarDepthAcquisitionFrame
{
	FVirtualLidarDepthAcquisitionRequest Request;
	int32 CaptureWidth = 0;
	int32 CaptureHeight = 0;
	TArray<float> ForwardDepthCentimeters;
	TArray<FLinearColor> SemanticIdDepth;
	TMap<int32, FVirtualLidarGpuSemanticIdentity> SemanticIdentities;
	FString SemanticStatus;
	int64 AcquisitionEndUnixNanoseconds = 0;
};

enum class EVirtualSensorBackendPollResult : uint8
{
	Idle,
	Pending,
	Completed,
	Failed
};

/** Runtime-pluggable acquisition boundary shared by scalable sensor backends. */
class DEVICERUNTIME_API IVirtualSensorAcquisitionBackend
{
public:
	virtual ~IVirtualSensorAcquisitionBackend() = default;
	virtual EVirtualLidarAcquisitionBackend GetBackendType() const = 0;
	virtual bool IsAvailable() const = 0;
	virtual bool BeginAcquisition(const FVirtualLidarDepthAcquisitionRequest& Request) = 0;
	virtual EVirtualSensorBackendPollResult PollAcquisition(FVirtualLidarDepthAcquisitionFrame& OutFrame) = 0;
	virtual void CancelAcquisition() = 0;
	virtual FString GetBackendStatusMessage() const = 0;
};
