#pragma once
#include "CoreMinimal.h"
struct FVirtualLidarSemanticClassRule;
/** Configure once at module startup, before acquisition; do not mutate while readers are active. */
namespace DeviceSemanticPolicy
{
    DEVICERUNTIME_API void Configure(TArray<FVirtualLidarSemanticClassRule> Rules,TMap<FName,float> Reflectances,FName DiagnosticLabel,FName DiagnosticTag);
    DEVICERUNTIME_API const TArray<FVirtualLidarSemanticClassRule>& Rules();
    DEVICERUNTIME_API float Reflectance(FName Label);
    DEVICERUNTIME_API FName DiagnosticLabel();
    DEVICERUNTIME_API FName DiagnosticTag();
}
