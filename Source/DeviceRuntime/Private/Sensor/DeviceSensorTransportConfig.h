#pragma once

#include "CoreMinimal.h"

struct FVirtualSensorTransportProfile;

namespace DeviceSensorTransportConfig
{
// 기존 직접 INI 파서를 공유한다. URL의 //, 첫 non-empty 파일, 빈 키 의미를 그대로 유지한다.
bool ReadValues(const TArray<FString>& CandidateFiles, bool bUseGameIniFallback, TMap<FString, FString>& Values, FString& Source);
bool ReadLegacyConnection(FVirtualSensorTransportProfile& Profile, FString& SessionPasscode);
}
