#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeviceDataChannelSettings.generated.h"

/** Project policy. DTCore's channel settings continue to own connections only. */
USTRUCT(BlueprintType)
struct DEVICERUNTIME_API FDeviceDataDestination
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device data")
	FName Alias;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device data")
	FString Destination;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device data")
	bool bSubscribe = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device data")
	FString Purpose;
};

USTRUCT(BlueprintType)
struct DEVICERUNTIME_API FDeviceDataChannelTopics
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device data")
	FName ChannelName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Device data")
	TArray<FDeviceDataDestination> Destinations;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Device data topics"))
class DEVICERUNTIME_API UDeviceDataChannelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Empty list/entry retains the existing SensorTransport.ini and map/UI profile. */
	UPROPERTY(Config, EditAnywhere, Category = "Device data")
	TArray<FDeviceDataChannelTopics> Channels;

	const FDeviceDataChannelTopics* FindTopics(FName ChannelName) const;
	const FDeviceDataDestination* FindDestination(FName ChannelName, FName Alias) const;
	FString Resolve(FName ChannelName, FName Alias, const FString& LegacyDestination) const;
	FString ResolveSubscription(FName ChannelName, FName Alias, const FString& LegacyDestination) const;
	TArray<FString> GetSubscriptions(FName ChannelName) const;
	TArray<FString> GetDuplicateSubscriptionWarnings() const;
	bool Validate(FString& Error) const;
};
