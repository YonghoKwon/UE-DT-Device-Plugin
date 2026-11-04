#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DxWebSocketSubsystem.h"
#include "DxDeviceSensorChannelSubsystem.generated.h"

class UDxDeviceSensorBulkChannel;
class UDxDeviceSensorStompChannel;
class UDxWebSocketSubsystem;
class UDxDataChannelBase;
class UDeviceDataChannelSettings;
class UDxDeviceSensorChannelSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDeviceDataTopicMessage, FName, ChannelName, FName, Alias, const FWebSocketMessage&, Message);

/** Binding lifetime belongs to the registry; callbacks never retain the registry/world. */
UCLASS()
class UDeviceDataTopicSubscription : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<UDxDeviceSensorChannelSubsystem> Owner;
	TWeakObjectPtr<UDxDataChannelBase> Channel;
	FName ChannelName, Alias;
	FString Destination, Key;
	void Issue();
	void ObserveSharedState(EDxDataChannelState State);
	UFUNCTION()
	void Receive(const FWebSocketMessage& Message);
};

/** 채널 등록만 GI 수명에 묶는다. 기본 등록은 연결/worker/tick을 시작하지 않는다. */
UCLASS()
class DEVICERUNTIME_API UDxDeviceSensorChannelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	static UDxDeviceSensorBulkChannel* FindBulk(const UWorld* World);
	static UDxDeviceSensorStompChannel* FindStomp(const UWorld* World);
	/** Registers interests, never opens a connection. STOMP retains keys across reconnects. */
	void ConfigureTopicSubscriptions(const UDeviceDataChannelSettings& Settings);
	UFUNCTION(BlueprintPure, Category = "Device data")
	FString GetTopicConfigurationWarning() const { return TopicConfigurationWarning; }
	UPROPERTY(BlueprintAssignable, Category = "Device data")
	FDeviceDataTopicMessage OnTopicMessage;

private:
	TWeakObjectPtr<UDxWebSocketSubsystem> Socket;
	UPROPERTY(Transient)
	TObjectPtr<UDxDeviceSensorBulkChannel> Bulk;
	UPROPERTY(Transient)
	TObjectPtr<UDxDeviceSensorStompChannel> Sensor;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDeviceDataTopicSubscription>> TopicSubscriptions;
	FString TopicConfigurationWarning;
	UFUNCTION()
	void HandleChannelState(FName ChannelName, EDxDataChannelState State, FString Reason);
};
