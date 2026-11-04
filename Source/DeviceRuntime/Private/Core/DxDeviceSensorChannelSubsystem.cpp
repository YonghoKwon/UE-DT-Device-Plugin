#include "Device/Core/DxDeviceSensorChannelSubsystem.h"

#include "Device/Core/DxDeviceSensorBulkChannel.h"
#include "Device/Core/DxDeviceSensorStompChannel.h"
#include "Device/Core/DeviceDataChannelSettings.h"
#include "Core/DTCoreSettings.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
template <typename T>
T* RegisterSensorChannel(UDxWebSocketSubsystem& Socket, UObject* Outer, FName Name)
{
	FDxDataChannelSettings Settings;
	Settings.ChannelName = Name;
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	const auto* Explicit = GetDefault<UDTCoreSettings>()->DataChannels.FindByPredicate([Name](const auto& Item)
	                                                                                   { return Item.ChannelName == Name; });
	if (Explicit)
		Settings = *Explicit;
	if (auto* Existing = Socket.FindChannel(Name))
	{
		if (auto* Typed = Cast<T>(Existing))
		{
			Typed->SetExplicitSettings(Explicit != nullptr);
			return Typed;
		}
		// Empty ChannelClass is provisionally auto-created by DTCore as generic STOMP.
		if (Existing->GetClass() != UDxStompChannel::StaticClass())
		{
			UE_LOG(LogTemp, Error, TEXT("[DeviceDataChannel] %s is registered with an incompatible channel class."), *Name.ToString());
			return nullptr;
		}
		Socket.UnregisterChannel(Name);
	}
	auto* Channel = NewObject<T>(Outer);
	Channel->SetExplicitSettings(Explicit != nullptr);
	Settings.ChannelClass = T::StaticClass();
	return Socket.RegisterChannelWithSettings(Channel, Settings) ? Channel : nullptr;
}
}

void UDxDeviceSensorChannelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UDxWebSocketSubsystem>();
	Super::Initialize(Collection);
	auto* Owner = GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>();
	Socket = Owner;
	if (!Owner)
		return;
	Bulk = RegisterSensorChannel<UDxDeviceSensorBulkChannel>(*Owner, this, TEXT("sensor-bulk"));
	Sensor = RegisterSensorChannel<UDxDeviceSensorStompChannel>(*Owner, this, TEXT("sensor"));
	ConfigureTopicSubscriptions(*GetDefault<UDeviceDataChannelSettings>());
}

void UDxDeviceSensorChannelSubsystem::Deinitialize()
{
	for (UDeviceDataTopicSubscription* Subscription : TopicSubscriptions)
		if (auto* Channel = Subscription->Channel.Get())
		{
			Channel->OnStateChanged.RemoveDynamic(this, &UDxDeviceSensorChannelSubsystem::HandleChannelState);
			if (!Subscription->Key.IsEmpty())
				Channel->Unsubscribe(Subscription->Key, FSTOMPRequestCompleted());
		}
	TopicSubscriptions.Empty();
	OnTopicMessage.Clear();
	if (auto* Owner = Socket.Get())
	{
		if (auto* Default = Owner->FindChannel(TEXT("Default")))
			Default->OnStateChanged.RemoveDynamic(this, &UDxDeviceSensorChannelSubsystem::HandleChannelState);
		if (Bulk && Owner->FindChannel(TEXT("sensor-bulk")) == Bulk)
			Owner->UnregisterChannel(TEXT("sensor-bulk"));
		if (Sensor && Owner->FindChannel(TEXT("sensor")) == Sensor)
			Owner->UnregisterChannel(TEXT("sensor"));
	}
	Bulk = nullptr;
	Sensor = nullptr;
	Socket.Reset();
	Super::Deinitialize();
}

void UDeviceDataTopicSubscription::Issue()
{
	if (Key.IsEmpty())
		if (auto* Target = Channel.Get())
		{
			FSTOMPSubscriptionEvent Event;
			Event.BindDynamic(this, &UDeviceDataTopicSubscription::Receive);
			Key = Target->Subscribe(Destination, Event, FSTOMPRequestCompleted());
		}
}

void UDeviceDataTopicSubscription::Receive(const FWebSocketMessage& Message)
{
	if (auto* Registry = Owner.Get())
		Registry->OnTopicMessage.Broadcast(ChannelName, Alias, Message);
}

void UDeviceDataTopicSubscription::ObserveSharedState(EDxDataChannelState State)
{
	if (State == EDxDataChannelState::Connected)
		Issue();
	else
		Key.Reset(); // Shared subscription IDs belong to the previous connection generation.
}

void UDxDeviceSensorChannelSubsystem::ConfigureTopicSubscriptions(const UDeviceDataChannelSettings& Settings)
{
	check(IsInGameThread());
	for (UDeviceDataTopicSubscription* Subscription : TopicSubscriptions)
		if (auto* Channel = Subscription->Channel.Get())
		{
			Channel->OnStateChanged.RemoveDynamic(this, &UDxDeviceSensorChannelSubsystem::HandleChannelState);
			if (!Subscription->Key.IsEmpty())
				Channel->Unsubscribe(Subscription->Key, FSTOMPRequestCompleted());
		}
	TopicSubscriptions.Empty();
	TopicConfigurationWarning = FString::Join(Settings.GetDuplicateSubscriptionWarnings(), TEXT("\n"));
	FString Error;
	if (!Settings.Validate(Error))
	{
		TopicConfigurationWarning = Error;
		UE_LOG(LogTemp, Error, TEXT("[DeviceDataChannel] %s"), *Error);
		return;
	}
	if (!TopicConfigurationWarning.IsEmpty())
		UE_LOG(LogTemp, Warning, TEXT("[DeviceDataChannel] %s"), *TopicConfigurationWarning);
	if (auto* SocketOwner = Socket.Get())
		for (FName Name : {FName(TEXT("sensor")), FName(TEXT("Default"))})
			if (auto* Channel = SocketOwner->FindChannel(Name))
			{
				const auto Destinations = Settings.GetSubscriptions(Name);
				if (!Destinations.IsEmpty())
					Channel->OnStateChanged.AddUniqueDynamic(this, &UDxDeviceSensorChannelSubsystem::HandleChannelState);
				for (const auto& Destination : Destinations)
				{
					const auto* Ack = Settings.FindDestination(Name, TEXT("ack"));
					if (Name == TEXT("sensor") && Ack && Ack->bSubscribe && Ack->Destination == Destination)
						continue; // Reuse the channel's existing ACK callback/key.
					auto* Subscription = NewObject<UDeviceDataTopicSubscription>(this);
					Subscription->Owner = this;
					Subscription->Channel = Channel;
					Subscription->ChannelName = Name;
					Subscription->Destination = Destination;
					for (const auto& Topic : Settings.FindTopics(Name)->Destinations)
						if (Topic.Destination == Destination && Topic.bSubscribe)
						{
							Subscription->Alias = Topic.Alias;
							break;
						}
					TopicSubscriptions.Add(Subscription);
					// Default wraps the existing shared connection and cannot queue interests while disconnected.
					if (Name != TEXT("Default") || Channel->GetState() == EDxDataChannelState::Connected)
						Subscription->Issue();
				}
			}
}

void UDxDeviceSensorChannelSubsystem::HandleChannelState(FName ChannelName, EDxDataChannelState State, FString)
{
	if (ChannelName != TEXT("Default"))
		return; // UDxStompChannel retains and reissues all sensor interests itself.
	for (UDeviceDataTopicSubscription* Subscription : TopicSubscriptions)
		if (Subscription->ChannelName == ChannelName)
			Subscription->ObserveSharedState(State);
}

UDxDeviceSensorBulkChannel* UDxDeviceSensorChannelSubsystem::FindBulk(const UWorld* World)
{
	auto* Game = World ? World->GetGameInstance() : nullptr;
	auto* Registry = Game ? Game->GetSubsystem<UDxDeviceSensorChannelSubsystem>() : nullptr;
	return Registry ? Registry->Bulk.Get() : nullptr;
}

UDxDeviceSensorStompChannel* UDxDeviceSensorChannelSubsystem::FindStomp(const UWorld* World)
{
	auto* Game = World ? World->GetGameInstance() : nullptr;
	auto* Registry = Game ? Game->GetSubsystem<UDxDeviceSensorChannelSubsystem>() : nullptr;
	return Registry ? Registry->Sensor.Get() : nullptr;
}
