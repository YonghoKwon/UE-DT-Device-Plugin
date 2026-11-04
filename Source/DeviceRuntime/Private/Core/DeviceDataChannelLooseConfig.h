#pragma once

#include "CoreMinimal.h"

namespace DeviceDataChannelConfig
{
// Preserve UE array operators, but exclude unrelated sections and credential values.
FString FilterProjectOverrides(const FString& Text);
void ApplyLoosePackageOverrides();
}
