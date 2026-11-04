#include "DeviceDataChannelConfigProbe.h"

#if !UE_BUILD_SHIPPING
#include "Device/Core/DeviceDataChannelSettings.h"
#include "Device/Core/DxDeviceSensorChannelSubsystem.h"
#include "Device/Core/DxDeviceSensorBulkChannel.h"
#include "Device/Core/DxDeviceSensorStompChannel.h"
#include "Core/DTCoreSettings.h"
#include "Core/DxDataChannelBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/FileManager.h"

namespace
{
FString SafeProbeUrl(const FString& Url)
{
	return (Url.StartsWith(TEXT("ws://127.0.0.1:")) || Url.StartsWith(TEXT("tcp://127.0.0.1:"))) && !Url.Contains(TEXT("@")) ? Url : TEXT("[configured/redacted]");
}

void WriteConfigProbe()
{
	auto Report = MakeShared<FJsonObject>();
	const auto* Topics = GetDefault<UDeviceDataChannelSettings>();
	Report->SetStringField(TEXT("platform"), FPlatformProperties::IniPlatformName());
	Report->SetStringField(TEXT("default_url"), SafeProbeUrl(GetDefault<UDTCoreSettings>()->WebSocketUrl));
	TArray<TSharedPtr<FJsonValue>> Rows;
	UDxWebSocketSubsystem* Socket = nullptr;
	UDxDeviceSensorChannelSubsystem* Registry = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (UGameInstance* Game = Context.OwningGameInstance)
		{
			Socket = Game->GetSubsystem<UDxWebSocketSubsystem>();
			Registry = Game->GetSubsystem<UDxDeviceSensorChannelSubsystem>();
			if (Registry)
				break;
		}
	Report->SetBoolField(TEXT("registry_ready"), Registry != nullptr);
	for (FName Name : {FName(TEXT("sensor")), FName(TEXT("sensor-bulk")), FName(TEXT("Default"))})
	{
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("name"), Name.ToString());
		if (auto* Channel = Socket ? Socket->FindChannel(Name) : nullptr)
		{
			Row->SetStringField(TEXT("url"), SafeProbeUrl(Channel->GetChannelSettings().Url));
			Row->SetBoolField(TEXT("auto_connect"), Channel->GetChannelSettings().bAutoConnect);
		}
		Row->SetStringField(TEXT("camera"), Topics->Resolve(Name, TEXT("camera"), VirtualSensorDefaults::CameraTopic));
		Row->SetStringField(TEXT("lidar"), Topics->Resolve(Name, TEXT("lidar"), VirtualSensorDefaults::LidarTopic));
		Row->SetStringField(TEXT("pointcloud"), Topics->Resolve(Name, TEXT("pointcloud"), VirtualSensorDefaults::PointCloudTopic));
		Row->SetNumberField(TEXT("subscriptions"), Topics->GetSubscriptions(Name).Num());
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Report->SetArrayField(TEXT("channels"), Rows);
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Reports/device-channel-config.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FPlatformMisc::RequestExit(false);
}
}
#endif

void RegisterDeviceDataChannelConfigProbe()
{
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("DeviceChannelConfigProbe")))
		FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(&WriteConfigProbe);
#endif
}
