# DTDevice — UE-DT-Device-Plugin

DTCore 필수 기반 가상 LiDAR·카메라 플러그인입니다. 장치 분리·공통 에셋·Comp/Dx 이름·포맷·데이터 채널 통합이 완료됐습니다. 과제 Slab/해치/코일/학습/Workspace와 과제 UI는 소비 프로젝트에 둡니다. 선택적 DTCore 없는 모드는 없습니다.

## 요구·모듈

UE 5.3 / Win64 / D3D12, native RT는 DXR inline ray query Tier1.1과 시작 검증을 요구합니다. 검증 DTCore는 **e6644e2964ba3359c0689e3539cbdbdbf743b137**입니다. [Compatibility](Docs/Compatibility.md)의 증거 범위를 따릅니다.

| 모듈 | LoadingPhase | 책임 |
|---|---|---|
| DeviceNative | PostConfigInit | 지속 RT 장면·자원 수명, native 안전 정책, plugin Shaders 매핑 |
| DeviceRuntime | Default | 장치·스케줄러·정규화·코덱·전송·파일·중립 계약; UI 없는 worker 지원 |
| DeviceUI | None(명시 로드) | UDxDeviceSensorPanelBase 및 공통 body, UDxDeviceSensorMonitorPanel/Host |

플러그인이 소비 프로젝트 모듈을 include/UHT 역참조하지 않습니다. host semantic/provider/bridge를 사용하며 실행 상태는 공통 base/body 한 곳이 소유합니다.

## 연결·시작

```powershell
git submodule add https://github.com/YonghoKwon/UE-DT-Core-Plugin.git Plugins/DTCore
git submodule add https://github.com/YonghoKwon/UE-DT-Device-Plugin.git Plugins/DTDevice
# 이미 연결된 소비 프로젝트를 받았다면 parent가 고정한 두 gitlink를 초기화:
git submodule update --init Plugins/DTCore Plugins/DTDevice
```

DTCore를 이 저장소 안에 중첩하지 않습니다. 소비 host는 정확한 두 SHA를 고정합니다. [.uplugin](DTDevice.uplugin), [빈 C++ 프로젝트 절차](Docs/GettingStarted.md)를 따릅니다.

공통 에셋은 [Content/Sensor](Content/Sensor)에 있습니다. semantic 재질·Niagara·sprite 경로는 `/DTDevice/Sensor/...`입니다. 과제 WBP·맵·SaveGame은 host에 남고 기존 참조는 최종 이름/패키지 redirects로 호환합니다. [DefaultDTDevice.ini](Config/DefaultDTDevice.ini)를 유지하며 기존 자산을 resave하지 않습니다.

## 데이터 채널과 설정

DTCore는 연결/등록/재연결/상태를 소유하고 프로젝트 Game 설정은 destination 정책을 소유합니다. `Default`는 공유 저빈도 JSON, `sensor`는 UDxDeviceSensorStompChannel, `sensor-bulk`는 UDxDeviceSensorBulkChannel의 기존 Worker 위임입니다. 기본 PCD·33-byte wire·FIFO·reservation·receipt 의미는 유지합니다.

공통/플랫폼/패키지 loose 위치, 토픽 별칭·복수 구독·중복 경고, 빈 설정의 SensorTransport.ini 대체는 [DataChannels](Docs/DataChannels.md)를 따릅니다. 비밀값을 ini·로그·압축물·커밋에 넣지 않습니다.

## TestHost

[TestHost](TestHost)는 DTCore+DTDevice만 쓰는 최소 프로젝트입니다. 기본 맵은 [DTDeviceDemo](TestHost/Content/Maps/DTDeviceDemo.umap)이며 LiDAR·카메라·엔진 도형·기본 모니터를 제공합니다.

```powershell
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/setup_testhost.ps1 -DTCoreSource <고정-DTCore-폴더> -SkipBuild
# 빌드까지 준비하려면 -SkipBuild를 생략합니다.
TestHost/Open_TestHost.bat
powershell -ExecutionPolicy Bypass -File TestHost/Scripts/run_measurements.ps1
```

DTCore source 우선순위는 명령줄 → TestHost.local.ini → DTCORE_SOURCE/DTCORE_REF → 공개 원본/검증 SHA입니다. 로컬 폴더는 junction 연결만 하며 checkout하지 않습니다. 주소·인증값을 커밋하지 않습니다. `create_demo.ps1 -Regenerate`는 생성 전용 demo를 다시 만들 때만 사용합니다.

기본 필터는 golden·형상·wall clock·readback·PCD20Hz·주기 저장·카메라 GT·native oracle/안전·에셋/demo 모니터를 검사합니다. 필터가 확장한 실제 테스트 수와 skip은 [summary](Docs/Compatibility.md)를 기준으로 보고합니다. NullRHI는 GPU 통과가 아니며 직접 마우스/실장비/회사 수용은 별도입니다.

## 이름·스타일·라이선스

Comp 접미사와 UDxDeviceSensorPanelBase/UDxDeviceSensorMonitorPanel 이름, [.clang-format](.clang-format), [.git-blame-ignore-revs](.git-blame-ignore-revs)를 사용합니다. 순수 포맷의 공백 외 토큰 차이를 [token_compare](Tools/Style/token_compare.py)로 확인합니다. payload 키·schemaVersion·로그/테스트 이름·저장 필드·wire는 이름 정리로 바꾸지 않습니다.

저작권 및 사용·배포 조건은 [LICENSE](LICENSE)를 따릅니다. 세부 계획·옛 문서는 [Docs/archive](Docs/archive)에 원문을 보존합니다.
