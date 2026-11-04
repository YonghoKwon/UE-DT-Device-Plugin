#include "Device/Core/DxDeviceSensorBulkChannel.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Device/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Device/Core/DxDeviceSensorStompChannel.h"
#include "Device/Core/DeviceDataChannelSettings.h"
#include "Device/Core/DxDeviceSensorChannelSubsystem.h"
#include "Core/DxDataChannelLibrary.h"
#include "Engine/GameInstance.h"
#include "Async/TaskGraphInterfaces.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "../../Sensor/DeviceStompSessionClient.h"
#include "../../Core/DeviceDataChannelLooseConfig.h"
#include "Misc/ConfigCacheIni.h"
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
	{
		auto* Topics = GetMutableDefault<UDeviceDataChannelSettings>();
		TGuardValue<TArray<FDeviceDataChannelTopics>> Restore(Topics->Channels, {});
		FDeviceDataChannelTopics Policy;
		Policy.ChannelName = TEXT("sensor-bulk");
		Policy.Destinations.Add({TEXT("pointcloud"), TEXT("topic.fixture.pcd"), false, TEXT("PCD")});
		Policy.Destinations.Add({TEXT("one"), TEXT("topic.fixture.one"), true, TEXT("diagnostic")});
		Policy.Destinations.Add({TEXT("two"), TEXT("topic.fixture.two"), true, TEXT("diagnostic")});
		Topics->Channels.Add(Policy);
		FVirtualSensorHighThroughputProfile Resolved;
		Channel->ResolveLegacyProfile(Resolved, Passcode);
		TestEqual(TEXT("bulk sender resolves its project alias"), Resolved.PointCloudTopic, FString(TEXT("topic.fixture.pcd")));
		TestEqual(TEXT("bulk worker receives both explicit interests"), Resolved.ProjectSubscriptionDestinations.Num(), 2);
	}
	return true;
}

namespace
{
class FDataIfStompProbe : public IStompClient
{
public:
	FStompHeader ConnectHeaders, SendHeaders;
	FStompBuffer SentBody;
	FString SentDestination;
	bool bConnected = false, bCompleteImmediately = true;
	int32 ConnectCalls = 0;
	TArray<FStompRequestCompleted> Receipts;
	TArray<FStompSubscriptionEvent> Subscriptions;
	TArray<FString> SubscriptionDestinations;
	TArray<FStompRequestCompleted> SubscribeReceipts;
	virtual void Connect(const FStompHeader& Headers) override
	{
		ConnectHeaders = Headers;
		bConnected = true;
		++ConnectCalls;
	}
	virtual void Disconnect(const FStompHeader&) override { bConnected = false; }
	virtual bool IsConnected() const override { return bConnected; }
	virtual FStompSubscriptionId Subscribe(const FString& Destination, const FStompSubscriptionEvent& Event, const FStompRequestCompleted& Callback) override
	{
		SubscriptionDestinations.Add(Destination);
		Subscriptions.Add(Event);
		SubscribeReceipts.Add(Callback);
		return FString::Printf(TEXT("probe-subscription-%d"), Subscriptions.Num());
	}
	virtual void Unsubscribe(FStompSubscriptionId, const FStompRequestCompleted&) override {}
	virtual void Send(const FString& Destination, const FStompBuffer& Body, const FStompHeader& Headers, const FStompRequestCompleted& Callback) override
	{
		SentDestination = Destination;
		SentBody = Body;
		SendHeaders = Headers;
		Receipts.Add(Callback);
		if (bCompleteImmediately)
			Callback.ExecuteIfBound(true, FString());
	}
	virtual FStompClientConnectedEvent& OnConnected() override { return Connected; }
	virtual FStompClientConnectionErrorEvent& OnConnectionError() override { return ConnectionError; }
	virtual FStompClientErrorEvent& OnError() override { return Error; }
	virtual FStompClientClosedEvent& OnClosed() override { return Closed; }
	FStompClientConnectedEvent Connected;
	FStompClientConnectionErrorEvent ConnectionError;
	FStompClientErrorEvent Error;
	FStompClientClosedEvent Closed;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceStompChannelPolicy, "MA0T10.DataIf10.StompPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceStompChannelPolicy::RunTest(const FString&)
{
	const auto AcceptedAck = UVirtualSensorTransportComp::MakeAckSubscriptionResult(TEXT("topic.ack"), true, FString());
	TestEqual(TEXT("legacy ACK success message"), AcceptedAck.Message, FString(TEXT("소비자 ACK Topic 구독 완료")));
	const auto FailedAck = UVirtualSensorTransportComp::MakeAckSubscriptionResult(TEXT("topic.ack"), false, TEXT("fixture"));
	TestEqual(TEXT("legacy ACK failure message"), FailedAck.Message, FString(TEXT("ACK Topic 구독 실패: fixture")));
	TestFalse(TEXT("subscription receipt is not consumer acknowledgement"), AcceptedAck.bConsumerAckReceived);
	auto* Channel = NewObject<UDxDeviceSensorStompChannel>();
	FDxDataChannelSettings Settings;
	Settings.ChannelName = TEXT("sensor");
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	Channel->InitializeChannel(Settings.ChannelName, Settings, nullptr);
	TestEqual(TEXT("JSON callbacks stay on GT"), Channel->GetThreadPolicy(), EDxDataChannelThreadPolicy::GameThread);
	TestEqual(TEXT("default registration does not connect"), Channel->GetState(), EDxDataChannelState::Disconnected);
	auto* First = NewObject<UGameInstance>();
	auto* Second = NewObject<UGameInstance>();
	FVirtualSensorTransportProfile Profile;
	FString Error;
	TestTrue(TEXT("first legacy owner"), Channel->AcquireOwner(First, Profile, FString(), FString(), Error));
	TestTrue(TEXT("matching legacy owner shares connection"), Channel->AcquireOwner(Second, Profile, FString(), FString(), Error));
	auto Other = Profile;
	Other.BrokerUrl = TEXT("ws://127.0.0.1:61617");
	TestFalse(TEXT("one owner cannot replace another's connection"), Channel->AcquireOwner(First, Other, FString(), FString(), Error));
	TestFalse(TEXT("profile conflict has a reason"), Error.IsEmpty());
	Channel->ReleaseOwner(Second);
	TestTrue(TEXT("sole owner may change profile"), Channel->AcquireOwner(First, Other, FString(), FString(), Error));
	Channel->ReleaseOwner(First);
	Channel->ShutdownChannel();
	TestFalse(TEXT("shutdown cannot reconnect"), Channel->Connect());
	TestFalse(TEXT("shutdown cannot acquire owner"), Channel->AcquireOwner(First, Profile, FString(), FString(), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceStompLegacyWire, "MA0T10.DataIf10.LegacyWire", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceStompLegacyWire::RunTest(const FString&)
{
	auto Inner = MakeShared<FDataIfStompProbe>();
	FStompHeader Legacy;
	FString Error;
	TestTrue(TEXT("legacy empty passcode header remains present"), UVirtualSensorTransportComp::BuildStompConnectHeaders(TEXT("fixture-user"), FString(), FString(), Legacy, Error));
	TSharedPtr<IStompClient> Adapter = MakeShared<FDeviceStompSessionClient>(Inner, Legacy);
	Adapter->Connect({{TEXT("host"), TEXT("fixture-host")}});
	TestEqual(TEXT("CONNECT login unchanged"), Inner->ConnectHeaders.FindRef(TEXT("login")), FString(TEXT("fixture-user")));
	TestTrue(TEXT("CONNECT empty passcode retained"), Inner->ConnectHeaders.Contains(TEXT("passcode")));
	TestEqual(TEXT("other CONNECT headers retained"), Inner->ConnectHeaders.FindRef(TEXT("host")), FString(TEXT("fixture-host")));
	const FStompBuffer Bytes{0, 1, 0xff, 0, 33};
	const FStompHeader Headers{{TEXT("x-request-id"), TEXT("request-oracle")}, {TEXT("destination-type"), TEXT("MULTICAST")}};
	bool bCompleted = false;
	Adapter->Send(VirtualSensorDefaults::PointCloudTopic, Bytes, Headers, FStompRequestCompleted::CreateLambda([&bCompleted](bool bSuccess, const FString&)
	                                                                                                           { bCompleted = bSuccess; }));
	TestTrue(TEXT("binary body byte identical including NUL"), Inner->SentBody == Bytes);
	TestTrue(TEXT("header names/values unchanged"), Inner->SendHeaders.OrderIndependentCompareEqual(Headers));
	TestTrue(TEXT("receipt callback delegates unchanged"), bCompleted);
	TestEqual(TEXT("destination unchanged"), Inner->SentDestination, FString(VirtualSensorDefaults::PointCloudTopic));
	TestEqual(TEXT("DTCore destination helper agrees for the default only"), UDxDataChannelLibrary::MakeDestination({TEXT("topic"), TEXT("virtual"), TEXT("sensor"), TEXT("export"), TEXT("0")}), FString(VirtualSensorDefaults::PointCloudTopic));
	// 커스텀 destination/기존 헤더를 새 규약으로 정규화하거나 dx-* 헤더로 교체하지 않는다.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceStompChannelLifecycle, "MA0T10.DataIf10.StompLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceStompChannelLifecycle::RunTest(const FString&)
{
	auto* Game = NewObject<UGameInstance>();
	auto* Channel = NewObject<UDxDeviceSensorStompChannel>(Game);
	auto Fake = MakeShared<FDataIfStompProbe>();
	Fake->bCompleteImmediately = false;
	Channel->TestClient = Fake;
	FDxDataChannelSettings Settings;
	Settings.ChannelName = TEXT("sensor");
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	Settings.ReceiptTimeoutSeconds = 0.1f;
	Settings.MaxMessageBytes = 3;
	Settings.ReconnectInitialDelaySeconds = 0.5f;
	Channel->InitializeChannel(Settings.ChannelName, Settings, Game);
	FString Error;
	TestTrue(TEXT("acquire native result owner"), Channel->AcquireOwner(Game, FVirtualSensorTransportProfile{}, FString(), FString(), Error));
	int32 Connections = 0, AckResults = 0, Completions = 0, Accepted = 0;
	Channel->Connected.AddLambda([&Connections, Channel](const FString&, const FString&, const FString&)
	                             {
		++Connections;
		Channel->SubscribeToAck(TEXT("topic.ack.fixture")); });
	Channel->AckSubscribed.AddLambda([&AckResults](bool bSuccess, const FString&)
	                                 { AckResults += bSuccess ? 1 : 0; });
	const auto Pump = []()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	};
	TestTrue(TEXT("connect delegates to DTCore"), Channel->Connect());
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("session-fixture"), FString());
	Pump();
	TestEqual(TEXT("connected event mapped once"), Connections, 1);
	if (!TestEqual(TEXT("ACK subscribed once"), Fake->Subscriptions.Num(), 1))
	{
		Channel->ShutdownChannel();
		return false;
	}
	Fake->SubscribeReceipts[0].Execute(true, FString());
	Pump();
	TestEqual(TEXT("ACK subscription result retained"), AckResults, 1);
	TestFalse(TEXT("named channel byte limit rejects before send"), Channel->Send(TEXT("topic.fixture"), FStompBuffer{1, 2, 3, 4}, {}));
	TestEqual(TEXT("byte limit recorded as dropped"), Channel->GetStatus().DroppedCount, int64(1));
	auto Completion = FStompRequestCompleted::CreateLambda([&Completions, &Accepted](bool bSuccess, const FString&)
	                                                       { ++Completions; Accepted += bSuccess ? 1 : 0; });
	TestTrue(TEXT("native receipt submission"), Channel->Send(TEXT("topic.fixture"), FStompBuffer{1, 0, 2}, {}, Completion));
	Fake->Receipts[0].Execute(true, FString());
	Pump();
	TestEqual(TEXT("native receipt completion once"), Completions, 1);
	TestEqual(TEXT("native receipt acceptance"), Accepted, 1);
	TestTrue(TEXT("receipt timeout submission"), Channel->Send(TEXT("topic.fixture"), FStompBuffer{3}, {}, Completion));
	Game->GetTimerManager().Tick(0.01f);
	++GFrameCounter;
	Game->GetTimerManager().Tick(0.2f);
	Pump();
	TestEqual(TEXT("timeout reports once"), Completions, 2);
	TestEqual(TEXT("timeout is not acceptance"), Accepted, 1);
	Fake->Receipts[1].Execute(true, FString());
	Pump();
	TestEqual(TEXT("late receipt ignored"), Completions, 2);
	TestTrue(TEXT("old generation submission"), Channel->Send(TEXT("topic.fixture"), FStompBuffer{4}, {}, Completion));
	Fake->bConnected = false;
	Fake->Closed.Broadcast(TEXT("fixture disconnect"));
	Pump();
	TestEqual(TEXT("closed channel schedules DTCore reconnect"), Channel->GetState(), EDxDataChannelState::Reconnecting);
	Game->GetTimerManager().Tick(0.01f);
	++GFrameCounter;
	Game->GetTimerManager().Tick(0.6f);
	TestEqual(TEXT("scheduled Connect actually reconnects"), Fake->ConnectCalls, 2);
	Fake->Receipts[2].Execute(true, FString());
	Pump();
	TestEqual(TEXT("old connection receipt ignored"), Completions, 2);
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("session-fixture-2"), FString());
	Pump();
	TestEqual(TEXT("second connection mapped"), Connections, 2);
	TestEqual(TEXT("ACK re-subscription after reconnect"), Fake->Subscriptions.Num(), 2);
	Channel->ReleaseOwner(Game);
	TestEqual(TEXT("last legacy owner release disconnects"), Channel->GetState(), EDxDataChannelState::Disconnected);
	Channel->ShutdownChannel();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceTopicAliases, "MA0T10.DataIf10.TopicAliases", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceTopicAliases::RunTest(const FString&)
{
	auto* Settings = NewObject<UDeviceDataChannelSettings>();
	Settings->Channels.Empty();
	TestEqual(TEXT("unset settings preserve legacy custom destination"), Settings->Resolve(TEXT("sensor"), TEXT("lidar"), TEXT("custom.legacy")), FString(TEXT("custom.legacy")));
	FDeviceDataChannelTopics Topics;
	Topics.ChannelName = TEXT("sensor");
	Topics.Destinations.Add({TEXT("lidar"), TEXT("topic.fixture.lidar"), true, TEXT("telemetry")});
	Topics.Destinations.Add({TEXT("status"), TEXT("topic.fixture.status"), true, TEXT("status")});
	Topics.Destinations.Add({TEXT("same-status"), TEXT("topic.fixture.status"), true, TEXT("alias")});
	Settings->Channels.Add(Topics);
	TestEqual(TEXT("configured alias wins"), Settings->Resolve(TEXT("sensor"), TEXT("lidar"), TEXT("legacy")), FString(TEXT("topic.fixture.lidar")));
	TestEqual(TEXT("different channel falls back independently"), Settings->Resolve(TEXT("sensor-bulk"), TEXT("lidar"), TEXT("bulk.legacy")), FString(TEXT("bulk.legacy")));
	TestEqual(TEXT("two unique interests, repeated alias destination deduplicated"), Settings->GetSubscriptions(TEXT("sensor")).Num(), 2);
	FString Error;
	TestTrue(TEXT("valid policy"), Settings->Validate(Error));
	Settings->Channels[0].Destinations.Add({TEXT("lidar"), TEXT("other"), false, FString()});
	TestFalse(TEXT("duplicate alias rejected"), Settings->Validate(Error));
	TestFalse(TEXT("validation gives reason"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceDuplicateTopicWarning, "MA0T10.DataIf10.DuplicateTopicWarning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceDuplicateTopicWarning::RunTest(const FString&)
{
	auto* Settings = NewObject<UDeviceDataChannelSettings>();
	Settings->Channels.Empty();
	for (FName Name : {FName(TEXT("sensor")), FName(TEXT("sensor-bulk"))})
	{
		FDeviceDataChannelTopics Topics;
		Topics.ChannelName = Name;
		Topics.Destinations.Add({TEXT("status"), TEXT("topic.fixture.shared"), true, TEXT("fixture")});
		Settings->Channels.Add(Topics);
	}
	TestEqual(TEXT("cross-channel warning"), Settings->GetDuplicateSubscriptionWarnings().Num(), 1);
	AddExpectedError(TEXT("Duplicate topic subscription:"), EAutomationExpectedErrorFlags::Contains, 1);
	auto* Registry = NewObject<UDxDeviceSensorChannelSubsystem>(NewObject<UGameInstance>());
	Registry->ConfigureTopicSubscriptions(*Settings);
	TestTrue(TEXT("same warning available to status UI"), Registry->GetTopicConfigurationWarning().Contains(TEXT("sensor-bulk")));
	TestEqual(TEXT("warning does not remove interests"), Settings->GetSubscriptions(TEXT("sensor")).Num(), 1);
	Settings->Channels[1].Destinations[0].bSubscribe = false;
	TestEqual(TEXT("publish-only topic does not warn"), Settings->GetDuplicateSubscriptionWarnings().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceMultipleTopicSubscriptions, "MA0T10.DataIf10.MultipleTopicSubscriptions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceMultipleTopicSubscriptions::RunTest(const FString&)
{
	auto* Game = NewObject<UGameInstance>();
	auto* Channel = NewObject<UDxDeviceSensorStompChannel>(Game);
	auto Fake = MakeShared<FDataIfStompProbe>();
	Channel->TestClient = Fake;
	FDxDataChannelSettings Settings;
	Settings.ChannelName = TEXT("sensor");
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	Settings.ReconnectInitialDelaySeconds = 0.5f;
	Channel->InitializeChannel(Settings.ChannelName, Settings, Game);
	TArray<TStrongObjectPtr<UDeviceDataTopicSubscription>> Interests;
	for (const TCHAR* Topic : {TEXT("topic.fixture.one"), TEXT("topic.fixture.two")})
	{
		auto* Interest = NewObject<UDeviceDataTopicSubscription>();
		Interest->Channel = Channel;
		Interest->Destination = Topic;
		Interest->Issue();
		Interest->Issue();
		TestFalse(TEXT("interest queued while disconnected"), Interest->Key.IsEmpty());
		Interests.Emplace(Interest);
	}
	TestEqual(TEXT("interest registration is inert"), Fake->ConnectCalls, 0);
	Channel->Connect();
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("one"), FString());
	const auto Pump = []()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	};
	Pump();
	TestEqual(TEXT("multiple topics on one connection"), Fake->Subscriptions.Num(), 2);
	Fake->bConnected = false;
	Fake->Closed.Broadcast(TEXT("fixture"));
	Pump();
	Game->GetTimerManager().Tick(0.01f);
	++GFrameCounter;
	Game->GetTimerManager().Tick(0.6f);
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("two"), FString());
	Pump();
	if (TestEqual(TEXT("both topics resubscribed"), Fake->Subscriptions.Num(), 4))
	{
		TestEqual(TEXT("first destination preserved"), Fake->SubscriptionDestinations[0], Fake->SubscriptionDestinations[2]);
		TestEqual(TEXT("second destination preserved"), Fake->SubscriptionDestinations[1], Fake->SubscriptionDestinations[3]);
	}
	Channel->ShutdownChannel();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceSharedTopicLifecycle, "MA0T10.DataIf10.SharedTopicLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceSharedTopicLifecycle::RunTest(const FString&)
{
	auto* Channel = NewObject<UDxDeviceSensorStompChannel>();
	auto Fake = MakeShared<FDataIfStompProbe>();
	Channel->TestClient = Fake;
	FDxDataChannelSettings Settings;
	Settings.ChannelName = TEXT("sensor");
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	Channel->InitializeChannel(Settings.ChannelName, Settings, nullptr);
	Channel->Connect();
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("fixture"), FString());
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	auto* Interest = NewObject<UDeviceDataTopicSubscription>();
	Interest->Channel = Channel;
	Interest->Destination = TEXT("topic.fixture.shared");
	Interest->ObserveSharedState(EDxDataChannelState::Connected);
	TestEqual(TEXT("shared ready issues one interest"), Fake->Subscriptions.Num(), 1);
	Interest->ObserveSharedState(EDxDataChannelState::Connected);
	TestEqual(TEXT("repeated ready is idempotent"), Fake->Subscriptions.Num(), 1);
	Interest->ObserveSharedState(EDxDataChannelState::Reconnecting);
	TestTrue(TEXT("old shared ID is discarded"), Interest->Key.IsEmpty());
	Interest->ObserveSharedState(EDxDataChannelState::Connected);
	TestEqual(TEXT("new shared generation requests interest again"), Fake->Subscriptions.Num(), 2);
	Channel->ShutdownChannel();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceLooseTopicConfig, "MA0T10.DataIf10.LooseTopicConfig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceLooseTopicConfig::RunTest(const FString&)
{
	const FString Common = TEXT("[/Script/DTCore.DTCoreSettings]\nWebSocketUrl=\"ws://127.0.0.1:1\"\nWebSocketPasscode=not-imported\n+DataChannels=(ChannelName=\"sensor\",Url=\"ws://127.0.0.1:2\")\n[/Script/DeviceRuntime.DeviceDataChannelSettings]\n+Channels=(ChannelName=\"sensor\",Destinations=())\n[/Script/EngineSettings.GameMapsSettings]\nGameDefaultMap=/not/imported\n");
	const FString Platform = TEXT("[/Script/DTCore.DTCoreSettings]\nWebSocketUrl=\"ws://127.0.0.1:3\"\n!DataChannels=ClearArray\n+DataChannels=(ChannelName=\"sensor-bulk\",Url=\"tcp://127.0.0.1:4\")\n[/Script/DeviceRuntime.DeviceDataChannelSettings]\n!Channels=ClearArray\n+Channels=(ChannelName=\"Default\",Destinations=())\n");
	const FString Filtered = DeviceDataChannelConfig::FilterProjectOverrides(Common);
	TestFalse(TEXT("credential value is excluded"), Filtered.Contains(TEXT("not-imported")));
	TestFalse(TEXT("unrelated map setting is excluded"), Filtered.Contains(TEXT("GameDefaultMap")));
	FConfigFile Combined;
	Combined.CombineFromBuffer(Filtered, TEXT("common-fixture"));
	Combined.CombineFromBuffer(DeviceDataChannelConfig::FilterProjectOverrides(Platform), TEXT("platform-fixture"));
	FString Url;
	Combined.GetString(TEXT("/Script/DTCore.DTCoreSettings"), TEXT("WebSocketUrl"), Url);
	TestEqual(TEXT("platform wins over common"), Url, FString(TEXT("ws://127.0.0.1:3")));
	TArray<FString> Entries;
	Combined.GetArray(TEXT("/Script/DTCore.DTCoreSettings"), TEXT("DataChannels"), Entries);
	TestEqual(TEXT("array clear operator retained"), Entries.Num(), 1);
	if (Entries.Num() == 1)
		TestTrue(TEXT("platform channel row retained"), Entries[0].Contains(TEXT("sensor-bulk")));
	Combined.GetArray(TEXT("/Script/DeviceRuntime.DeviceDataChannelSettings"), TEXT("Channels"), Entries);
	TestEqual(TEXT("topic policy independently replaced"), Entries.Num(), 1);
	if (Entries.Num() == 1)
		TestTrue(TEXT("Default topic row retained"), Entries[0].Contains(TEXT("Default")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceConfiguredAckPolicy, "MA0T10.DataIf10.ConfiguredAckPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceConfiguredAckPolicy::RunTest(const FString&)
{
	auto* Policy = GetMutableDefault<UDeviceDataChannelSettings>();
	TGuardValue<TArray<FDeviceDataChannelTopics>> Restore(Policy->Channels, {});
	FDeviceDataChannelTopics Topics;
	Topics.ChannelName = TEXT("sensor");
	Topics.Destinations.Add({TEXT("ack"), TEXT("topic.fixture.project-ack"), false, TEXT("consumer ACK")});
	Policy->Channels.Add(Topics);
	TestTrue(TEXT("explicit disabled ACK does not fall back to legacy subscription"), Policy->ResolveSubscription(TEXT("sensor"), TEXT("ack"), TEXT("legacy.ack")).IsEmpty());
	Policy->Channels[0].Destinations[0].bSubscribe = true;
	auto* Game = NewObject<UGameInstance>();
	auto* Channel = NewObject<UDxDeviceSensorStompChannel>(Game);
	auto Fake = MakeShared<FDataIfStompProbe>();
	Channel->TestClient = Fake;
	FDxDataChannelSettings Settings;
	Settings.ChannelName = TEXT("sensor");
	Settings.bAutoConnect = false;
	Settings.bUseDefaultCredentials = false;
	Settings.ReconnectInitialDelaySeconds = 0.5f;
	Channel->InitializeChannel(Settings.ChannelName, Settings, Game);
	TestEqual(TEXT("configured ACK registers without connecting"), Fake->ConnectCalls, 0);
	Channel->Connect();
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("first"), FString());
	const auto Pump = []()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	};
	Pump();
	TestEqual(TEXT("one configured ACK interest"), Fake->Subscriptions.Num(), 1);
	Channel->SubscribeToAck(TEXT("topic.fixture.project-ack"));
	TestEqual(TEXT("legacy caller shares configured ACK key"), Fake->Subscriptions.Num(), 1);
	Fake->bConnected = false;
	Fake->Closed.Broadcast(TEXT("fixture"));
	Pump();
	Game->GetTimerManager().Tick(0.01f);
	++GFrameCounter;
	Game->GetTimerManager().Tick(0.6f);
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("second"), FString());
	Pump();
	TestEqual(TEXT("configured ACK automatically renews once"), Fake->Subscriptions.Num(), 2);
	Channel->ReleaseOwner(Game);
	Channel->Connect();
	Fake->Connected.Broadcast(TEXT("1.2"), TEXT("third"), FString());
	Pump();
	TestEqual(TEXT("configured interest survives legacy-owner teardown"), Fake->Subscriptions.Num(), 3);
	Channel->ShutdownChannel();
	return true;
}
#endif
