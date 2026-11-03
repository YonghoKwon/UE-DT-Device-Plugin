#include "Device/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "Device/Sensor/DeviceSensorCoordinator.h"
#include "Device/Core/VirtualSensorStreamPublisherComponent.h"
#include "Device/Core/VirtualSensorHighThroughputTransportSubsystem.h"

#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Core/DTCoreSettings.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "IStompClient.h"
#include "IStompMessage.h"
#include "StompModule.h"
#include "Device/Camera/VirtualCameraSensorActor.h"
#include "Device/Sensor/CameraJsonLiveSourceComponent.h"
#include "Device/Sensor/LidarCsvReplaySourceComponent.h"
#include "Device/Sensor/LidarHttpJsonLiveSourceComponent.h"
#include "Device/Sensor/LidarJsonLinesReplaySourceComponent.h"
#include "Device/Sensor/LidarJsonLiveSourceComponent.h"
#include "Device/Sensor/LidarUdpJsonLiveSourceComponent.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "WebSocket/TransactionCodeMessage.h"

AVirtualSensorExternalSourceHostActor::AVirtualSensorExternalSourceHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	LidarCsvReplay = CreateDefaultSubobject<ULidarCsvReplaySourceComponent>(TEXT("LidarCsvReplay"));
	LidarJsonLinesReplay = CreateDefaultSubobject<ULidarJsonLinesReplaySourceComponent>(TEXT("LidarJsonLinesReplay"));
	LidarBufferedJson = CreateDefaultSubobject<ULidarJsonLiveSourceComponent>(TEXT("LidarBufferedJson"));
	LidarHttpJson = CreateDefaultSubobject<ULidarHttpJsonLiveSourceComponent>(TEXT("LidarHttpJson"));
	LidarUdpJson = CreateDefaultSubobject<ULidarUdpJsonLiveSourceComponent>(TEXT("LidarUdpJson"));
	CameraBufferedJson = CreateDefaultSubobject<UCameraJsonLiveSourceComponent>(TEXT("CameraBufferedJson"));
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson), static_cast<URealSensorSourceComponent*>(CameraBufferedJson) })
	{
		Source->SetupAttachment(Root);
		Source->bAutoStartSource = false;
		Source->bSendTransportByDefault = false;
	}
	LidarCsvReplay->SourceId = TEXT("Demo-LiDAR-CSV");
	LidarCsvReplay->CsvFilePath = TEXT("Samples/slab_replay_sample.csv");
	LidarJsonLinesReplay->SourceId = TEXT("Demo-LiDAR-JSONL");
	LidarBufferedJson->SourceId = TEXT("Live-LiDAR-Buffered");
	LidarJsonLinesReplay->JsonLinesFilePath = TEXT("Samples/slab_replay_sample.jsonl");
	LidarHttpJson->SourceId = TEXT("Live-LiDAR-HTTP");
	LidarHttpJson->ListenPort = 8082;
	LidarHttpJson->RoutePath = TEXT("/ma0t10/lidar/live");
	LidarUdpJson->SourceId = TEXT("Live-LiDAR-UDP");
	LidarUdpJson->BindAddress = TEXT("127.0.0.1");
	LidarUdpJson->BindPort = 0;
	CameraBufferedJson->SourceId = TEXT("Live-Camera-JSON");
}

void AVirtualSensorExternalSourceHostActor::BeginPlay()
{
	Super::BeginPlay();
	bEndingPlay = false;
	AVirtualLidarSensorActor* Lidar = nullptr;
	AVirtualCameraSensorActor* Camera = nullptr;
	for (TActorIterator<AVirtualLidarSensorActor> It(GetWorld()); It; ++It) { Lidar = *It; break; }
	for (TActorIterator<AVirtualCameraSensorActor> It(GetWorld()); It; ++It) { Camera = *It; break; }
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson) })
	{
		Source->TargetSensorActor = Lidar;
		Source->TargetLidar = Lidar ? Lidar->ScanComponent : nullptr;
	}
	CameraBufferedJson->TargetSensorActor = Camera;
	CameraBufferedJson->TargetCamera = Camera ? Camera->CaptureComponent : nullptr;

	InitializeTopicReceiverRuntime();
	if (bAutoStartTopicReceivers)
	{
		bTopicReceiversRequested = true;
		GetWorldTimerManager().SetTimer(
			TopicReceiverRetryTimer,
			this,
			&AVirtualSensorExternalSourceHostActor::AttemptTopicSubscriptions,
			FMath::Max(0.01f, InitialSubscribeDelaySeconds),
			false);
	}
}

void AVirtualSensorExternalSourceHostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopTopicReceivers();
	GetWorldTimerManager().ClearTimer(ReceiverRefreshTimer);
	if(BoundPublisher.IsValid()) BoundPublisher->OnStreamConfigurationChanged.Remove(PublisherChangeHandle);
	if(auto* Raw=GetWorld()->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()) Raw->OnReceived.Remove(SharedReceiveHandle);
	Super::EndPlay(EndPlayReason);
}
