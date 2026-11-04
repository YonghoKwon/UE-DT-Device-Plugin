#include "Device/Transport/VirtualSensorStreamReceiverRegistration.h"
#include "Device/Sensor/DeviceSensorStreamCodec.h"
#include "Device/Contracts/VirtualSensorStreamReceiverRegistry.h"

namespace
{
class FVirtualSensorTcReceiverCodec final : public IVirtualSensorStreamReceiverCodec
{
public:
	virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> CreateResult(EVirtualSensorTopicReceiveKind Kind) const override
	{
		if (Kind == EVirtualSensorTopicReceiveKind::PointCloud)
			return MakeShared<FVirtualPointCloudStreamReceiverData>();
		if (Kind == EVirtualSensorTopicReceiveKind::Camera)
			return MakeShared<FVirtualCameraStreamReceiverData>();
		return MakeShared<FVirtualLidarStreamReceiverData>();
	}

	virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> ParseBinaryPcd(
	    const TArray<uint8>& Body, const TMap<FName, FString>& Headers) const override
	{
		return StaticCastSharedPtr<FVirtualSensorTopicReceivedDataBase>(DeviceSensorStreamCodec::ParseBinaryPcd(Body, Headers));
	}
};
}

void RegisterVirtualSensorStreamReceiverCodec()
{
	VirtualSensorStreamReceiverRegistry::Register(MakeShared<FVirtualSensorTcReceiverCodec, ESPMode::ThreadSafe>());
}

void UnregisterVirtualSensorStreamReceiverCodec()
{
	VirtualSensorStreamReceiverRegistry::Unregister();
}
