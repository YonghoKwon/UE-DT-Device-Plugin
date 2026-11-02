#include "SensorOwnedRayTracing.h"
#include "Modules/ModuleManager.h"

// PostConfigInit: shader directory mapping must be installed before global shader types register.
class FDeviceNativeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override { SensorOwnedRayTracing::RegisterShaders(); }
	virtual void ShutdownModule() override { SensorOwnedRayTracing::Shutdown(); }
};

IMPLEMENT_MODULE(FDeviceNativeModule, DeviceNative);
