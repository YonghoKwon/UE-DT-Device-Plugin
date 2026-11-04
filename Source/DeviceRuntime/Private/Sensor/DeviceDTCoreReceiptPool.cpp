#include "DeviceDTCoreReceiptPool.h"
#include "DeviceDTCorePublishReceipt.h"
bool UDeviceDTCoreReceiptPool::Reserve(UDeviceDTCorePublishReceipt* Receipt)
{
	if (!IsInGameThread() || !Receipt || !HasCapacity())
		return false;
	Pending.Add(Receipt);
	return true;
}
void UDeviceDTCoreReceiptPool::Release(UDeviceDTCorePublishReceipt* Receipt)
{
	Pending.Remove(Receipt);
}
void UDeviceDTCoreReceiptPool::Deinitialize()
{
	const auto Retired = MoveTemp(Pending);
	for (UDeviceDTCorePublishReceipt* Receipt : Retired)
		if (Receipt)
			Receipt->Cancel();
	Super::Deinitialize();
}
