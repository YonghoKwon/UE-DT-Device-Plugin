#include "Device/Sensor/DeviceSemanticPolicy.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
namespace { TArray<FVirtualLidarSemanticClassRule> GDeviceSemanticHostRules; TMap<FName,float> GDeviceSemanticHostReflectances; FName GDeviceSemanticDiagnosticLabel,GDeviceSemanticDiagnosticTag; }
void DeviceSemanticPolicy::Configure(TArray<FVirtualLidarSemanticClassRule> Rules,TMap<FName,float> Reflectances,FName DiagnosticLabel,FName DiagnosticTag)
{ check(IsInGameThread()); GDeviceSemanticHostRules=MoveTemp(Rules); GDeviceSemanticHostReflectances=MoveTemp(Reflectances);GDeviceSemanticDiagnosticLabel=DiagnosticLabel;GDeviceSemanticDiagnosticTag=DiagnosticTag; }
const TArray<FVirtualLidarSemanticClassRule>& DeviceSemanticPolicy::Rules() { return GDeviceSemanticHostRules; }
float DeviceSemanticPolicy::Reflectance(FName Label) { const float* Value=GDeviceSemanticHostReflectances.Find(Label);return Value?*Value:.20f; }
FName DeviceSemanticPolicy::DiagnosticLabel() { return GDeviceSemanticDiagnosticLabel; }
FName DeviceSemanticPolicy::DiagnosticTag() { return GDeviceSemanticDiagnosticTag; }
