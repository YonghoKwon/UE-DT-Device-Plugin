#pragma once
#include "CoreMinimal.h"
struct FVirtualSensorTopicReceivedDataBase;
struct DEVICERUNTIME_API FVirtualSensorPcdReceiveContract
{
	static bool Normalize(const TMap<FName,FString>& Input,TMap<FName,FString>& Output,FVirtualSensorTopicReceivedDataBase& Data);
};
