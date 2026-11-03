#include "Modules/ModuleManager.h"
#include "Device/Transport/VirtualSensorStreamReceiverRegistration.h"
class FDeviceRuntimeModule final : public IModuleInterface
{
public:
    void StartupModule() override {RegisterVirtualSensorStreamReceiverCodec();}
    void ShutdownModule() override {UnregisterVirtualSensorStreamReceiverCodec();}
};
IMPLEMENT_MODULE(FDeviceRuntimeModule,DeviceRuntime)
