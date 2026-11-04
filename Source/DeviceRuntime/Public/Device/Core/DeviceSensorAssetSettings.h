#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeviceSensorAssetSettings.generated.h"
class UMaterialInterface;
class UNiagaraSystem;
/** Explicit host soft paths. Defaults preserve existing project assets; no asset is moved or created. */
UCLASS(Config = Engine, DefaultConfig)
class DEVICERUNTIME_API UDeviceSensorAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UDeviceSensorAssetSettings();
	UPROPERTY(EditAnywhere, Config, Category = "Device assets")
	TSoftObjectPtr<UMaterialInterface> LidarSemanticMaterial;
	UPROPERTY(EditAnywhere, Config, Category = "Device assets")
	TSoftObjectPtr<UNiagaraSystem> LidarPointCloudSystem;

	/** Dedicated sensor connection policy; independent of DTCore's shared WebSocket. */
	UPROPERTY(EditAnywhere, Config, Category = "Device transport", meta = (ClampMin = "0.001"))
	double RawTcpReconnectInitialDelaySeconds = 1.0;
	UPROPERTY(EditAnywhere, Config, Category = "Device transport", meta = (ClampMin = "0.001"))
	double RawTcpReconnectMaxDelaySeconds = 10.0;
	UPROPERTY(EditAnywhere, Config, Category = "Device transport", meta = (ClampMin = "1.0"))
	double RawTcpReconnectBackoffMultiplier = 2.0;
};
