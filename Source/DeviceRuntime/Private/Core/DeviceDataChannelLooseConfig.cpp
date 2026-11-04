#include "DeviceDataChannelLooseConfig.h"

#include "Device/Core/DeviceDataChannelSettings.h"
#include "Core/DTCoreSettings.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "Misc/OutputDeviceNull.h"

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

namespace
{
template <typename T>
void SeedRows(FConfigFile& File, const TCHAR* Section, const TCHAR* Key, const TArray<T>& Rows)
{
	for (const auto& Row : Rows)
	{
		FString Text;
		T::StaticStruct()->ExportText(Text, &Row, nullptr, nullptr, PPF_None, nullptr);
		File.FindOrAddSection(Section)->Add(FName(Key), FConfigValue(Text));
	}
}

template <typename T>
bool ReadRows(const FConfigFile& File, const TCHAR* Section, const TCHAR* Key, TArray<T>& Result)
{
	TArray<FString> Rows;
	File.GetArray(Section, Key, Rows);
	FOutputDeviceNull Errors;
	for (const auto& Text : Rows)
	{
		T Row;
		if (!T::StaticStruct()->ImportText(*Text, &Row, nullptr, PPF_None, &Errors, T::StaticStruct()->GetName()))
			return false;
		Result.Add(MoveTemp(Row));
	}
	return true;
}
}

bool DeviceDataChannelConfig::ApplyBuffers(const FString& Common, const FString& Platform, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics)
{
	const FString CommonValues = FilterProjectOverrides(Common);
	const FString PlatformValues = FilterProjectOverrides(Platform);
	if (CommonValues.IsEmpty() && PlatformValues.IsEmpty())
		return true;
	// This local file owns its data. Never copy, replace, dirty or flush GConfig's files.
	FConfigFile Combined;
	Combined.NoSave = true;
	SeedRows(Combined, CoreSection, TEXT("DataChannels"), Connections.DataChannels);
	SeedRows(Combined, TopicSection, TEXT("Channels"), Topics.Channels);
	Combined.CombineFromBuffer(CommonValues, TEXT("loose common channel policy"));
	Combined.CombineFromBuffer(PlatformValues, TEXT("loose platform channel policy"));
	TArray<FDxDataChannelSettings> NewConnections;
	TArray<FDeviceDataChannelTopics> NewTopics;
	if (!ReadRows(Combined, CoreSection, TEXT("DataChannels"), NewConnections) || !ReadRows(Combined, TopicSection, TEXT("Channels"), NewTopics))
		return false;
	Swap(Topics.Channels, NewTopics);
	FString Error;
	if (!Topics.Validate(Error))
	{
		Swap(Topics.Channels, NewTopics);
		return false;
	}
	Connections.DataChannels = MoveTemp(NewConnections);
	Combined.GetString(CoreSection, TEXT("WebSocketUrl"), Connections.WebSocketUrl);
	Combined.GetInt(CoreSection, TEXT("MaxReconnectAttempts"), Connections.MaxReconnectAttempts);
	Combined.GetFloat(CoreSection, TEXT("ReconnectInitialDelaySeconds"), Connections.ReconnectInitialDelaySeconds);
	Combined.GetFloat(CoreSection, TEXT("ReconnectMaxDelaySeconds"), Connections.ReconnectMaxDelaySeconds);
	Combined.GetFloat(CoreSection, TEXT("ReconnectBackoffMultiplier"), Connections.ReconnectBackoffMultiplier);
	Combined.GetFloat(CoreSection, TEXT("SubscribeReceiptTimeoutSeconds"), Connections.SubscribeReceiptTimeoutSeconds);
	return true;
}

void DeviceDataChannelConfig::ApplyLoosePackageOverrides()
{
	if (!FPlatformProperties::RequiresCookedData())
		return;
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
	const FString Platform = FPlatformProperties::IniPlatformName();
	FString Common, PlatformText;
	ReadPhysicalConfig(Root / TEXT("DefaultGame.ini"), Common);
	ReadPhysicalConfig(Root / Platform / (Platform + TEXT("Game.ini")), PlatformText);
	if (FilterProjectOverrides(Common).IsEmpty() && FilterProjectOverrides(PlatformText).IsEmpty())
		return;
	if (ApplyBuffers(Common, PlatformText, *GetMutableDefault<UDTCoreSettings>(), *GetMutableDefault<UDeviceDataChannelSettings>()))
	{
		UE_LOG(LogTemp, Display, TEXT("[DeviceDataChannel] Applied project loose connection/topic policy; credentials, unrelated settings and global config cache unchanged."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DeviceDataChannel] Invalid loose channel policy; existing settings retained."));
	}
}
