#include "DeviceDataChannelLooseConfig.h"

#include "Device/Core/DeviceDataChannelSettings.h"
#include "Core/DTCoreSettings.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
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
	// Bypass pak precedence only for explicit deployment files. Never write them.
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
		// A later file edits an earlier row of the same channel instead of adding a shadowed duplicate.
		Result.RemoveAll([&Row](const T& Existing)
		                 { return Existing.ChannelName == Row.ChannelName; });
		Result.Add(MoveTemp(Row));
	}
	return true;
}

// FilterProjectOverrides output only: does this file set the array key in the section?
bool MentionsKey(const FString& Filtered, const TCHAR* Section, const TCHAR* Key)
{
	FString Current;
	TArray<FString> Lines;
	Filtered.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("[")))
		{
			Current = Line.Mid(1, Line.Len() - 2);
			continue;
		}
		FString Name, Value;
		if (Current == Section && Line.Split(TEXT("="), &Name, &Value))
		{
			Name.TrimStartAndEndInline();
			if (Name.StartsWith(TEXT("+")) || Name.StartsWith(TEXT("!")) || Name.StartsWith(TEXT("-")) || Name.StartsWith(TEXT(".")))
				Name.RightChopInline(1);
			if (Name == Key)
				return true;
		}
	}
	return false;
}
}

namespace
{
struct FChannelConfigLayer
{
	FString Text;
	bool bReplaceLists = false;
};

bool ApplyLayers(const TArray<FChannelConfigLayer>& Layers, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics)
{
	// Local ownership only. Never copy, dirty or flush GConfig.
	FConfigFile Combined;
	Combined.NoSave = true;
	SeedRows(Combined, CoreSection, TEXT("DataChannels"), Connections.DataChannels);
	SeedRows(Combined, TopicSection, TEXT("Channels"), Topics.Channels);
	for (const auto& Layer : Layers)
	{
		const FString Values = DeviceDataChannelConfig::FilterProjectOverrides(Layer.Text);
		if (Layer.bReplaceLists)
		{
			if (MentionsKey(Values, CoreSection, TEXT("DataChannels")))
				Combined.FindOrAddSection(CoreSection)->Remove(TEXT("DataChannels"));
			if (MentionsKey(Values, TopicSection, TEXT("Channels")))
				Combined.FindOrAddSection(TopicSection)->Remove(TEXT("Channels"));
		}
		Combined.CombineFromBuffer(Values, TEXT("loose channel policy"));
	}
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

}

bool DeviceDataChannelConfig::ApplyBuffers(const FString& Common, const FString& Platform, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics)
{
	return ApplyLayers({{Common, true}, {Platform, false}}, Connections, Topics);
}

TArray<FString> DeviceDataChannelConfig::RuntimeCandidatePaths(const FString& ProjectDir, const FString& LaunchDir, const FString& ProjectName, const FString& Platform, bool bPlatform)
{
	TArray<FString> Result;
	const FString File = bPlatform ? Platform / (Platform + TEXT("Game.ini")) : TEXT("Game.ini");
	for (const FString& Root : {ProjectDir / TEXT("Config"), LaunchDir / TEXT("Config"), LaunchDir / TEXT("../../Config"), LaunchDir / ProjectName / TEXT("Config")})
	{
		FString Path = FPaths::ConvertRelativePathToFull(Root / File);
		FPaths::NormalizeFilename(Path);
		FPaths::CollapseRelativeDirectories(Path);
		Result.AddUnique(Path);
	}
	return Result;
}

bool DeviceDataChannelConfig::ApplyRuntimeBuffers(const FString& DefaultGame, const TArray<FString>& CommonByPriority, const TArray<FString>& PlatformByPriority, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics)
{
	TArray<FChannelConfigLayer> Layers{{DefaultGame, true}};
	// Reverse the candidate order when applying: first candidate has highest priority.
	// Platform lists retain their existing partial-row override behavior.
	for (int32 I = PlatformByPriority.Num() - 1; I >= 0; --I)
		Layers.Add({PlatformByPriority[I], false});
	for (int32 I = CommonByPriority.Num() - 1; I >= 0; --I)
		Layers.Add({CommonByPriority[I], true});
	return ApplyLayers(Layers, Connections, Topics);
}

void DeviceDataChannelConfig::ApplyLoosePackageOverrides()
{
	if (!FPlatformProperties::RequiresCookedData())
		return;
	const FString Platform = FPlatformProperties::IniPlatformName();
	FString DefaultGame;
	ReadPhysicalConfig(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()) / TEXT("DefaultGame.ini"), DefaultGame);
	TArray<FString> Common, PlatformText;
	for (bool bPlatform : {false, true})
		for (const auto& Path : RuntimeCandidatePaths(FPaths::ProjectDir(), FPaths::LaunchDir(), FApp::GetProjectName(), Platform, bPlatform))
		{
			FString Text;
			ReadPhysicalConfig(Path, Text);
			(bPlatform ? PlatformText : Common).Add(MoveTemp(Text));
		}
	const auto HasPolicy = [](const FString& Text)
	{
		return !FilterProjectOverrides(Text).IsEmpty();
	};
	if (!HasPolicy(DefaultGame) && !Common.ContainsByPredicate(HasPolicy) && !PlatformText.ContainsByPredicate(HasPolicy))
		return;
	if (ApplyRuntimeBuffers(DefaultGame, Common, PlatformText, *GetMutableDefault<UDTCoreSettings>(), *GetMutableDefault<UDeviceDataChannelSettings>()))
	{
		UE_LOG(LogTemp, Display, TEXT("[DeviceDataChannel] Applied project loose connection/topic policy; credentials, unrelated settings and global config cache unchanged."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DeviceDataChannel] Invalid loose channel policy; existing settings retained."));
	}
}
