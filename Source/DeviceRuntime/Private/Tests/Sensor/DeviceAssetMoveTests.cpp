#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Device/Core/DeviceSensorAssetSettings.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceMovedAssets,"MA0T10.DevicePlugin.MovedAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceMovedAssets::RunTest(const FString&)
{
    const auto* Settings=GetDefault<UDeviceSensorAssetSettings>();
    auto* Semantic=Settings->LidarSemanticMaterial.LoadSynchronous();
    auto* System=Settings->LidarPointCloudSystem.LoadSynchronous();
    auto* Sprite=LoadObject<UMaterialInterface>(nullptr,TEXT("/DTDevice/Sensor/VFX/M_VirtualLidarPointSprite.M_VirtualLidarPointSprite"));
    TestNotNull(TEXT("plugin semantic material"),Semantic); TestNotNull(TEXT("plugin Niagara"),System); TestNotNull(TEXT("plugin sprite"),Sprite);
    TestEqual(TEXT("legacy semantic resolves without resave"),LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId")),Semantic);
    TestEqual(TEXT("legacy Niagara resolves without resave"),LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud.NS_VirtualLidarPointCloud")),System);
    TestEqual(TEXT("serialized old sprite import resolves"),LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Sensor/VFX/M_VirtualLidarPointSprite.M_VirtualLidarPointSprite")),Sprite);
    return true;
}
#endif
