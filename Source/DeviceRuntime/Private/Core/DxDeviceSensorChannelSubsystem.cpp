#include "Device/Core/DxDeviceSensorChannelSubsystem.h"

#include "Device/Core/DxDeviceSensorBulkChannel.h"
#include "Core/DTCoreSettings.h"
#include "Core/DxStompChannel.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UDxDeviceSensorChannelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UDxWebSocketSubsystem>();
	Super::Initialize(Collection);
	auto* Owner = GetGameInstance()->GetSubsystem<UDxWebSocketSubsystem>();
	Socket = Owner;
	if (!Owner)
		return;
	const FName Name(TEXT("sensor-bulk"));
	FDxDataChannelSettings Settings;
	Settings.ChannelName = Name;
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	const auto* Explicit = GetDefault<UDTCoreSettings>()->DataChannels.FindByPredicate([Name](const auto& Item)
	                                                                                   { return Item.ChannelName == Name; });
	if (Explicit)
		Settings = *Explicit;
	if (auto* Existing = Owner->FindChannel(Name))
	{
		Bulk = Cast<UDxDeviceSensorBulkChannel>(Existing);
		if (Bulk)
		{
			Bulk->SetExplicitSettings(Explicit != nullptr);
			return;
		}
		// DTCore는 비어 있는 ChannelClass를 generic STOMP로 생성한다. 이 예약 이름만 교체한다.
		if (Existing->GetClass() != UDxStompChannel::StaticClass())
		{
			UE_LOG(LogTemp, Error, TEXT("[DeviceDataChannel] sensor-bulk is registered with an incompatible channel class."));
			return;
		}
		Owner->UnregisterChannel(Name);
	}
	Bulk = NewObject<UDxDeviceSensorBulkChannel>(this);
	Bulk->SetExplicitSettings(Explicit != nullptr);
	Settings.ChannelClass = UDxDeviceSensorBulkChannel::StaticClass();
	if (!Owner->RegisterChannelWithSettings(Bulk, Settings))
		Bulk = nullptr;
}

void UDxDeviceSensorChannelSubsystem::Deinitialize()
{
	if (auto* Owner = Socket.Get(); Owner && Bulk && Owner->FindChannel(TEXT("sensor-bulk")) == Bulk)
		Owner->UnregisterChannel(TEXT("sensor-bulk"));
	Bulk = nullptr;
	Socket.Reset();
	Super::Deinitialize();
}

UDxDeviceSensorBulkChannel* UDxDeviceSensorChannelSubsystem::FindBulk(const UWorld* World)
{
	auto* Game = World ? World->GetGameInstance() : nullptr;
	auto* Registry = Game ? Game->GetSubsystem<UDxDeviceSensorChannelSubsystem>() : nullptr;
	return Registry ? Registry->Bulk.Get() : nullptr;
}
