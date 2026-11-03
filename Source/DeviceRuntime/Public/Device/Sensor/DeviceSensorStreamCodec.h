#pragma once
#include "CoreMinimal.h"
#include "Device/Contracts/DeviceSensorStreamData.h"
/** Stateless parsers shared by DTCore TC handlers and the raw transport worker. */
namespace DeviceSensorStreamCodec
{
    DEVICERUNTIME_API TSharedPtr<FTransactionCodeDataBase> ParseCameraJson(const FString& JsonString);
    DEVICERUNTIME_API TSharedPtr<FTransactionCodeDataBase> ParseLidarJson(const FString& JsonString);
    DEVICERUNTIME_API TSharedPtr<FTransactionCodeDataBase> ParsePointCloudJson(const FString& JsonString);
    DEVICERUNTIME_API TSharedPtr<FTransactionCodeDataBase> ParseBinaryPcd(const TArray<uint8>& Body,const TMap<FName,FString>& Headers);
}
