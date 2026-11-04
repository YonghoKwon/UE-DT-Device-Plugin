#include "Device/Core/DxDeviceSensorBulkChannel.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Device/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceBulkChannelPolicy, "MA0T10.DataIf10.BulkPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceBulkChannelPolicy::RunTest(const FString&)
{
	auto* Channel = NewObject<UDxDeviceSensorBulkChannel>();
	FDxDataChannelSettings Settings;
	Settings.ChannelName = TEXT("sensor-bulk");
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	Settings.Url = TEXT("tcp://127.0.0.1:61617");
	Settings.ReconnectInitialDelaySeconds = 3;
	Settings.ReconnectMaxDelaySeconds = 12;
	Settings.ReconnectBackoffMultiplier = 4;
	Channel->InitializeChannel(Settings.ChannelName, Settings, nullptr);
	TestEqual(TEXT("worker policy"), Channel->GetThreadPolicy(), EDxDataChannelThreadPolicy::Worker);
	TestEqual(TEXT("registration is inert"), Channel->GetState(), EDxDataChannelState::Disconnected);
	FVirtualSensorHighThroughputProfile Profile;
	const auto Original = Profile;
	FString Passcode;
	Channel->ResolveLegacyProfile(Profile, Passcode);
	TestEqual(TEXT("absent named settings preserve legacy URL"), Profile.BrokerUrl, Original.BrokerUrl);
	TestEqual(TEXT("absent named settings preserve raw retry default"), Profile.ReconnectMaxDelaySeconds, Original.ReconnectMaxDelaySeconds);
	Channel->SetExplicitSettings(true);
	Channel->ResolveLegacyProfile(Profile, Passcode);
	TestEqual(TEXT("named URL wins"), Profile.BrokerUrl, Settings.Url);
	TestEqual(TEXT("named initial wins"), Profile.ReconnectInitialDelaySeconds, 3.0);
	TestEqual(TEXT("named max wins"), Profile.ReconnectMaxDelaySeconds, 12.0);
	TestEqual(TEXT("named multiplier wins"), Profile.ReconnectBackoffMultiplier, 4.0);
	TestTrue(TEXT("no implicit default credentials"), Profile.UserName.IsEmpty() && Passcode.IsEmpty());
	Channel->ObserveSubmitted();
	Channel->ObserveBackpressure();
	Channel->ObserveDropped();
	const auto Status = Channel->GetStatus();
	TestEqual(TEXT("published observation, not enqueue"), Status.PublishedCount, int64(1));
	TestEqual(TEXT("backpressure observation"), Status.BackpressureCount, int64(1));
	TestEqual(TEXT("drop observation"), Status.DroppedCount, int64(1));
	FVirtualSensorBinaryFrame Frame;
	FString Error;
	TestFalse(TEXT("no World does not create an unowned worker"), Channel->PublishFrame(Frame, Error));
	TestFalse(TEXT("rejection has a reason"), Error.IsEmpty());
	Channel->ShutdownChannel();
	TestFalse(TEXT("shutdown cannot reconnect"), Channel->Connect());
	return true;
}
#endif
