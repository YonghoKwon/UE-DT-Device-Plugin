#include "Device/Core/DxDeviceSensorChannelSubsystem.h"

#include "Device/Core/DxDeviceSensorBulkChannel.h"
#include "Device/Core/DxDeviceSensorStompChannel.h"
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
}

void UDxDeviceSensorChannelSubsystem::Deinitialize()
{
	if (auto* Owner = Socket.Get())
	{
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
