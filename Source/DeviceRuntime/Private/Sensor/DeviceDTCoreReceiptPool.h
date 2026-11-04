#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DeviceDTCoreReceiptPool.generated.h"
class UDeviceDTCorePublishReceipt;
/** Admission budget only. DTCore exclusively owns the connection and STOMP client. No tick/thread/subscription. */
UCLASS(Transient)
class UDeviceDTCoreReceiptPool : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	bool HasCapacity() const { return Pending.Num() < 16; }
	bool Reserve(UDeviceDTCorePublishReceipt* Receipt);
	void Release(UDeviceDTCorePublishReceipt* Receipt);
	int32 Num() const { return Pending.Num(); }
	virtual void Deinitialize() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDeviceDTCorePublishReceipt>> Pending;
};
