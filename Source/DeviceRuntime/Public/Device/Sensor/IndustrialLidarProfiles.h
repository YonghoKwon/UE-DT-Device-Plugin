#pragma once
#include "Device/Sensor/VirtualSensorDeviceProfileTypes.h"

/** Published envelopes, not a vendor calibration or packet implementation. */
namespace IndustrialLidar
{
struct FSpec
{
	EVirtualLidarDeviceProfile Profile;
	const TCHAR *Maker, *Model, *Source, *Pattern, *Conditions;
	int32 Beams, Columns, Rows, PointRate, Echoes;
	float HFov, Bottom, Top, MinMeters, RangeMeters, Hz, PrecisionMM, Wavelength;
};
DEVICERUNTIME_API TConstArrayView<FSpec> Specs();
DEVICERUNTIME_API const FSpec* Find(EVirtualLidarDeviceProfile Profile);
DEVICERUNTIME_API bool Resolve(EVirtualLidarDeviceProfile Profile, EVirtualSensorSimulationQuality Quality, FVirtualLidarProfilePreset& Out);
/** Physical channel layout for spinning sensors; deterministic nonrepeating
 * envelope approximation for Livox. Frame phase is fixed at acquisition start. */
DEVICERUNTIME_API FRotator Beam(EVirtualLidarDeviceProfile Profile, int32 Ray, int32 Width, int32 Height, int64 Frame, float Pitch, float Yaw);
}
