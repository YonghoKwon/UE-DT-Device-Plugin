#include "Device/Sensor/VirtualSensorOutputComponent.h"

#include "Device/Sensor/VirtualSensorRecorderComponent.h"
#include "Device/Sensor/VirtualSensorTransportComponent.h"
#include "Device/Core/VirtualSensorStreamPublisherComponent.h"

UVirtualSensorOutputComponent::UVirtualSensorOutputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualSensorOutputComponent::SetSharedServices(
	UVirtualSensorTransportComponent* InTransport,
	UVirtualSensorRecorderComponent* InRecorder,
	UVirtualSensorStreamPublisherComponent* InStreamPublisher)
{
	TransportComponent = InTransport;
	RecorderComponent = InRecorder;
	StreamPublisherComponent = InStreamPublisher;
}

bool UVirtualSensorOutputComponent::RouteFrame(const FVirtualSensorFrameEnvelope& Frame)
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
	if (StreamPublisherComponent)
	{
		StreamPublisherComponent->SubmitFrame(Frame);
	}
	else if (TransportComponent && Frame.bSendTransport)
	{
		TransportComponent->SendJson(Frame.SensorId, SensorType, LastJsonPayload);
	}
	if (RecorderComponent && Frame.bRecord)
	{
		RecorderComponent->RecordJsonFrame(Frame.SensorId, SensorType, Frame.FrameId, LastJsonPayload);
	}

	OnFrameRouted.Broadcast(Frame.SensorId, Frame.FrameId);
	return true;
}

bool UVirtualSensorOutputComponent::RouteAcquiredFrame(const FVirtualSensorFrameEnvelope& Frame)
{
	if (Frame.SensorId.IsEmpty() || !StreamPublisherComponent)
	{
		return false;
	}
	if (!Frame.PointSnapshot.IsValid() && !Frame.BinaryPayload.IsValid())
	{
		return false;
	}
	StreamPublisherComponent->SubmitFrame(Frame);
	return true;
}

FString UVirtualSensorOutputComponent::BuildSensorType(EVirtualSensorKind Kind) const
{
	return Kind == EVirtualSensorKind::Camera ? TEXT("virtual_camera") : TEXT("virtual_lidar");
}
