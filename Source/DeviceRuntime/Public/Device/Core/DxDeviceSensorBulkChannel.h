#pragma once

#include "CoreMinimal.h"
#include "Core/DxDataChannelBase.h"
#include "DxDeviceSensorBulkChannel.generated.h"

class UVirtualSensorHighThroughputTransportSubsystem;
struct FVirtualSensorHighThroughputProfile;
struct FVirtualSensorBinaryFrame;

/** 기존 World 소유 Raw TCP worker의 GI 수명 채널. socket/PCD/receipt 처리는 worker에만 있다.
 * 공개 제어/프레임 접수는 GT, payload/socket/검증은 기존 worker에서 실행한다.
 */
UCLASS()
class DEVICERUNTIME_API UDxDeviceSensorBulkChannel : public UDxDataChannelBase
{
	GENERATED_BODY()

public:
	virtual EDxDataChannelThreadPolicy GetThreadPolicy() const override { return EDxDataChannelThreadPolicy::Worker; }
	virtual bool Connect() override;
	virtual void Disconnect() override;
	virtual FString Subscribe(const FString& Destination, const FSTOMPSubscriptionEvent& EventCallback, const FSTOMPRequestCompleted& CompletionCallback) override;
	virtual void Unsubscribe(const FString& SubscriptionKey, const FSTOMPRequestCompleted& CompletionCallback) override;

	bool Prepare(UVirtualSensorHighThroughputTransportSubsystem& Source, const FVirtualSensorHighThroughputProfile& Profile, const FString& Passcode, FString& Error, bool bAllowIdleReplacement);
	bool Start(UVirtualSensorHighThroughputTransportSubsystem& Source, const FVirtualSensorHighThroughputProfile& Profile, const FString& Passcode);
	bool PublishFrame(const FVirtualSensorBinaryFrame& Frame, FString& Error);
	void StopForWorld(UVirtualSensorHighThroughputTransportSubsystem& Source);

	// 기존 event drain에서 호출. worker 이벤트/접수의 원래 집계 지점을 바꾸지 않는다.
	void ObserveConnection(bool bConnected, const FString& Reason);
	void ObserveSubmitted() { RecordPublished(); }
	void ObserveBackpressure() { RecordBackpressure(); }
	void ObserveDropped() { RecordDropped(); }
	void SetExplicitSettings(bool bExplicit) { bExplicitSettings = bExplicit; }
	void ResolveLegacyProfile(FVirtualSensorHighThroughputProfile& Profile, FString& Passcode) const;

protected:
	virtual void OnChannelShutdown() override;
	virtual bool PublishBytes(const FString& Destination, const TArray<uint8>& Body, const TMap<FName, FString>& Headers, const FSTOMPRequestCompleted& CompletionCallback) override;

private:
	bool BindWorld(UVirtualSensorHighThroughputTransportSubsystem& Source, FString& Error);
	TWeakObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> Transport;
	bool bExplicitSettings = false;
};
