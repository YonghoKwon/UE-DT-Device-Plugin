#pragma once

#include "CoreMinimal.h"
#include "Core/DxStompChannel.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "IStompClient.h"
#include "DxDeviceSensorStompChannel.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FDeviceSensorChannelConnected, const FString&, const FString&, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDeviceSensorChannelFailure, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FDeviceSensorChannelAck, const FWebSocketMessage&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDeviceSensorChannelAckSubscribed, bool, const FString&);

/** 기존 JSON/수동 binary STOMP 연결을 DTCore 연결·세대·재연결 기반으로 제공한다. */
UCLASS()
class DEVICERUNTIME_API UDxDeviceSensorStompChannel : public UDxStompChannel
{
	GENERATED_BODY()

public:
	virtual bool Connect() override;
	bool AcquireOwner(UObject* Owner, const FVirtualSensorTransportProfile& Profile, const FString& Passcode, const FString& BearerToken, FString& Error);
	void ReleaseOwner(UObject* Owner);
	bool IsConnected() const;
	bool Send(const FString& Destination, const FString& Body, const FStompHeader& Headers, const FStompRequestCompleted& Callback = FStompRequestCompleted());
	bool Send(const FString& Destination, const FStompBuffer& Body, const FStompHeader& Headers, const FStompRequestCompleted& Callback = FStompRequestCompleted());
	FString SubscribeToAck(const FString& Topic);
	void SetExplicitSettings(bool bExplicit) { bExplicitSettings = bExplicit; }
	FDeviceSensorChannelConnected Connected;
	FDeviceSensorChannelFailure Failed;
	FDeviceSensorChannelAck AckReceived;
	FDeviceSensorChannelAckSubscribed AckSubscribed;

protected:
	virtual void OnChannelInitialized() override;
	virtual void OnChannelShutdown() override;
	virtual TSharedPtr<IStompClient> CreateStompClient(const FString& Url) override;
	virtual void HandleTransportConnected() override;
	virtual void HandleTransportClosed(const FString& Reason) override;
	virtual void HandleTransportError(const FString& Error) override;

private:
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
	friend class FDeviceStompChannelLifecycle;
	TSharedPtr<IStompClient> TestClient;
#endif
	UFUNCTION()
	void HandleAckFrame(const FWebSocketMessage& Message);
	UFUNCTION()
	void HandleAckSubscribed(bool bSuccess, FString Error);
	void ClearAckSubscription();
	void PruneOwners();
	TSet<TWeakObjectPtr<UObject>> Owners;
	FVirtualSensorTransportProfile LegacyProfile;
	FString SessionPasscode, SessionBearerToken;
	FString ProtocolVersion, SessionId, ServerString, AckKey, AckTopic;
	bool bExplicitSettings = false;
};
