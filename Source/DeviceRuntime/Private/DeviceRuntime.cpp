#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"
#include "Misc/ConfigCacheIni.h"
#include "Device/Transport/VirtualSensorStreamReceiverRegistration.h"
class FDeviceRuntimeModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        // Moved packages retain their serialized Niagara imports; no asset resave is required.
        FCoreRedirects::ReadRedirectsFromIni(GConfig->GetConfigFilename(TEXT("DTDevice")));
        RegisterVirtualSensorStreamReceiverCodec();
    }
    void ShutdownModule() override {UnregisterVirtualSensorStreamReceiverCodec();}
};
IMPLEMENT_MODULE(FDeviceRuntimeModule,DeviceRuntime)
