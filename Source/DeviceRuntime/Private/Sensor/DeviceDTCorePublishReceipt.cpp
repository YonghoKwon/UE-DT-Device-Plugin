#include "DeviceDTCorePublishReceipt.h"
#include "DeviceDTCoreReceiptPool.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

void UDeviceDTCorePublishReceipt::Start(UVirtualSensorTransportComp* InOwner, UDxWebSocketSubsystem* InSocket, UDeviceDTCoreReceiptPool* InPool, const FVirtualSensorTransportResult& Submitted, double TimeoutSeconds)
{
	Owner = InOwner;
	Pool = InPool;
	Socket = InSocket;
	World = InOwner->GetWorld();
	Result = Submitted;
	StartedSeconds = FPlatformTime::Seconds();
	InSocket->OnConnectionStateChanged.AddDynamic(this, &UDeviceDTCorePublishReceipt::HandleConnectionState);
	if (World.IsValid())
		World->GetTimerManager().SetTimer(TimeoutHandle, this, &UDeviceDTCorePublishReceipt::Timeout, FMath::Max(0.1, TimeoutSeconds), false);
}
void UDeviceDTCorePublishReceipt::Detach()
{
	if (Socket.IsValid())
		Socket->OnConnectionStateChanged.RemoveDynamic(this, &UDeviceDTCorePublishReceipt::HandleConnectionState);
	if (World.IsValid())
		World->GetTimerManager().ClearTimer(TimeoutHandle);
	Socket.Reset();
	World.Reset();
}
void UDeviceDTCorePublishReceipt::Cancel()
{
	bFinished = true;
	Detach();
	if (Pool.IsValid())
		Pool->Release(this);
	Pool.Reset();
	Owner.Reset();
}
void UDeviceDTCorePublishReceipt::AbandonOwner()
{
	Owner.Reset();
	if (World.IsValid())
		World->GetTimerManager().ClearTimer(TimeoutHandle);
	// The shared client's receipt is still outstanding. Keep the global slot until its outcome or disconnect.
}
void UDeviceDTCorePublishReceipt::Resolve(bool bSuccess, bool bReceiptCompleted, const FString& Message)
{
	if (bFinished)
		return;
	TStrongObjectPtr<UDeviceDTCorePublishReceipt> KeepAlive(this);
	bFinished = true;
	Detach();
	Result.bSubmitted = true;
	Result.bAccepted = bSuccess;
	Result.bReceiptReceived = bSuccess;
	Result.bReceiptCompleted = bReceiptCompleted;
	Result.LatencyMs = static_cast<float>((FPlatformTime::Seconds() - StartedSeconds) * 1000.0);
	Result.Message = Message;
	if (auto* Target = Owner.Get())
		Target->CompleteDTCoreReceipt(this, Result);
	Owner.Reset();
	if (Pool.IsValid())
		Pool->Release(this);
	Pool.Reset();
}
void UDeviceDTCorePublishReceipt::HandleCompleted(bool bSuccess, FString Error)
{
	// Do not copy arbitrary broker error bodies into UI/logs.
	Resolve(bSuccess, true, bSuccess ? TEXT("DTCore Broker receipt 수락 · 소비자 처리 미확인") : TEXT("DTCore Broker receipt 거절"));
}
void UDeviceDTCorePublishReceipt::HandleConnectionState(EDxWebSocketConnectionState State, FString Reason)
{
	if (State != EDxWebSocketConnectionState::Ready && State != EDxWebSocketConnectionState::Subscribing)
		Resolve(false, false, TEXT("DTCore 연결 변경 · Broker 수락 여부 미확인"));
}
void UDeviceDTCorePublishReceipt::Timeout()
{
	if (bFinished || !Owner.IsValid())
		return;
	FVirtualSensorTransportResult TimedOut = Result;
	TimedOut.bSubmitted = true;
	TimedOut.bAccepted = false;
	TimedOut.bReceiptCompleted = false;
	TimedOut.LatencyMs = static_cast<float>((FPlatformTime::Seconds() - StartedSeconds) * 1000.0);
	TimedOut.Message = TEXT("DTCore Broker receipt 대기 시간 초과 · 수락 여부 미확인, 자동 재전송 없음");
	if (auto* Target = Owner.Get())
		Target->CompleteDTCoreReceipt(this, TimedOut);
	AbandonOwner();
}
