#include "DeviceDataChannelLooseConfig.h"

#include "Device/Core/DeviceDataChannelSettings.h"
#include "Core/DTCoreSettings.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

namespace
{
const TCHAR* CoreSection = TEXT("/Script/DTCore.DTCoreSettings");
const TCHAR* TopicSection = TEXT("/Script/DeviceRuntime.DeviceDataChannelSettings");
const TArray<FName>& ConnectionProperties()
{
	static const TArray<FName> Properties{TEXT("DataChannels"), TEXT("WebSocketUrl"), TEXT("MaxReconnectAttempts"), TEXT("ReconnectInitialDelaySeconds"), TEXT("ReconnectMaxDelaySeconds"), TEXT("ReconnectBackoffMultiplier"), TEXT("SubscribeReceiptTimeoutSeconds")};
	return Properties;
}

bool ReadPhysicalConfig(const FString& Path, FString& Text)
{
	// Bypass pak precedence only for the two explicit deployment files. Never write them.
	TUniquePtr<IFileHandle> File(IPlatformFile::GetPlatformPhysical().OpenRead(*Path));
	if (!File)
		return false;
	const int64 Size = File->Size();
	if (Size <= 0 || Size > 2 * 1024 * 1024)
		return false;
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(static_cast<int32>(Size));
	if (!File->Read(Bytes.GetData(), Size))
		return false;
	FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
	return true;
}
}

FString DeviceDataChannelConfig::FilterProjectOverrides(const FString& Text)
{
	FString Result, Section;
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	bool bHasValue = false;
	for (FString Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
		{
			Section = Line.Mid(1, Line.Len() - 2);
			if (Section == CoreSection || Section == TopicSection)
				Result += Line + TEXT("\n");
			continue;
		}
		FString Key, Value;
		if (Line.StartsWith(TEXT(";")) || !Line.Split(TEXT("="), &Key, &Value))
			continue;
		Key.TrimStartAndEndInline();
		if (Key.StartsWith(TEXT("+")) || Key.StartsWith(TEXT("!")) || Key.StartsWith(TEXT("-")) || Key.StartsWith(TEXT(".")))
			Key.RightChopInline(1);
		if ((Section == TopicSection && Key == TEXT("Channels")) || (Section == CoreSection && ConnectionProperties().Contains(FName(*Key))))
		{
			Result += Line + TEXT("\n");
			bHasValue = true;
		}
	}
	return bHasValue ? Result : FString();
}

void DeviceDataChannelConfig::ApplyLoosePackageOverrides()
{
	if (!FPlatformProperties::RequiresCookedData() || !GConfig)
		return;
	auto* Connections = GetMutableDefault<UDTCoreSettings>();
	auto* Topics = GetMutableDefault<UDeviceDataChannelSettings>();
	auto* Game = GConfig->FindConfigFile(GGameIni);
	if (!Game)
		return;
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
	const FString Platform = FPlatformProperties::IniPlatformName();
	FConfigFile Original = *Game;
	int32 Count = 0;
	for (const FString& Path : {Root / TEXT("DefaultGame.ini"), Root / Platform / (Platform + TEXT("Game.ini"))})
	{
		FString Text;
		if (ReadPhysicalConfig(Path, Text))
		{
			const FString Filtered = FilterProjectOverrides(Text);
			if (!Filtered.IsEmpty())
			{
				Game->CombineFromBuffer(Filtered, Path);
				++Count;
			}
		}
	}
	if (Count > 0)
	{
		Game->NoSave = true;
		for (FName Name : ConnectionProperties())
			if (auto* Property = FindFProperty<FProperty>(UDTCoreSettings::StaticClass(), Name))
				Connections->ReloadConfig(UDTCoreSettings::StaticClass(), *GGameIni, UE::LCPF_None, Property);
		if (auto* Property = FindFProperty<FProperty>(UDeviceDataChannelSettings::StaticClass(), TEXT("Channels")))
			Topics->ReloadConfig(UDeviceDataChannelSettings::StaticClass(), *GGameIni, UE::LCPF_None, Property);
		UE_LOG(LogTemp, Display, TEXT("[DeviceDataChannel] Applied project loose connection/topic policy (%d files); credentials and unrelated settings unchanged."), Count);
	}
	// Only the two settings objects retain the selected values. Do not dirty/save the config cache.
	*Game = MoveTemp(Original);
}
