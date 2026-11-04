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
	LidarCsvReplayComp = CreateDefaultSubobject<ULidarCsvReplaySourceComp>(TEXT("LidarCsvReplay"));
	LidarJsonLinesReplayComp = CreateDefaultSubobject<ULidarJsonLinesReplaySourceComp>(TEXT("LidarJsonLinesReplay"));
	LidarBufferedJsonComp = CreateDefaultSubobject<ULidarJsonLiveSourceComp>(TEXT("LidarBufferedJson"));
	LidarHttpJsonComp = CreateDefaultSubobject<ULidarHttpJsonLiveSourceComp>(TEXT("LidarHttpJson"));
	LidarUdpJsonComp = CreateDefaultSubobject<ULidarUdpJsonLiveSourceComp>(TEXT("LidarUdpJson"));
	CameraBufferedJsonComp = CreateDefaultSubobject<UCameraJsonLiveSourceComp>(TEXT("CameraBufferedJson"));
	for (URealSensorSourceComp* Source : { static_cast<URealSensorSourceComp*>(LidarCsvReplayComp), static_cast<URealSensorSourceComp*>(LidarJsonLinesReplayComp), static_cast<URealSensorSourceComp*>(LidarBufferedJsonComp), static_cast<URealSensorSourceComp*>(LidarHttpJsonComp), static_cast<URealSensorSourceComp*>(LidarUdpJsonComp), static_cast<URealSensorSourceComp*>(CameraBufferedJsonComp) })
	{
		Source->SetupAttachment(Root);
		Source->bAutoStartSource = false;
		Source->bSendTransportByDefault = false;
	}
	LidarCsvReplayComp->SourceId = TEXT("Demo-LiDAR-CSV");
	LidarCsvReplayComp->CsvFilePath = TEXT("Samples/slab_replay_sample.csv");
	LidarJsonLinesReplayComp->SourceId = TEXT("Demo-LiDAR-JSONL");
	LidarBufferedJsonComp->SourceId = TEXT("Live-LiDAR-Buffered");
	LidarJsonLinesReplayComp->JsonLinesFilePath = TEXT("Samples/slab_replay_sample.jsonl");
	LidarHttpJsonComp->SourceId = TEXT("Live-LiDAR-HTTP");
	LidarHttpJsonComp->ListenPort = 8082;
	LidarHttpJsonComp->RoutePath = TEXT("/ma0t10/lidar/live");
	LidarUdpJsonComp->SourceId = TEXT("Live-LiDAR-UDP");
	LidarUdpJsonComp->BindAddress = TEXT("127.0.0.1");
	LidarUdpJsonComp->BindPort = 0;
	CameraBufferedJsonComp->SourceId = TEXT("Live-Camera-JSON");
}

void AVirtualSensorExternalSourceHostActor::BeginPlay()
{
	Super::BeginPlay();
	bEndingPlay = false;
	AVirtualLidarSensorActor* Lidar = nullptr;
	AVirtualCameraSensorActor* Camera = nullptr;
	for (TActorIterator<AVirtualLidarSensorActor> It(GetWorld()); It; ++It) { Lidar = *It; break; }
	for (TActorIterator<AVirtualCameraSensorActor> It(GetWorld()); It; ++It) { Camera = *It; break; }
	for (URealSensorSourceComp* Source : { static_cast<URealSensorSourceComp*>(LidarCsvReplayComp), static_cast<URealSensorSourceComp*>(LidarJsonLinesReplayComp), static_cast<URealSensorSourceComp*>(LidarBufferedJsonComp), static_cast<URealSensorSourceComp*>(LidarHttpJsonComp), static_cast<URealSensorSourceComp*>(LidarUdpJsonComp) })
	{
		Source->TargetSensorActor = Lidar;
		Source->TargetLidarComp = Lidar ? Lidar->ScanComp : nullptr;
	}
	CameraBufferedJsonComp->TargetSensorActor = Camera;
	CameraBufferedJsonComp->TargetCameraComp = Camera ? Camera->CaptureComp : nullptr;

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
