#include "Modules/ModuleManager.h"
#include "Device/Sensor/DeviceSemanticPolicy.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
class FDTDeviceTestHostModule final : public FDefaultGameModuleImpl
{
    void StartupModule() override
    {
        FVirtualLidarSemanticClassRule Slab; Slab.Label=TEXT("Slab"); Slab.ActorTags={TEXT("Slab")}; Slab.DisplayColor=FLinearColor(1,.25f,.02f);
        FVirtualLidarSemanticClassRule Floor; Floor.Label=TEXT("Floor"); Floor.ActorTags={TEXT("Floor")}; Floor.DisplayColor=FLinearColor(.2f,.25f,1);
        DeviceSemanticPolicy::Configure({Slab,Floor},{{TEXT("Slab"),.2f},{TEXT("Floor"),.25f}},TEXT("Slab"),TEXT("Slab"));
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FDTDeviceTestHostModule, DTDeviceTestHost, "DTDeviceTestHost");
