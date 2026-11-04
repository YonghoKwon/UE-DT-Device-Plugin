#include "CreateDTDeviceDemoCommandlet.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Camera/VirtualCameraSensorActor.h"
#include "Device/Camera/VirtualCameraCaptureComp.h"
#include "DeviceUI/DeviceSensorMonitorHostActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerStart.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

UCreateDTDeviceDemoCommandlet::UCreateDTDeviceDemoCommandlet()
{ IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UCreateDTDeviceDemoCommandlet::Main(const FString& Params)
{
    // Explicit destination allows the documented clean-project rehearsal under Saved as well.
    const FString PackageName=TEXT("/Game/Maps/DTDeviceDemo");
    const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetMapPackageExtension());
    if (FPaths::FileExists(Filename) && !FParse::Param(*Params,TEXT("Regenerate")))
    { UE_LOG(LogTemp,Error,TEXT("Demo exists; use -Regenerate only for the owned generated demo.")); return 1; }
    UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World) return 2;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!Cube || !Plane) return 3;
    auto AddShape=[&](const TCHAR* Label,UStaticMesh* Mesh,FVector Position,FVector Scale,FName Tag)
    {
        auto* Actor=World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator);
        Actor->SetActorLabel(Label); Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh); Actor->SetActorScale3D(Scale); Actor->Tags.Add(Tag);
    };
    AddShape(TEXT("Floor"),Plane,FVector(700,0,0),FVector(50,50,1),TEXT("Floor"));
    AddShape(TEXT("Slab"),Cube,FVector(1000,0,100),FVector(8,4,2),TEXT("Slab"));
    World->SpawnActor<ADirectionalLight>(FVector(0,0,1000),FRotator(-45,-30,0));
    World->SpawnActor<ASkyLight>();
    World->SpawnActor<APlayerStart>(FVector(-800,-1200,650),FRotator(-12,45,0));
    auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>(FVector(0,0,300),FRotator(-5,0,0));
    Lidar->SetActorLabel(TEXT("Demo LiDAR"));
    Lidar->ScanComp->ApplySimulationQuality(EVirtualSensorSimulationQuality::RealTimePreview);
    Lidar->ScanComp->bUseMultiHit=false;
    Lidar->ScanComp->SetPointCloudPreviewEnabled(true);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>(FVector(-400,-600,450),FRotator(-10,25,0));
    Camera->SetActorLabel(TEXT("Demo Camera"));
    World->SpawnActor<ADeviceSensorMonitorHostActor>();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World,PackageName)) return 4;
    UE_LOG(LogTemp,Display,TEXT("DTDEVICE_DEMO_CREATED %s"),*PackageName); return 0;
}
