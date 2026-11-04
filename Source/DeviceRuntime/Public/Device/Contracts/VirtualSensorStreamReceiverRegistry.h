#pragma once

#include "Device/Contracts/VirtualSensorStreamReceiverTypes.h"

/** Stateless receiver codec supplied by the host TC layer. Calls may run on workers. */
class DEVICERUNTIME_API IVirtualSensorStreamReceiverCodec
{
public:
	virtual ~IVirtualSensorStreamReceiverCodec() = default;
	virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> CreateResult(EVirtualSensorTopicReceiveKind Kind) const = 0;
	virtual TSharedPtr<FVirtualSensorTopicReceivedDataBase> ParseBinaryPcd(
	    const TArray<uint8>& Body, const TMap<FName, FString>& Headers) const = 0;
};

/** Module-owned registration; snapshots retain the stateless codec during worker calls. */
namespace VirtualSensorStreamReceiverRegistry
{
DEVICERUNTIME_API void Register(const TSharedRef<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe>& Codec);
DEVICERUNTIME_API void Unregister();
DEVICERUNTIME_API TSharedPtr<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe> Get();
}
