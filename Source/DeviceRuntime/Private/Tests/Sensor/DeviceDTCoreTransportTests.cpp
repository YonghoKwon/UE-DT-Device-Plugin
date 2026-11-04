#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Async/Async.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Engine/GameInstance.h"
#include "../../Sensor/DeviceDTCorePublishReceipt.h"
#include "../../Sensor/DeviceDTCoreReceiptPool.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceDTCoreTransportPolicyTest, "MA0T10.DTCoreExt09.TransportPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceDTCoreTransportPolicyTest::RunTest(const FString& Parameters)
{
	auto* Transport = NewObject<UVirtualSensorTransportComp>();
	TestEqual(TEXT("default mode unchanged"), Transport->TransportMode, EVirtualSensorTransportMode::LogOnly);
	TestEqual(TEXT("old STOMP enum byte"), static_cast<uint8>(EVirtualSensorTransportMode::StompWebSocket), static_cast<uint8>(4));
	TestEqual(TEXT("new mode is appended"), static_cast<uint8>(EVirtualSensorTransportMode::DTCoreWebSocket), static_cast<uint8>(5));
	TestFalse(TEXT("default has no tick"), Transport->PrimaryComponentTick.bCanEverTick);
	TestTrue(TEXT("status allowed"), Transport->IsDTCoreJsonRequestAllowed(TEXT("status"), false));
	TestTrue(TEXT("manual one-shot allowed"), Transport->IsDTCoreJsonRequestAllowed(TEXT("manual-payload"), true));
	TestTrue(TEXT("slab result allowed"), Transport->IsDTCoreJsonRequestAllowed(TEXT("slab-run-result"), false));
	TestFalse(TEXT("point stream cannot use shared socket"), Transport->IsDTCoreJsonRequestAllowed(TEXT("pointcloud-stream"), false));
	TestFalse(TEXT("camera stream cannot use shared socket"), Transport->IsDTCoreJsonRequestAllowed(TEXT("camera-stream"), false));
	TestFalse(TEXT("telemetry stream cannot use shared socket"), Transport->IsDTCoreJsonRequestAllowed(TEXT("telemetry-stream"), false));
	Transport->TransportMode = EVirtualSensorTransportMode::DTCoreWebSocket;
	TestTrue(TEXT("PCD keeps dedicated broker eligibility"), Transport->UsesDedicatedSensorBroker());
	TestTrue(TEXT("new enum remains selectable by existing config parser"), StaticEnum<EVirtualSensorTransportMode>()->GetValueByNameString(TEXT("DTCoreWebSocket")) == 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceDTCoreTransportTest, "MA0T10.DTCoreExt09.TransportDisconnected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceDTCoreTransportTest::RunTest(const FString& Parameters)
{
	auto* Game = NewObject<UGameInstance>();
	auto* Socket = NewObject<UDxWebSocketSubsystem>(Game);
	auto* Transport = NewObject<UVirtualSensorTransportComp>();
	TestFalse(TEXT("default does not create a dedicated client"), Transport->StompClient.IsValid());
	TestEqual(TEXT("default has no shared receipts"), Transport->DTCoreReceipts.Num(), 0);
	Transport->TransportMode = EVirtualSensorTransportMode::DTCoreWebSocket;
	const auto Result = Transport->SendDTCoreJson(Socket, TEXT("SENSOR-A"), TEXT("lidar"), TEXT("manual-payload"), 77, TEXT("{}"), true, true, {{TEXT("x-run-uuid"), TEXT("run-snapshot")}});
	TestFalse(TEXT("disconnected publish returns false"), Result.bSubmitted);
	TestFalse(TEXT("not a broker receipt"), Result.bReceiptCompleted);
	TestFalse(TEXT("not consumer acceptance"), Result.bAccepted);
	TestEqual(TEXT("shared drop counted exactly once"), Socket->GetPublishDroppedCount(), 1);
	TestTrue(TEXT("failure exposes drop count"), Result.Message.Contains(TEXT("dropped=1")));
	TestEqual(TEXT("frame id remains immutable"), Result.FrameId, static_cast<int64>(77));
	TestEqual(TEXT("run id remains immutable"), Result.RunId, FString(TEXT("run-snapshot")));
	TestEqual(TEXT("failed send leaves no receipt"), Transport->DTCoreReceipts.Num(), 0);
	TestFalse(TEXT("failed send leaves no state binding"), Socket->OnConnectionStateChanged.IsBound());
	const auto Stream = Transport->SendDTCoreJson(Socket, TEXT("SENSOR-A"), TEXT("lidar"), TEXT("pointcloud-stream"), 78, TEXT("{}"), false, false);
	TestFalse(TEXT("continuous frame explicitly rejected"), Stream.bSubmitted);
	TestEqual(TEXT("policy rejection never calls shared publisher"), Socket->GetPublishDroppedCount(), 1);
	Transport->LastResult.RequestId = TEXT("unchanged-by-worker");
	auto Worker = Async(EAsyncExecution::Thread, [Transport]()
	                    { return Transport->SendJsonRequest(TEXT("SENSOR-A"), TEXT("lidar"), TEXT("manual-payload"), 123, TEXT("{}"), true); });
	const auto RejectedWorker = Worker.Get();
	TestFalse(TEXT("off-thread shared request rejected"), RejectedWorker.bSubmitted);
	TestEqual(TEXT("off-thread request never applies component UI state"), Transport->LastResult.RequestId, FString(TEXT("unchanged-by-worker")));
	TestEqual(TEXT("off-thread request never allocates a receipt"), Transport->DTCoreReceipts.Num(), 0);
	auto* Pool = NewObject<UDeviceDTCoreReceiptPool>(Game);
	FVirtualSensorTransportResult Pending;
	Pending.bManualRequest = true;
	Pending.bReceiptRequested = true;
	Pending.FrameId = 91;
	Pending.RequestId = Transport->LastResult.RequestId;
	auto* Receipt = NewObject<UDeviceDTCorePublishReceipt>(Transport);
	Transport->DTCoreReceipts.Add(Receipt);
	TestTrue(TEXT("global receipt slot reserved"), Pool->Reserve(Receipt));
	Receipt->Start(Transport, Socket, Pool, Pending, 1.0);
	Socket->OnConnectionStateChanged.Broadcast(EDxWebSocketConnectionState::Disconnected, TEXT("test-only disconnect"));
	TestEqual(TEXT("disconnect releases pending context"), Transport->DTCoreReceipts.Num(), 0);
	TestFalse(TEXT("disconnect receipt remains unknown"), Transport->LastResult.bReceiptCompleted);
	TestFalse(TEXT("disconnect is not broker acceptance"), Transport->LastResult.bAccepted);
	TestFalse(TEXT("disconnect removes state binding"), Socket->OnConnectionStateChanged.IsBound());
	Receipt->HandleCompleted(true, FString());
	TestFalse(TEXT("late completion cannot apply after cancellation"), Transport->LastResult.bAccepted);
	TestFalse(TEXT("shared mode never creates dedicated client for JSON"), Transport->StompClient.IsValid());
	TestEqual(TEXT("disconnect frees global slot"), Pool->Num(), 0);
	auto* Timed = NewObject<UDeviceDTCorePublishReceipt>(Pool);
	TestTrue(TEXT("deadline slot reserved"), Pool->Reserve(Timed));
	Transport->DTCoreReceipts.Add(Timed);
	Timed->Start(Transport, Socket, Pool, Pending, 1.0);
	Timed->StartedSeconds -= 2.0;
	Timed->Timeout();
	TestTrue(TEXT("deadline reports elapsed latency"), Transport->LastResult.LatencyMs >= 1900.0f);
	TestEqual(TEXT("deadline clears component context"), Transport->DTCoreReceipts.Num(), 0);
	TestEqual(TEXT("deadline cannot free an unresolved shared-client slot"), Pool->Num(), 1);
	Timed->HandleCompleted(true, FString());
	TestEqual(TEXT("late outcome releases shared slot"), Pool->Num(), 0);
	TestFalse(TEXT("late outcome cannot overwrite timed-out UI"), Transport->LastResult.bAccepted);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		auto* Unresolved = NewObject<UDeviceDTCorePublishReceipt>(Pool);
		TestTrue(TEXT("bounded shared slot reserved"), Pool->Reserve(Unresolved));
		Transport->DTCoreReceipts.Add(Unresolved);
		Unresolved->Start(Transport, Socket, Pool, Pending, 1.0);
	}
	Transport->CancelDTCoreReceipts();
	TestEqual(TEXT("owner teardown clears its UI contexts"), Transport->DTCoreReceipts.Num(), 0);
	TestFalse(TEXT("owner teardown cannot reset shared admission budget"), Pool->HasCapacity());
	TestEqual(TEXT("unresolved budget remains globally bounded"), Pool->Num(), 16);
	Socket->OnConnectionStateChanged.Broadcast(EDxWebSocketConnectionState::Disconnected, TEXT("test-only reconnect"));
	TestEqual(TEXT("connection retirement frees all global slots"), Pool->Num(), 0);

	return true;
}
#endif
