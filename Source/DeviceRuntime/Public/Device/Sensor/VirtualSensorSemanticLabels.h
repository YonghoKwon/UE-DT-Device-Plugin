#pragma once
#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
struct FVirtualLidarSemanticClassRule;

/** Canonical project defaults. Existing per-LiDAR overrides remain supported. */
namespace VirtualSensorSemanticLabels
{
DEVICERUNTIME_API const TArray<FVirtualLidarSemanticClassRule>& Defaults();
DEVICERUNTIME_API bool Matches(const FVirtualLidarSemanticClassRule& Rule, const AActor* Actor, const UPrimitiveComponent* Component);
DEVICERUNTIME_API FName Resolve(const UPrimitiveComponent* Component);
// Stable IDs: 0 background/invalid, 1 Other, 2..6 original defaults, 7 Coil.
DEVICERUNTIME_API uint8 ClassId(FName Label);
DEVICERUNTIME_API FName Label(uint8 Id);
DEVICERUNTIME_API FColor Color(uint8 Id);
}
