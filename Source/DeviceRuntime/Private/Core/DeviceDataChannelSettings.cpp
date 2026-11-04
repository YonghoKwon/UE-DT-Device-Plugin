#include "Device/Core/DeviceDataChannelSettings.h"

const FDeviceDataChannelTopics* UDeviceDataChannelSettings::FindTopics(FName ChannelName) const
{
	return Channels.FindByPredicate([ChannelName](const auto& Item)
	                                { return Item.ChannelName == ChannelName; });
}

const FDeviceDataDestination* UDeviceDataChannelSettings::FindDestination(FName ChannelName, FName Alias) const
{
	if (const auto* Topics = FindTopics(ChannelName))
		return Topics->Destinations.FindByPredicate([Alias](const auto& Item)
		                                            { return Item.Alias == Alias; });
	return nullptr;
}

FString UDeviceDataChannelSettings::Resolve(FName ChannelName, FName Alias, const FString& LegacyDestination) const
{
	if (const auto* Topic = FindDestination(ChannelName, Alias); Topic && !Topic->Destination.IsEmpty())
		return Topic->Destination;
	return LegacyDestination;
}

FString UDeviceDataChannelSettings::ResolveSubscription(FName ChannelName, FName Alias, const FString& LegacyDestination) const
{
	if (const auto* Topic = FindDestination(ChannelName, Alias); Topic && !Topic->Destination.IsEmpty())
		return Topic->bSubscribe ? Topic->Destination : FString();
	return LegacyDestination;
}

TArray<FString> UDeviceDataChannelSettings::GetSubscriptions(FName ChannelName) const
{
	TArray<FString> Result;
	if (const auto* Topics = FindTopics(ChannelName))
		for (const auto& Topic : Topics->Destinations)
			if (Topic.bSubscribe && !Topic.Destination.IsEmpty())
				Result.AddUnique(Topic.Destination);
	return Result;
}

TArray<FString> UDeviceDataChannelSettings::GetDuplicateSubscriptionWarnings() const
{
	TArray<FString> Result;
	TMap<FString, FName> Owners;
	for (const auto& Channel : Channels)
		for (const auto& Destination : GetSubscriptions(Channel.ChannelName))
		{
			if (const auto* Previous = Owners.Find(Destination); Previous && *Previous != Channel.ChannelName)
				Result.Add(FString::Printf(TEXT("Duplicate topic subscription: %s (%s / %s)"), *Destination, *Previous->ToString(), *Channel.ChannelName.ToString()));
			else
				Owners.Add(Destination, Channel.ChannelName);
		}
	return Result;
}

bool UDeviceDataChannelSettings::Validate(FString& Error) const
{
	Error.Reset();
	TSet<FName> Names;
	for (const auto& Channel : Channels)
	{
		if ((Channel.ChannelName != TEXT("sensor") && Channel.ChannelName != TEXT("sensor-bulk") && Channel.ChannelName != TEXT("Default")) || Names.Contains(Channel.ChannelName))
		{
			Error = TEXT("Device topic channels must be unique: sensor, sensor-bulk, Default.");
			return false;
		}
		Names.Add(Channel.ChannelName);
		TSet<FName> Aliases;
		for (const auto& Topic : Channel.Destinations)
		{
			if (Topic.Alias.IsNone() || Aliases.Contains(Topic.Alias) || Topic.Destination.Contains(TEXT("\n")) || Topic.Destination.Contains(TEXT("\r")))
			{
				Error = TEXT("Device topic aliases must be unique/nonempty; destinations cannot contain line breaks.");
				return false;
			}
			Aliases.Add(Topic.Alias);
		}
	}
	return true;
}
