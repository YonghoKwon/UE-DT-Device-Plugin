# DTCore + DTDevice 시작하기 (UE 5.3 / Windows DX12)

센서·카메라와 기본 모니터를 붙이는 최소 절차입니다. DTCore는 필수입니다. 프로젝트 업무 UI나 Slab 시나리오 코드는 필요하지 않습니다. 검증한 DTCore 커밋은 `bc60f05e2d1e64f4ffe1410cf3239fe24cb4d4c9`이며 DTDevice 버전 조합은 [Compatibility.md](Compatibility.md)를 확인합니다.

## 1. 내부망 미러와 프로젝트 연결

인터넷에 연결된 준비 PC에서 저장소별로 한 번 수행합니다. 아래 URL은 공개 원본 예시이며 내부 주소·자격증명을 커밋하지 않습니다.

```powershell
git clone --mirror https://github.com/YonghoKwon/UE-DT-Core-Plugin.git DTCore.git
git -C DTCore.git push --mirror <내부-DTCore-저장소>
git clone --mirror https://github.com/YonghoKwon/UE-DT-Device-Plugin.git DTDevice.git
git -C DTDevice.git push --mirror <내부-DTDevice-저장소>
```

`push --mirror`는 **새 빈 미러 저장소에만** 사용합니다. 기존 저장소에는 삭제까지 반영되므로 관리자의 미러 정책을 따릅니다. 외부 다운로드 없이 내부 Git과 설치된 UE 5.3만으로 아래를 진행할 수 있습니다.

빈 **C++** 프로젝트 루트에서 두 저장소를 연결합니다. 원본을 쓴다면 아래 공개 주소를, 내부망에서는 미러 주소를 사용합니다.

```powershell
git submodule add https://github.com/YonghoKwon/UE-DT-Core-Plugin.git Plugins/DTCore
git submodule add https://github.com/YonghoKwon/UE-DT-Device-Plugin.git Plugins/DTDevice
git -C Plugins/DTCore checkout bc60f05e2d1e64f4ffe1410cf3239fe24cb4d4c9
# DTDevice는 Compatibility.md의 검증한 커밋/태그를 선택하고 parent gitlink를 함께 기록합니다.
```

이미 연결된 프로젝트를 받았다면 `git clone --recursive ...` 또는 `git submodule update --init Plugins/DTCore Plugins/DTDevice`로 **프로젝트가 고정한** 커밋을 받습니다. DTCore 소스를 임의로 고치거나 최신으로 자동 갱신하지 않습니다.

## 2. 프로젝트 설정

`.uproject`의 `Plugins` 배열에 추가합니다.

```json
{ "Name": "DTCore", "Enabled": true },
{ "Name": "DTDevice", "Enabled": true }
```

게임 모듈의 `Build.cs` 의존성에 `DTCore`, `DeviceRuntime`, `DeviceUI`를 추가합니다. `DeviceNative`는 Runtime의 의존성으로 연결됩니다. 화면 없는 워커에는 `DeviceUI`를 추가하지 않아도 됩니다.

`Config/DefaultEngine.ini`:

```ini
[/Script/EngineSettings.GameMapsSettings]
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

기존 프로젝트는 GameMode를 무조건 교체하지 말고 DTCore 기반 GameMode/PlayerController를 사용하고 있는지 확인합니다. `Config/DefaultGame.ini`에 패키지 cook 범위를 명시합니다.

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/DTDevice/Sensor")
```

플러그인 에셋은 다음 세 개입니다. 기존 프로젝트 경로를 쓰던 참조는 호스트 `PackageRedirects`와 플러그인 `DefaultDTDevice.ini`가 이어 줍니다. 원본 에셋을 다시 저장할 필요가 없습니다.

- `/DTDevice/Sensor/Materials/M_LidarSemanticId`
- `/DTDevice/Sensor/VFX/NS_VirtualLidarPointCloud`
- `/DTDevice/Sensor/VFX/M_VirtualLidarPointSprite`

## 3. 업무 의미 분류는 호스트에서 등록

게임 모듈의 `StartupModule()`에서 센서가 시작하기 전에 한 번 등록합니다. 프레임 처리 중에는 변경하지 않습니다. Tag만으로 동작하는 최소 예시입니다.

```cpp
#include "Device/Sensor/DeviceSemanticPolicy.h"
#include "Device/Sensor/VirtualLidarScanComp.h"

FVirtualLidarSemanticClassRule Slab;
Slab.Label=TEXT("Slab"); Slab.ActorTags={TEXT("Slab")};
Slab.DisplayColor=FLinearColor(1,.25f,.02f);
FVirtualLidarSemanticClassRule Floor;
Floor.Label=TEXT("Floor"); Floor.ActorTags={TEXT("Floor")};
Floor.DisplayColor=FLinearColor(.2f,.25f,1);
DeviceSemanticPolicy::Configure({Slab,Floor},
    {{TEXT("Slab"),.2f},{TEXT("Floor"),.25f}},TEXT("Slab"),TEXT("Slab"));
```

새 소비 프로젝트의 예시 정책입니다. 기존 과제의 wire ID와 순서를 이 예시에 맞춰 교체하면 안 됩니다.

## 4. 센서 배치 → Play

1. 바닥에 엔진 `Plane`, 앞에 `Cube`를 배치합니다. Actor Tags에 각각 `Floor`, `Slab`을 넣고 충돌을 켭니다. LiDAR는 X축 전방, 단위는 UE cm입니다.
2. `VirtualLidarSensorActor`를 바닥보다 위, Cube 앞에 놓습니다. `SimulationQuality=RealTimePreview`, `MultiHit=false`, CSV/JSONL/PCD 자동 파일 export는 끕니다. 점군 미리보기는 켭니다. 서버 PCD 정책을 미리보기 점 수에 맞춰 줄이지 않습니다.
3. `VirtualCameraSensorActor`와 Directional Light/Sky Light, Player Start를 배치합니다.
4. `DeviceSensorMonitorHostActor`를 **한 개** 배치합니다. `Show On Begin Play=true`, `Panel Class=DxDeviceSensorMonitorPanel`이 기본값입니다. 별도의 Level Blueprint `Create Widget`은 추가하지 않습니다.
5. Editor Development를 빌드하고 Play합니다. 왼쪽 패널에서 센서를 선택하면 ID·주기·hit/점 수·서버/미리보기 점 수·상태와 영상이 표시됩니다. `1회 캡처`는 선택 센서의 기존 수동 캡처 API를 씁니다. 미리보기 점 증감은 측정 해상도와 서버 payload를 바꾸지 않습니다.

BP로 외형만 바꾸려면 `DxDeviceSensorMonitorPanel`을 부모로 WBP를 만들고 Host의 `Panel Class`에 지정합니다. 기본 C++ 레이아웃을 그대로 쓰려면 Designer Root를 비워 둡니다. 직접 Designer를 구성할 경우 공개 `RefreshSensors`, `SelectSensor`, `RefreshStatus`, `CaptureOnce`, `AdjustPreview` API에 연결합니다. 프로젝트 전용 패널을 복사할 필요가 없습니다.

## 5. 재현 가능한 TestHost

플러그인 루트에서:

```powershell
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/setup_testhost.ps1 -DTCoreSource <고정-DTCore-폴더>
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/create_demo.ps1
TestHost/Open_TestHost.bat
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/run_measurements.ps1
```

데모는 `TestHost/Content/Maps/DTDeviceDemo.umap`입니다. Game/EditorStartup 기본 맵으로 열립니다. `create_demo.ps1 -Regenerate`는 이 **생성 전용 맵**을 다시 만들 때만 씁니다. 소비 프로젝트의 맵·기존 센서 에셋을 저장하는 스크립트가 아닙니다. TestHost Editor 모듈의 `CreateDTDeviceDemo` commandlet이 엔진 도형·센서·모니터만 생성합니다. 다른 빈 C++ 프로젝트에서 재현할 때는 이 Editor 모듈을 임시 프로젝트로 복사해 모듈/Editor Target에 등록한 뒤 같은 commandlet을 실행할 수 있습니다.

측정은 D3D12에서 기존 17개와 데모 모니터 스모크를 실행합니다. `MA0T10.DevicePlugin.DemoMonitorRhi`는 hit>0, Slab hit>0, 패널 생성·선택·상태 갱신·한 번 캡처·미리보기/서버 정책 분리를 확인합니다. NullRHI는 GPU 검증 통과 근거가 아닙니다. 실제 서버 송신 수용과 화면의 직접 마우스 점검은 자동화와 별도입니다.

## 6. 운영 참고

Native LiDAR 안전 상태는 소비 프로젝트의 `Saved/NativeLidarSafety`에 있습니다. startup oracle 실패나 장치 오류 때 지원 RHI로 전환합니다. 다중 게임 프로세스에는 고유 `-UserDir`을 사용합니다. 정상 장치의 일시적 제출 실패는 그 세션에서만 RHI입니다. 기존 안전 정책이나 파일을 검증을 위해 지우지 않습니다.

배포 시 플러그인 `Content`, `Config`, `Shaders`와 세 모듈을 포함합니다. 프로젝트 서버 설정·개인 키·토큰을 가이드, 에셋, archive, Git에 넣지 않습니다. 저장소 `LICENSE`는 Copyright (c) 2026 YonghoKwon, 모든 권리 보유입니다.

빈 C++ 소비 프로젝트 준비 예제: `TestHost/Scripts/prepare_guide_rehearsal.ps1 -DTCoreSource <고정-DTCore-폴더>`는 기존 파일을 덮지 않고 `TestHost/Saved/GuideRehearsal/GuideHost.uproject`를 만듭니다. 생성 후 해당 `GuideHostEditor`를 빌드하고 `-run=CreateDTDeviceDemo`로 데모를 생성한 뒤 엽니다. OPT-IN SocketFaults는 로컬 fault server를 주지 않으면 Skipped로 별도 보고합니다.

### 빈 프로젝트의 DTCore 업무 설정

이 최소 예제는 센서·모니터용입니다. 업무용 API/TransactionCode DataTable과 STOMP 서버를 구성하지 않은 새 프로젝트에서는 DTCore의 테이블 미설정·기본 연결 재시도 메시지가 남을 수 있습니다. 실제 업무 연동은 DTCore 문서에 따라 별도로 구성하며 공용 서버 설정을 자동으로 덮어쓰지 않습니다. 화면 확인용 자동 캡처는 `DisableAllScreenMessages`로 일시적인 화면 진단 문자열만 숨겼고, 원래 로그는 보존합니다. 이는 제품 기본값이나 센서 출력 정책의 변경이 아닙니다.


## 전용 센서 연결 재연결 정책 (DTCORE-EXT-09)

DTDevice 설정 `/Script/DeviceRuntime.DeviceSensorAssetSettings`의 `RawTcpReconnectInitialDelaySeconds`(1초), `RawTcpReconnectMaxDelaySeconds`(10초), `RawTcpReconnectBackoffMultiplier`(2)로 Raw TCP STOMP 재연결 지연을 정합니다. 기본 실패 간격은 기존과 같은 1/2/4/8/10/10초입니다. Engine 설정이며 DTCore 공유 WebSocket의 Reconnect 설정과 독립적입니다. 과제 DefaultGame.ini에 DTCore Reconnect 기본값을 자동으로 쓰지 않습니다.

설정은 연결 profile 생성 시 복사됩니다. 실행 중 워커는 UObject 설정을 읽지 않으며, 정책을 바꾸려면 새 profile로 전용 연결을 다시 준비합니다. 직접 `FVirtualSensorHighThroughputProfile`을 전달하는 소비자는 그 profile의 재연결 값을 사용할 수 있습니다. PCD wire/receipt/큐 상한과 과부하 backoff는 변경하지 않습니다.
