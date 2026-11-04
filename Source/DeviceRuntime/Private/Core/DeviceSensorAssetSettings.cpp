#include "Device/Core/DeviceSensorAssetSettings.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
UDeviceSensorAssetSettings::UDeviceSensorAssetSettings()
{
	LidarSemanticMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/DTDevice/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId")));
	LidarPointCloudSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/DTDevice/Sensor/VFX/NS_VirtualLidarPointCloud.NS_VirtualLidarPointCloud")));
}
