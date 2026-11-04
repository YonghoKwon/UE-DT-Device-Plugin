#pragma once

#include "CoreMinimal.h"
class UDTCoreSettings;
class UDeviceDataChannelSettings;

namespace DeviceDataChannelConfig
{
// Preserve UE array operators, but exclude unrelated sections and credential values.
FString FilterProjectOverrides(const FString& Text);
bool ApplyBuffers(const FString& Common, const FString& Platform, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics);
void ApplyLoosePackageOverrides();
}
