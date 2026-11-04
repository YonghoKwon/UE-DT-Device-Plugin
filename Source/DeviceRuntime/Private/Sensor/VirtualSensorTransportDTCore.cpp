#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "DeviceDTCorePublishReceipt.h"
#include "DeviceDTCoreReceiptPool.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Core/DxDataChannelBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDxWebSocketSubsystem* UVirtualSensorTransportComp::GetDTCoreWebSocket() const
{
	if (!IsInGameThread())
		return nullptr;
	auto* World = GetWorld();
	auto* Game = World ? World->GetGameInstance() : nullptr;
	return Game ? Game->GetSubsystem<UDxWebSocketSubsystem>() : nullptr;
}
int32 UVirtualSensorTransportComp::GetDTCorePublishDroppedCount() const
{
	auto* Socket = TransportMode == EVirtualSensorTransportMode::DTCoreWebSocket ? GetDTCoreWebSocket() : nullptr;
	return Socket ? Socket->GetPublishDroppedCount() : 0;
}
bool UVirtualSensorTransportComp::IsDTCoreJsonRequestAllowed(const FString& DataKind, bool bManualRequest)
{
	return bManualRequest || DataKind.Equals(TEXT("status"), ESearchCase::IgnoreCase) || DataKind.Equals(TEXT("sensor-status"), ESearchCase::IgnoreCase) ||
	       DataKind.Equals(TEXT("slab-result"), ESearchCase::IgnoreCase) || DataKind.Equals(TEXT("slab-run-result"), ESearchCase::IgnoreCase) || DataKind.Equals(TEXT("run-result"), ESearchCase::IgnoreCase);
}
FVirtualSensorTransportResult UVirtualSensorTransportComp::SendDTCoreJson(UDxWebSocketSubsystem* Socket, const FString& SensorId, const FString& SensorType, const FString& DataKind, int64 FrameId, const FString& JsonText, bool bManualRequest, bool bRequestReceipt, const TMap<FString, FString>& AdditionalHeaders)
{
	FVirtualSensorTransportResult Result;
	Result.Protocol = TEXT("STOMP/DTCore");
	if (!IsInGameThread())
	{
		Result.Message = TEXT("공유 JSON 전송은 게임 스레드 요청 API를 사용하세요.");
		return Result;
	}
	Result.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Result.SensorId = SensorId;
	Result.SensorType = SensorType;
	Result.DataKind = DataKind;
	Result.FrameId = FrameId;
	Result.RunId = AdditionalHeaders.FindRef(TEXT("x-run-uuid"));
	Result.bManualRequest = bManualRequest;
	Result.bReceiptRequested = bRequestReceipt;
	Result.Destination = ResolveTopic(SensorType, DataKind);
	FTCHARToUTF8 Utf8(*JsonText);
	Result.DataLength = Utf8.Length();
	if (!IsDTCoreJsonRequestAllowed(DataKind, bManualRequest))
	{
		Result.Message = TEXT("DTCore 공유 연결은 상태·수동 1회·결과 JSON 전용입니다. 연속 센서 프레임은 전용 연결을 사용하세요.");
		return Result;
	}
	if (Result.DataLength > TransportProfile.MaxMessageBytes)
	{
		Result.Message = TEXT("DTCore JSON 메시지 크기 제한 초과");
		return Result;
	}
	if (!Socket)
	{
		Result.Message = TEXT("DTCore WebSocket 서비스가 없습니다. 센서 전용 연결로 자동 전환하지 않습니다.");
		return Result;
	}
	UDeviceDTCoreReceiptPool* Pool = nullptr;
	const bool bConnected = Socket->GetConnectionState() == EDxWebSocketConnectionState::Ready || Socket->GetConnectionState() == EDxWebSocketConnectionState::Subscribing;
	if (bRequestReceipt && bConnected)
	{
		auto* Game = Socket->GetGameInstance();
		Pool = Game ? Game->GetSubsystem<UDeviceDTCoreReceiptPool>() : nullptr;
		if (!Pool || !Pool->HasCapacity())
		{
			Result.bBackpressureRejected = true;
			Result.Message = TEXT("공유 JSON receipt 대기 상한(16) · 응답 또는 공유 연결 복구 후 다시 시도하세요.");
			return Result;
		}
	}

	TMap<FName, FString> Headers;
	Headers.Add(TEXT("destination-type"), TEXT("MULTICAST"));
	Headers.Add(TEXT("content-type"), TEXT("application/json;charset=utf-8"));
	Headers.Add(TEXT("x-sensor-id"), SensorId);
	Headers.Add(TEXT("x-sensor-type"), SensorType);
	Headers.Add(TEXT("x-data-kind"), DataKind);
	Headers.Add(TEXT("x-frame-id"), LexToString(FrameId));
	Headers.Add(TEXT("x-request-id"), Result.RequestId);
	for (const auto& Pair : AdditionalHeaders)
		if (Pair.Key.StartsWith(TEXT("x-")))
			Headers.Add(FName(*Pair.Key), Pair.Value);
	UDeviceDTCorePublishReceipt* Receipt = nullptr;
	FSTOMPRequestCompleted Completion;
	if (bRequestReceipt && bConnected)
	{
		Receipt = NewObject<UDeviceDTCorePublishReceipt>(Pool);
		if (!Pool->Reserve(Receipt))
			return Result;
		DTCoreReceipts.Add(Receipt);
		FVirtualSensorTransportResult Submitted = Result;
		Submitted.bSubmitted = true;
		Receipt->Start(this, Socket, Pool, Submitted, TransportProfile.TimeoutSeconds);
		Completion.BindDynamic(Receipt, &UDeviceDTCorePublishReceipt::HandleCompleted);
	}
	auto* DefaultChannel = Socket->FindChannel(UDxWebSocketSubsystem::DefaultChannelName);
	Result.bSubmitted = DefaultChannel && DefaultChannel->PublishText(Result.Destination, JsonText, Headers, Completion);
	if (!Result.bSubmitted)
	{
		if (Receipt)
		{
			Receipt->Cancel();
			DTCoreReceipts.Remove(Receipt);
		}
		Result.Message = FString::Printf(TEXT("DTCore 공유 연결 준비 전 · 전송 거절 (dropped=%d)"), Socket->GetPublishDroppedCount());
		return Result;
	}
	Result.Message = bRequestReceipt ? TEXT("DTCore JSON 제출됨 · Broker receipt 대기") : TEXT("DTCore JSON 제출됨 · receipt 미요청, 소비자 처리 미확인");
	return Result;
}
void UVirtualSensorTransportComp::CompleteDTCoreReceipt(UDeviceDTCorePublishReceipt* Receipt, const FVirtualSensorTransportResult& Result)
{
	if (!DTCoreReceipts.Remove(Receipt) || TransportMode != EVirtualSensorTransportMode::DTCoreWebSocket)
		return;
	if (Result.bManualRequest && LastResult.RequestId == Result.RequestId)
		LastResult = Result;
	OnDataSent.Broadcast(Result);
}
void UVirtualSensorTransportComp::CancelDTCoreReceipts()
{
	for (UDeviceDTCorePublishReceipt* Receipt : DTCoreReceipts)
		if (Receipt)
			Receipt->AbandonOwner();
	DTCoreReceipts.Reset();
}
