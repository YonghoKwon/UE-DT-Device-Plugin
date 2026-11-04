#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxDeviceSensorChannelSubsystem.generated.h"

class UDxDeviceSensorBulkChannel;
class UDxWebSocketSubsystem;

/** 채널 등록만 GI 수명에 묶는다. 기본 등록은 연결/worker/tick을 시작하지 않는다. */
UCLASS()
class DEVICERUNTIME_API UDxDeviceSensorChannelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	static UDxDeviceSensorBulkChannel* FindBulk(const UWorld* World);

private:
	TWeakObjectPtr<UDxWebSocketSubsystem> Socket;
	UPROPERTY(Transient)
	TObjectPtr<UDxDeviceSensorBulkChannel> Bulk;
};
