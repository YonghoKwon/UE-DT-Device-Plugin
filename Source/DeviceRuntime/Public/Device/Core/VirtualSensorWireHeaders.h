#pragma once
#include "CoreMinimal.h"
struct FVirtualSensorBinaryFrame;
struct FVirtualPointCloudBinaryMetadata;

struct DEVICERUNTIME_API FVirtualSensorWireHeaders
{
	static TMap<FString, FString> RawPcd(const FVirtualPointCloudBinaryMetadata& Metadata);
	static TMap<FString, FString> RawFrame(const FVirtualSensorBinaryFrame& Frame);
	static TMap<FName, FString> EnginePcd(const FVirtualPointCloudBinaryMetadata& Metadata, const FString& RequestId);
};
