#pragma once

#include "CoreMinimal.h"
class UDTCoreSettings;
class UDeviceDataChannelSettings;

namespace DeviceDataChannelConfig
{
// Preserve UE array operators, but exclude unrelated sections and credential values.
FString FilterProjectOverrides(const FString& Text);
bool ApplyBuffers(const FString& Common, const FString& Platform, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics);
// First entry wins, matching DTCore's runtime-override candidate ordering.
TArray<FString> RuntimeCandidatePaths(const FString& ProjectDir, const FString& LaunchDir, const FString& ProjectName, const FString& Platform, bool bPlatform);
bool ApplyRuntimeBuffers(const FString& DefaultGame, const TArray<FString>& CommonByPriority, const TArray<FString>& PlatformByPriority, UDTCoreSettings& Connections, UDeviceDataChannelSettings& Topics);
void ApplyLoosePackageOverrides();
}
