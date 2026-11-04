#include "Device/Core/DxDeviceSensorStompChannel.h"

#include "../Sensor/DeviceStompSessionClient.h"
#include "../Sensor/DeviceSensorTransportConfig.h"
#include "Core/DTCoreSettings.h"
#include "Device/Core/DeviceDataChannelSettings.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "StompModule.h"
#include "TimerManager.h"

void UDxDeviceSensorStompChannel::OnChannelInitialized()
{
	const FName Name = GetChannelName();
	bExplicitSettings = GetDefault<UDTCoreSettings>()->DataChannels.ContainsByPredicate([Name](const auto& Item)
	                                                                                    { return Item.ChannelName == Name; });
	if (!bExplicitSettings)
		DeviceSensorTransportConfig::ReadLegacyConnection(LegacyProfile, SessionPasscode);
	const FString ProjectAck = GetDefault<UDeviceDataChannelSettings>()->ResolveSubscription(Name, TEXT("ack"), FString());
	bConfiguredAck = !ProjectAck.IsEmpty();
	if (bConfiguredAck)
		SubscribeToAck(ProjectAck);
}

void UDxDeviceSensorStompChannel::PruneOwners()
{
	for (auto It = Owners.CreateIterator(); It; ++It)
		if (!It->IsValid())
			It.RemoveCurrent();
}

bool UDxDeviceSensorStompChannel::AcquireOwner(UObject* Owner, const FVirtualSensorTransportProfile& Profile, const FString& Passcode, const FString& BearerToken, FString& Error)
{
	check(IsInGameThread());
	Error.Reset();
	if (!IsValid(Owner) || IsShutdown())
	{
		Error = TEXT("sensor channel is unavailable.");
		return false;
	}
	PruneOwners();
	if (!bExplicitSettings)
	{
		FStompHeader Headers;
		if (!UVirtualSensorTransportComp::BuildStompConnectHeaders(Profile.UserName, Passcode, BearerToken, Headers, Error))
			return false;
		const bool bChanged = LegacyProfile.BrokerUrl != Profile.BrokerUrl || LegacyProfile.UserName != Profile.UserName || SessionPasscode != Passcode || SessionBearerToken != BearerToken || LegacyProfile.AckTopic != Profile.AckTopic;
		const bool bOtherOwner = Owners.Num() > (Owners.Contains(Owner) ? 1 : 0);
		if (bChanged && bOtherOwner)
		{
			Error = TEXT("공유 sensor 채널 설정이 다릅니다. 기존 연결을 바꾸지 않고 준비를 거절했습니다.");
			return false;
		}
		if (bChanged)
		{
			if (!bConfiguredAck)
				ClearAckSubscription();
			Disconnect();
		}
		LegacyProfile = Profile;
		SessionPasscode = Passcode;
		SessionBearerToken = BearerToken;
	}
	Owners.Add(Owner);
	return true;
}

void UDxDeviceSensorStompChannel::ReleaseOwner(UObject* Owner)
{
	Owners.Remove(Owner);
	PruneOwners();
	if (Owners.IsEmpty() && !(bExplicitSettings && GetChannelSettings().bAutoConnect))
	{
		if (!bConfiguredAck)
			ClearAckSubscription();
		Disconnect();
		SessionPasscode.Reset();
		SessionBearerToken.Reset();
	}
}

bool UDxDeviceSensorStompChannel::Connect()
{
	check(IsInGameThread());
	if (IsShutdown())
		return false;
	if (IsConnected() || GetState() == EDxDataChannelState::Connecting)
		return true;
	return Super::Connect();
}

bool UDxDeviceSensorStompChannel::IsConnected() const
{
	const auto Client = GetStompClient();
	return Client && Client->IsConnected() && GetState() == EDxDataChannelState::Connected;
}

TSharedPtr<IStompClient> UDxDeviceSensorStompChannel::CreateStompClient(const FString& Url)
{
	const FString EffectiveUrl = bExplicitSettings ? Url : LegacyProfile.BrokerUrl;
	if (EffectiveUrl.IsEmpty())
		return nullptr;
	TSharedPtr<IStompClient> Client;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
	Client = TestClient;
#endif
	if (!Client)
		Client = FStompModule::Get().CreateClient(EffectiveUrl, bExplicitSettings ? FString() : SessionBearerToken);
	const uint64 SendGeneration = GetGeneration();
	TWeakObjectPtr<UDxDeviceSensorStompChannel> WeakThis(this);
	Client->OnConnected().AddWeakLambda(this, [WeakThis, SendGeneration](const FString& Protocol, const FString& Session, const FString& Server)
	                                    { AsyncTask(ENamedThreads::GameThread, [WeakThis, SendGeneration, Protocol, Session, Server]()
		                                            {
			if (auto* Self = WeakThis.Get(); Self && Self->IsCurrentGeneration(SendGeneration))
			{
				Self->ProtocolVersion = Protocol;
				Self->SessionId = Session;
				Self->ServerString = Server;
			} }); });
	if (bExplicitSettings)
		return Client;
	FStompHeader Headers;
	FString Error;
	if (!UVirtualSensorTransportComp::BuildStompConnectHeaders(LegacyProfile.UserName, SessionPasscode, SessionBearerToken, Headers, Error))
	{
		SetLastError(Error);
		return nullptr;
	}
	return MakeShared<FDeviceStompSessionClient>(Client, MoveTemp(Headers));
}

void UDxDeviceSensorStompChannel::HandleTransportConnected()
{
	Super::HandleTransportConnected();
	Connected.Broadcast(ProtocolVersion, SessionId, ServerString);
}

void UDxDeviceSensorStompChannel::HandleTransportClosed(const FString& Reason)
{
	if (!bConfiguredAck)
		ClearAckSubscription();
	Super::HandleTransportClosed(Reason);
	Failed.Broadcast(Reason);
}

void UDxDeviceSensorStompChannel::HandleTransportError(const FString& Error)
{
	Super::HandleTransportError(Error);
	Failed.Broadcast(Error);
}

bool UDxDeviceSensorStompChannel::Send(const FString& Destination, const FString& Body, const FStompHeader& Headers, const FStompRequestCompleted& Callback)
{
	FTCHARToUTF8 Convert(*Body);
	FStompBuffer Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Convert.Get()), Convert.Length());
	return Send(Destination, Bytes, Headers, Callback);
}

bool UDxDeviceSensorStompChannel::Send(const FString& Destination, const FStompBuffer& Body, const FStompHeader& Headers, const FStompRequestCompleted& Callback)
{
	check(IsInGameThread());
	if (IsShutdown() || !IsConnected())
	{
		RecordDropped();
		return false;
	}
	if (!CheckMessageSize(Body.Num()))
		return false;
	FReceiptHandler OnReceipt;
	if (Callback.IsBound())
	{
		struct FPending
		{
			bool bReported = false;
			FTimerHandle Timeout;
		};
		auto Pending = MakeShared<FPending>();
		TWeakObjectPtr<UDxDeviceSensorStompChannel> WeakThis(this);
		const uint64 SendGeneration = GetGeneration();
		auto Complete = [WeakThis, SendGeneration, Pending, Callback](bool bSuccess, const FString& Error)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, SendGeneration, Pending, Callback, bSuccess, Error]()
			          {
				auto* Self = WeakThis.Get();
				if (!Self || !Self->IsCurrentGeneration(SendGeneration) || Pending->bReported)
					return;
				Pending->bReported = true;
				if (auto* Game = Self->GetOwningGameInstance())
					Game->GetTimerManager().ClearTimer(Pending->Timeout);
				if (!bSuccess)
					Self->SetLastError(Error);
				Callback.ExecuteIfBound(bSuccess, Error); });
		};
		const float Timeout = GetChannelSettings().ReceiptTimeoutSeconds;
		if (Timeout > 0 && GetOwningGameInstance())
			GetOwningGameInstance()->GetTimerManager().SetTimer(Pending->Timeout, FTimerDelegate::CreateWeakLambda(this, [Complete]()
			                                                                                                       { Complete(false, TEXT("Receipt timeout")); }),
			                                                    Timeout, false);
		OnReceipt = MoveTemp(Complete);
	}
	if (!SendFrame(Destination, Body, Headers, MoveTemp(OnReceipt)))
	{
		RecordDropped();
		return false;
	}
	RecordPublished();
	return true;
}

FString UDxDeviceSensorStompChannel::SubscribeToAck(const FString& Topic)
{
	if (Topic.IsEmpty())
		return FString();
	if (AckTopic == Topic && !AckKey.IsEmpty())
		return AckKey;
	ClearAckSubscription();
	AckTopic = Topic;
	FSTOMPSubscriptionEvent Event;
	Event.BindDynamic(this, &UDxDeviceSensorStompChannel::HandleAckFrame);
	FSTOMPRequestCompleted Completion;
	Completion.BindDynamic(this, &UDxDeviceSensorStompChannel::HandleAckSubscribed);
	AckKey = Subscribe(Topic, Event, Completion);
	return AckKey;
}

void UDxDeviceSensorStompChannel::ClearAckSubscription()
{
	if (!AckKey.IsEmpty())
		Unsubscribe(AckKey, FSTOMPRequestCompleted());
	AckKey.Reset();
	AckTopic.Reset();
}

void UDxDeviceSensorStompChannel::HandleAckFrame(const FWebSocketMessage& Message)
{
	AckReceived.Broadcast(Message);
}

void UDxDeviceSensorStompChannel::HandleAckSubscribed(bool bSuccess, FString Error)
{
	AckSubscribed.Broadcast(bSuccess, Error);
}

void UDxDeviceSensorStompChannel::OnChannelShutdown()
{
	Super::OnChannelShutdown();
	Owners.Empty();
	SessionPasscode.Reset();
	SessionBearerToken.Reset();
	AckKey.Reset();
	AckTopic.Reset();
	Connected.Clear();
	Failed.Clear();
	AckReceived.Clear();
	AckSubscribed.Clear();
}
