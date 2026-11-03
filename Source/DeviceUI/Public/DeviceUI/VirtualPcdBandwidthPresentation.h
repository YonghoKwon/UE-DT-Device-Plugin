#pragma once

#include "CoreMinimal.h"
#include "Device/Core/VirtualPcdBandwidthMetrics.h"

struct FVirtualPcdBandwidthPresentation
{
 int32 PointBytes=33, SensorCount=0, ActiveCount=0, UnmeasuredCount=0;
 double SelectedMaximum=0, TotalMaximum=0, BudgetMiB=80;
 FVirtualPcdBandwidthSample Selected, Total;
 DEVICEUI_API FText Text() const;
 DEVICEUI_API FText Details() const;
};
