#include "Device/Core/DxDeviceSensorBulkChannel.h"

#include "Device/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Device/Core/DeviceDataChannelSettings.h"
#include "Core/DTCoreSettings.h"
#include "../Sensor/DeviceSensorTransportConfig.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UDxDeviceSensorBulkChannel::OnChannelInitialized()
{
	const FName Name = GetChannelName();
	bExplicitSettings = GetDefault<UDTCoreSettings>()->DataChannels.ContainsByPredicate([Name](const auto& Item)
	                                                                                    { return Item.ChannelName == Name; });
}

void UDxDeviceSensorBulkChannel::ResolveLegacyProfile(FVirtualSensorHighThroughputProfile& Profile, FString& Passcode) const
{
	const auto* Topics = GetDefault<UDeviceDataChannelSettings>();
	Profile.CameraTopic = Topics->Resolve(TEXT("sensor-bulk"), TEXT("camera"), Profile.CameraTopic);
	Profile.LidarTopic = Topics->Resolve(TEXT("sensor-bulk"), TEXT("lidar"), Profile.LidarTopic);
	Profile.PointCloudTopic = Topics->Resolve(TEXT("sensor-bulk"), TEXT("pointcloud"), Profile.PointCloudTopic);
	Profile.ProjectSubscriptionDestinations = Topics->GetSubscriptions(TEXT("sensor-bulk"));
	const auto& Settings = GetChannelSettings();
	if (!bExplicitSettings)
		return;
	Profile.BrokerUrl = ResolveUrl();
	const auto Credentials = ResolveCredentials();
	Profile.UserName = Credentials.FindRef(TEXT("login"));
	Passcode = Credentials.FindRef(TEXT("passcode"));
	if (Settings.ReconnectInitialDelaySeconds > 0)
		Profile.ReconnectInitialDelaySeconds = Settings.ReconnectInitialDelaySeconds;
	if (Settings.ReconnectMaxDelaySeconds > 0)
		Profile.ReconnectMaxDelaySeconds = Settings.ReconnectMaxDelaySeconds;
	if (Settings.ReconnectBackoffMultiplier > 0)
		Profile.ReconnectBackoffMultiplier = Settings.ReconnectBackoffMultiplier;
}

bool UDxDeviceSensorBulkChannel::BindWorld(UVirtualSensorHighThroughputTransportSubsystem& Source, FString& Error)
{
	if (IsShutdown())
	{
		Error = TEXT("sensor-bulk channel is shut down.");
		return false;
	}
	if (const auto* Previous = Transport.Get(); Previous && Previous != &Source && Previous->IsHighThroughputTransportRunning())
	{
		Error = TEXT("sensor-bulk is owned by another active World.");
		SetLastError(Error);
		return false;
	}
	Transport = &Source;
	Source.BulkChannel = this;
	return true;
}

bool UDxDeviceSensorBulkChannel::Prepare(UVirtualSensorHighThroughputTransportSubsystem& Source, const FVirtualSensorHighThroughputProfile& Profile, const FString& Passcode, FString& Error, bool bAllowIdleReplacement)
{
	check(IsInGameThread());
	if (!BindWorld(Source, Error))
		return false;
	auto EffectiveProfile = Profile;
	auto EffectivePasscode = Passcode;
	ResolveLegacyProfile(EffectiveProfile, EffectivePasscode);
	const bool bPrepared = Source.PrepareWorker(EffectiveProfile, EffectivePasscode, Error, bAllowIdleReplacement);
	if (!bPrepared)
		SetLastError(Error);
	else if (!Source.IsTransportConnected())
		UpdateState(EDxDataChannelState::Connecting, TEXT("Raw TCP worker preparing"));
	return bPrepared;
}

bool UDxDeviceSensorBulkChannel::Start(UVirtualSensorHighThroughputTransportSubsystem& Source, const FVirtualSensorHighThroughputProfile& Profile, const FString& Passcode)
{
	check(IsInGameThread());
	FString Error;
	if (!BindWorld(Source, Error))
		return false;
	auto EffectiveProfile = Profile;
	auto EffectivePasscode = Passcode;
	ResolveLegacyProfile(EffectiveProfile, EffectivePasscode);
	const bool bStarted = Source.StartWorker(EffectiveProfile, EffectivePasscode);
	if (bStarted && !Source.IsTransportConnected())
		UpdateState(EDxDataChannelState::Connecting, TEXT("Raw TCP worker started"));
	return bStarted;
}

bool UDxDeviceSensorBulkChannel::Connect()
{
	check(IsInGameThread());
	if (IsShutdown())
		return false;
	auto* Source = Transport.Get();
	if (!Source)
	{
		auto* Game = GetOwningGameInstance();
		auto* World = Game ? Game->GetWorld() : nullptr;
		Source = World ? World->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>() : nullptr;
	}
	if (!Source)
	{
		SetLastError(TEXT("Raw TCP worker attaches when a game World is ready."));
		return false;
	}
	if (Source->IsHighThroughputTransportRunning())
		return true;
	FVirtualSensorTransportProfile Legacy;
	FString Passcode;
	DeviceSensorTransportConfig::ReadLegacyConnection(Legacy, Passcode);
	return Start(*Source, UVirtualSensorHighThroughputTransportSubsystem::MakeProfile(Legacy), Passcode);
}

void UDxDeviceSensorBulkChannel::StopForWorld(UVirtualSensorHighThroughputTransportSubsystem& Source)
{
	check(IsInGameThread());
	Source.StopWorker();
	if (Transport.Get() == &Source)
	{
		Source.BulkChannel.Reset();
		Transport.Reset();
		UpdateState(EDxDataChannelState::Disconnected, TEXT("Raw TCP worker stopped"));
	}
}

void UDxDeviceSensorBulkChannel::Disconnect()
{
	if (auto* Source = Transport.Get())
		StopForWorld(*Source);
}

void UDxDeviceSensorBulkChannel::OnChannelShutdown()
{
	Disconnect();
}

bool UDxDeviceSensorBulkChannel::PublishFrame(const FVirtualSensorBinaryFrame& Frame, FString& Error)
{
	check(IsInGameThread());
	if (IsShutdown() || !Transport.IsValid())
	{
		RecordDropped();
		Error = TEXT("High-throughput transport is not running.");
		return false;
	}
	if (!CheckMessageSize(Frame.NumBytes()))
	{
		Error = GetStatus().LastError;
		return false;
	}
	const bool bAccepted = Transport->EnqueueWorkerFrame(Frame, Error);
	if (!bAccepted)
	{
		RecordDropped();
		SetLastError(Error);
	}
	return bAccepted;
}

void UDxDeviceSensorBulkChannel::ObserveConnection(bool bConnected, const FString& Reason)
{
	if (!bConnected)
		SetLastError(Reason);
	UpdateState(bConnected ? EDxDataChannelState::Connected : EDxDataChannelState::Reconnecting, Reason);
}

bool UDxDeviceSensorBulkChannel::PublishBytes(const FString&, const TArray<uint8>&, const TMap<FName, FString>&, const FSTOMPRequestCompleted&)
{
	// generic byte API는 acquisition snapshot/공유 body/예약 token이 없어 PCD 계약을 만들 수 없다.
	// 기존 PublishFrame/World façade만 사용하며 바디 복사·GT 직렬화를 추가하지 않는다.
	RecordDropped();
	SetLastError(TEXT("sensor-bulk requires an immutable FVirtualSensorBinaryFrame via PublishFrame."));
	return false;
}

FString UDxDeviceSensorBulkChannel::Subscribe(const FString&, const FSTOMPSubscriptionEvent&, const FSTOMPRequestCompleted&)
{
	SetLastError(TEXT("Use the existing explicit ConfigureReceiver selection for sensor-bulk diagnostics."));
	return FString();
}

void UDxDeviceSensorBulkChannel::Unsubscribe(const FString&, const FSTOMPRequestCompleted& CompletionCallback)
{
	CompletionCallback.ExecuteIfBound(false, TEXT("Use the existing ConfigureReceiver selection."));
}
