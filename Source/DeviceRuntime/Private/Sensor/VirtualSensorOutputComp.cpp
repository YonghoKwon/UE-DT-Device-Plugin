#include "Device/Sensor/VirtualSensorOutputComp.h"

#include "Device/Sensor/VirtualSensorRecorderComp.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"

UVirtualSensorOutputComp::UVirtualSensorOutputComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorOutputComp::SetSharedServices(
    UVirtualSensorTransportComp* InTransport,
    UVirtualSensorRecorderComp* InRecorder,
    UVirtualSensorStreamPublisherComp* InStreamPublisher)
{
	TransportComp = InTransport;
	RecorderComp = InRecorder;
	StreamPublisherComp = InStreamPublisher;
}

bool UVirtualSensorOutputComp::RouteFrame(const FVirtualSensorFrameEnvelope& Frame)
{
	if (!Frame.HasJsonPayload() || Frame.SensorId.IsEmpty())
	{
		return false;
	}

	if (LastRoutedSensorId == Frame.SensorId && LastRoutedFrameId == Frame.FrameId)
	{
		return false;
	}

	LastRoutedSensorId = Frame.SensorId;
	LastRoutedFrameId = Frame.FrameId;
	LastJsonPayload = *Frame.JsonPayload;

	const FString SensorType = BuildSensorType(Frame.SensorKind);
	if (StreamPublisherComp)
	{
		StreamPublisherComp->SubmitFrame(Frame);
	}
	else if (TransportComp && Frame.bSendTransport)
	{
		TransportComp->SendJson(Frame.SensorId, SensorType, LastJsonPayload);
	}
	if (RecorderComp && Frame.bRecord)
	{
		RecorderComp->RecordJsonFrame(Frame.SensorId, SensorType, Frame.FrameId, LastJsonPayload);
	}

	OnFrameRouted.Broadcast(Frame.SensorId, Frame.FrameId);
	return true;
}

bool UVirtualSensorOutputComp::RouteAcquiredFrame(const FVirtualSensorFrameEnvelope& Frame)
{
	if (Frame.SensorId.IsEmpty() || !StreamPublisherComp)
	{
		return false;
	}
	if (!Frame.PointSnapshot.IsValid() && !Frame.BinaryPayload.IsValid())
	{
		return false;
	}
	StreamPublisherComp->SubmitFrame(Frame);
	return true;
}

FString UVirtualSensorOutputComp::BuildSensorType(EVirtualSensorKind Kind) const
{
	return Kind == EVirtualSensorKind::Camera ? TEXT("virtual_camera") : TEXT("virtual_lidar");
}
