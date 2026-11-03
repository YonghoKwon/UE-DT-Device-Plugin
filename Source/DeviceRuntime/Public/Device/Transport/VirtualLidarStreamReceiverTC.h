#pragma once

#include "CoreMinimal.h"
#include "Device/Contracts/DeviceSensorStreamData.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "Device/Contracts/VirtualSensorStreamReceiverTypes.h"
#include "VirtualLidarStreamReceiverTC.generated.h"



/** virtual-lidar.v1 Topic 본문을 검증하는 UKP형 수신 Handler입니다. */
UCLASS()
class DEVICERUNTIME_API UVirtualLidarStreamReceiverTC : public UTransactionCodeMessage
{
	GENERATED_BODY()

public:
	UVirtualLidarStreamReceiverTC();
	virtual TSharedPtr<FTransactionCodeDataBase> ParseToStruct(const FString& JsonString) const override;
	virtual void ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data) override;

private:
	double LastOutputLogSeconds = -1.0;
};
