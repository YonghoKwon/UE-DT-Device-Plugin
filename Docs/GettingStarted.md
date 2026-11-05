# 빈 C++ 프로젝트에서 DTCore + DTDevice 시작하기

UE 5.3 / Win64 / D3D12 기준입니다. 현재 이름·include·설정·에셋을 소스와 정적으로 대조한 절차이며, 문서 정리 작업에서 새 프로젝트를 빌드하거나 화면을 다시 실행하지 않았습니다. 기본 연결과 검증 범위는 [Compatibility](Compatibility.md), 전송은 [DataChannels](DataChannels.md)를 따릅니다.

## 1. 저장소 연결

내부망이면 관리자가 마련한 두 미러 주소를 사용합니다. 인터넷 준비 PC의 `git clone --mirror <공개 URL>` → `git push --mirror <새 빈 내부 저장소>`는 관리자 미러 정책 안에서만 합니다. 기존 저장소에 mirror push를 임의로 실행하지 않습니다. 인증값은 URL에 넣지 않습니다.

빈 C++ 프로젝트 루트:

```powershell
git submodule add https://github.com/YonghoKwon/UE-DT-Core-Plugin.git Plugins/DTCore
git submodule add https://github.com/YonghoKwon/UE-DT-Device-Plugin.git Plugins/DTDevice
git -C Plugins/DTCore checkout e6644e2964ba3359c0689e3539cbdbdbf743b137
# DTDevice는 검증 조합에 맞는 정확한 SHA로 고정하고 parent gitlink에 기록합니다.
```

이미 고정한 host는 clone --recursive 또는 submodule update --init로 두 parent pin을 받습니다. DTCore를 DTDevice 안에 다시 넣거나 --remote로 임의 갱신하지 않습니다.

## 2. 프로젝트·모듈 설정

.uproject의 Plugins 배열에 DTCore와 DTDevice Enabled=true를 추가합니다. 게임 Build.cs의 PublicDependencyModuleNames에 Core/CoreUObject/Engine과 DTCore/DeviceRuntime을, 공통 화면이 필요하면 DeviceUI를 추가합니다. DeviceNative는 DeviceRuntime의 의존성입니다. UI 없는 worker에는 DeviceUI를 요구하지 않습니다.

Config/DefaultEngine.ini에 DTCore 기반 진입점을 설정합니다. 기존 프로젝트는 이미 연결된 host 정책을 무조건 교체하지 않습니다.

```ini
[/Script/EngineSettings.GameMapsSettings]
GameInstanceClass=/Script/DTCore.DxGameInstance
GlobalDefaultGameMode=/Script/DTCore.DxGameMode

[/Script/WindowsTargetPlatform.WindowsTargetSettings]
DefaultGraphicsRHI=DefaultGraphicsRHI_DX12
-D3D12TargetedShaderFormats=PCD3D_SM5
+D3D12TargetedShaderFormats=PCD3D_SM6

[/Script/Engine.RendererSettings]
r.RayTracing=True
r.SkinCache.CompileShaders=True
r.CustomDepth=3
r.Lumen.HardwareRayTracing=False
```

Config/DefaultGame.ini:

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/DTDevice/Sensor")
```

필수 에셋은 `/DTDevice/Sensor/Materials/M_LidarSemanticId`, `/DTDevice/Sensor/VFX/NS_VirtualLidarPointCloud`, `/DTDevice/Sensor/VFX/M_VirtualLidarPointSprite`입니다. DeviceSensorAssetSettings의 soft path와 cook 목록을 맞춥니다. 이전 host 참조는 PackageRedirects로 연결하며 기존 에셋을 resave하지 않습니다. 셰이더는 플러그인의 Shaders에서 기존 virtual `/MA0T10` 경로로 매핑됩니다.

## 3. 의미 분류 등록

host 게임 모듈 StartupModule에서 센서 시작 전에 한 번 등록합니다. 프레임마다 바꾸지 않습니다.

```cpp
#include "Device/Sensor/DeviceSemanticPolicy.h"
#include "Device/Sensor/VirtualLidarScanComp.h"

FVirtualLidarSemanticClassRule Slab;
Slab.Label = TEXT("Slab");
Slab.ActorTags = {TEXT("Slab")};
Slab.DisplayColor = FLinearColor(1, .25f, .02f);
FVirtualLidarSemanticClassRule Floor;
Floor.Label = TEXT("Floor");
Floor.ActorTags = {TEXT("Floor")};
Floor.DisplayColor = FLinearColor(.2f, .25f, 1);
DeviceSemanticPolicy::Configure({Slab, Floor},
    {{TEXT("Slab"), .2f}, {TEXT("Floor"), .25f}}, TEXT("Slab"), TEXT("Slab"));
```

이는 새 소비 프로젝트의 최소 예시입니다. 기존 과제의 class 순서·wire ID·Coil alias를 이 예시에 맞춰 덮지 않습니다. 고급 host는 identity/point-cloud analysis/panel bridge의 중립 extension을 구현합니다.

## 4. 배치·Play와 공통 패널

1. 엔진 Plane/Floor와 앞쪽 Cube를 두고 각각 Actor Tags Floor/Slab 및 충돌을 설정합니다. 단위는 UE cm, LiDAR X축은 전방입니다.
2. AVirtualLidarSensorActor와 AVirtualCameraSensorActor, 조명·PlayerStart를 배치합니다. LiDAR 미리보기는 켜되 파일 export/송신은 목적에 맞게 명시 선택합니다. 미리보기 점 수로 서버 PCD를 줄이지 않습니다.
3. ADeviceSensorMonitorHostActor를 한 개 놓습니다. bShowOnBeginPlay=true, PanelClass=UDxDeviceSensorMonitorPanel이 기본입니다. 별도 Level Blueprint CreateWidget 경로를 중복 추가하지 않습니다.
4. Editor Development 빌드 후 Play합니다. 센서 선택 → 상태·hit/점 수·카메라 영상 → 1회 캡처를 확인합니다. 이 단계의 직접 화면 수용은 자동 테스트와 별도로 기록합니다.
5. 외형을 바꾸는 WBP는 UDxDeviceSensorMonitorPanel 부모를 사용합니다. Designer Root가 비어 있으면 기본 C++ 레이아웃을 사용합니다. 직접 조립할 경우 RefreshSensors/SelectSensor/RefreshStatus/CaptureOnce/AdjustPreview API에 연결합니다.

기존 과제 패널은 UDxDeviceSensorPanelBase/body를 쓰는 host façade이며 plugin 독립 monitor를 복사해 별도 위젯 관리자를 만들지 않습니다. `NavigateTo("Sensor:<ID>")`와 NavigateBack은 같은 host bridge/소유권 안에서 사용합니다.

## 5. 전송과 TestHost

[DataChannels](DataChannels.md)에서 프로젝트 토픽 별칭과 DTCore 연결 설정을 분리합니다. Default 저빈도 JSON 선택은 UVirtualSensorTransportComp의 DTCoreWebSocket 모드입니다. 20Hz PCD는 sensor-bulk 기존 worker 경로를 유지합니다.

플러그인 루트에서:

```powershell
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/setup_testhost.ps1 -DTCoreSource <고정-DTCore-폴더> -SkipBuild
# 빌드까지 준비하려면 -SkipBuild 생략
TestHost/Open_TestHost.bat
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/run_measurements.ps1
```

Source/Ref 우선순위는 명령줄 → TestHost.local.ini → 환경 변수 → 검증한 공개 기본값입니다. 로컬 폴더 source는 링크만 하고 checkout하지 않습니다. 기본 데모는 TestHost/Content/Maps/DTDeviceDemo.umap입니다. create_demo.ps1 -Regenerate는 생성 전용 demo를 다시 만들 때만 사용하며 운영맵·기존 에셋을 저장하는 일반 준비 단계가 아닙니다.

run_measurements는 D3D12의 golden/geometry/cadence/readback/PCD/camera/native/에셋·demo monitor를 검사합니다. 실제 확장 테스트 수/skip/Hz·source pin을 Saved/Measurements/<UTC>/summary.json으로 기록합니다. NullRHI·합성·직접 마우스·회사 서버 수용을 구분합니다. TestHost는 실제 DTCore source를 사용하며 테스트용 가짜 DTCore를 생성하지 않습니다.
