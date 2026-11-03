#include "Device/Transport/VirtualPointCloudStreamReceiverTC.h"
#include "Device/Sensor/DeviceSensorStreamCodec.h"
#include "Device/Transport/VirtualPointCloudStreamReceiverTC.h"
#include "Device/Core/VirtualSensorPcdReceiveContract.h"
#include "Json.h"
#include "Device/Sensor/VirtualPcdCodec.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Device/Core/DeviceSensorLog.h"
UVirtualPointCloudStreamReceiverTC::UVirtualPointCloudStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_POINTCLOUD_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualPointCloudStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{ return DeviceSensorStreamCodec::ParsePointCloudJson(JsonString); }

TSharedPtr<FTransactionCodeDataBase> UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(const TArray<uint8>& Body,const TMap<FName,FString>& Headers)
{ return DeviceSensorStreamCodec::ParseBinaryPcd(Body,Headers); }

void UVirtualPointCloudStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualPointCloudStreamReceiverData> PointCloud = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Data);
	if (!PointCloud.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogDTDevice, Log, TEXT("[SensorTopicReceiver][PointCloud] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*PointCloud->SensorId, PointCloud->FrameId, PointCloud->MessageBytes,
			PointCloud->bValid ? TEXT("true") : TEXT("false"), *PointCloud->Message);
	}
}
