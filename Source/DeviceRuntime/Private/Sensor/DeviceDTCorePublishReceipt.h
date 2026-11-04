#pragma once
#include "CoreMinimal.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "DeviceDTCorePublishReceipt.generated.h"
class UDeviceDTCoreReceiptPool;

/** One bounded, weak-owner receipt. Never exists on the default transport path. */
UCLASS(Transient)
class UDeviceDTCorePublishReceipt : public UObject
{
	GENERATED_BODY()
public:
	void Start(UVirtualSensorTransportComp* InOwner, UDxWebSocketSubsystem* InSocket, UDeviceDTCoreReceiptPool* InPool, const FVirtualSensorTransportResult& Submitted, double TimeoutSeconds);
	void Cancel();
	void AbandonOwner();
	UFUNCTION()
	void HandleCompleted(bool bSuccess, FString Error);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FDeviceDTCoreTransportTest;
#endif
	UFUNCTION()
	void HandleConnectionState(EDxWebSocketConnectionState State, FString Reason);
	void Timeout();
	void Resolve(bool bSuccess, bool bReceiptCompleted, const FString& Message);
	void Detach();
	TWeakObjectPtr<UVirtualSensorTransportComp> Owner;
	TWeakObjectPtr<UDeviceDTCoreReceiptPool> Pool;
	TWeakObjectPtr<UDxWebSocketSubsystem> Socket;
	TWeakObjectPtr<UWorld> World;
	FVirtualSensorTransportResult Result;
	FTimerHandle TimeoutHandle;
	double StartedSeconds = 0.0;
	bool bFinished = false;
};
