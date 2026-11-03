#pragma once

#include "CoreTypes.h"

// Literal-only defaults. No project services, UObject types, or runtime policy.
namespace VirtualSensorDefaults
{
inline constexpr TCHAR CameraTopic[] = TEXT("topic.virtual.sensor.camera.0");
inline constexpr TCHAR LidarTopic[] = TEXT("topic.virtual.sensor.lidar.0");
inline constexpr TCHAR PointCloudTopic[] = TEXT("topic.virtual.sensor.export.0");
// Default Rev7 x3 measured PCD demand is about 75.9 MiB/s at 20 Hz.
inline constexpr float PointCloudBandwidthMiBPerSecond = 80.0f;


inline constexpr TCHAR CaptureDirectory[] = TEXT("SensorCaptures");
inline constexpr TCHAR RecordingDirectory[] = TEXT("SensorRecordings");
inline constexpr TCHAR PlacementDirectory[] = TEXT("SensorPlacements");
inline constexpr TCHAR LayoutDirectory[] = TEXT("SensorLayouts");
inline constexpr TCHAR StreamTempDirectory[] = TEXT("SensorStreamTemp");
inline constexpr TCHAR ReportsDirectory[] = TEXT("Reports");
inline constexpr TCHAR TimedCaptureDirectory[] = TEXT("SensorCaptures/LocalTimedCapture");
inline constexpr TCHAR TimedCaptureAutomationDirectory[] = TEXT("SensorCaptures/LocalTimedCapture/Automation");
inline constexpr TCHAR PointCloudSubdirectory[] = TEXT("PointCloud");
inline constexpr TCHAR ServerPayloadSubdirectory[] = TEXT("ServerPayload");
}
