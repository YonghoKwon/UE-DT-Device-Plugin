#include "Device/Sensor/VirtualLidarExportComp.h"

#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"

UVirtualLidarExportComp::UVirtualLidarExportComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}
bool UVirtualLidarExportComp::SerializeImmutableFrame(const FVirtualSensorFrameEnvelope& Frame, const FVirtualSensorStreamConfig& Config, FString& Extension, TArray<uint8>& Bytes, int32& PointCount, FString& Error)
{
	FVirtualSensorStreamConfig FileConfig = Config;
	FileConfig.PcdSendOptions = FVirtualPcdSendOptions(); // Export/Recorder always preserve Full binary storage.
	const bool Success = UVirtualSensorStreamPublisherComp::SerializePointCloudFrame(Frame, FileConfig, Extension, Bytes, PointCount, Error);
	if (!Success && Error.IsEmpty())
		Error = TEXT("선택 형식으로 저장할 검출점이 없거나 프레임 변환에 실패했습니다.");
	return Success;
}

void UVirtualLidarExportComp::BindScanComponent(UVirtualLidarScanComp* InScanComponent)
{
	ScanComp = InScanComponent;
}

bool UVirtualLidarExportComp::ExportCsv(const FString& FileNamePrefix) const
{
	return ScanComp && ScanComp->ExportLastPointCloudCsv(FileNamePrefix);
}

bool UVirtualLidarExportComp::ExportJsonLines(const FString& FileNamePrefix) const
{
	return ScanComp && ScanComp->ExportLastPointCloudJsonLines(FileNamePrefix);
}

bool UVirtualLidarExportComp::ExportPcd(const FString& FileNamePrefix) const
{
	return ScanComp && ScanComp->ExportLastPointCloudPcd(FileNamePrefix);
}

bool UVirtualLidarExportComp::ExportLas(const FString& FileNamePrefix) const
{
	return ScanComp && ScanComp->ExportLastPointCloudLas(FileNamePrefix);
}

bool UVirtualLidarExportComp::ExportLaz(const FString& FileNamePrefix) const
{
	return ScanComp && ScanComp->ExportLastPointCloudLaz(FileNamePrefix);
}

FString UVirtualLidarExportComp::GetLastExportPath() const
{
	return ScanComp ? ScanComp->GetLastPointCloudExportPath() : FString();
}
