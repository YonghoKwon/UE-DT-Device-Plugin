#include "Device/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "Device/Sensor/DeviceSensorCoordinator.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"
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
#include "Device/Sensor/CameraJsonLiveSourceComp.h"
#include "Device/Sensor/LidarCsvReplaySourceComp.h"
#include "Device/Sensor/LidarHttpJsonLiveSourceComp.h"
#include "Device/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "Device/Sensor/LidarJsonLiveSourceComp.h"
#include "Device/Sensor/LidarUdpJsonLiveSourceComp.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "WebSocket/TransactionCodeMessage.h"

AVirtualSensorExternalSourceHostActor::AVirtualSensorExternalSourceHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	LidarCsvReplay = CreateDefaultSubobject<ULidarCsvReplaySourceComp>(TEXT("LidarCsvReplay"));
	LidarJsonLinesReplay = CreateDefaultSubobject<ULidarJsonLinesReplaySourceComp>(TEXT("LidarJsonLinesReplay"));
	LidarBufferedJson = CreateDefaultSubobject<ULidarJsonLiveSourceComp>(TEXT("LidarBufferedJson"));
	LidarHttpJson = CreateDefaultSubobject<ULidarHttpJsonLiveSourceComp>(TEXT("LidarHttpJson"));
	LidarUdpJson = CreateDefaultSubobject<ULidarUdpJsonLiveSourceComp>(TEXT("LidarUdpJson"));
	CameraBufferedJson = CreateDefaultSubobject<UCameraJsonLiveSourceComp>(TEXT("CameraBufferedJson"));
	for (URealSensorSourceComp* Source : { static_cast<URealSensorSourceComp*>(LidarCsvReplay), static_cast<URealSensorSourceComp*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComp*>(LidarBufferedJson), static_cast<URealSensorSourceComp*>(LidarHttpJson), static_cast<URealSensorSourceComp*>(LidarUdpJson), static_cast<URealSensorSourceComp*>(CameraBufferedJson) })
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
	for (URealSensorSourceComp* Source : { static_cast<URealSensorSourceComp*>(LidarCsvReplay), static_cast<URealSensorSourceComp*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComp*>(LidarBufferedJson), static_cast<URealSensorSourceComp*>(LidarHttpJson), static_cast<URealSensorSourceComp*>(LidarUdpJson) })
	{
		Source->TargetSensorActor = Lidar;
		Source->TargetLidar = Lidar ? Lidar->ScanComp : nullptr;
	}
	CameraBufferedJson->TargetSensorActor = Camera;
	CameraBufferedJson->TargetCamera = Camera ? Camera->CaptureComp : nullptr;

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
