#include "Device/Contracts/DeviceIdentityPolicy.h"
namespace { TArray<FDeviceIdentityAlias> GDeviceIdentityHostAliases; }
void DeviceIdentityPolicy::Configure(TArray<FDeviceIdentityAlias> Aliases)
{ check(IsInGameThread()); GDeviceIdentityHostAliases=MoveTemp(Aliases); }
const TArray<FDeviceIdentityAlias>& DeviceIdentityPolicy::Aliases() { return GDeviceIdentityHostAliases; }
FString DeviceIdentityPolicy::Canonical(const FString& Id)
{
    for(const auto& Alias:GDeviceIdentityHostAliases)if(Id.Equals(Alias.Legacy,ESearchCase::CaseSensitive))return Alias.Canonical;
    return Id;
}
bool DeviceIdentityPolicy::ContainsLegacy(const FString& Text)
{
    for(const auto& Alias:GDeviceIdentityHostAliases)if(Text.Contains(Alias.Legacy,ESearchCase::CaseSensitive))return true;
    return false;
}
