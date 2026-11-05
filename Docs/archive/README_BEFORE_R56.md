> **이전 문서 원문, 현재 기준 아님.** 현재 문서는 상위 Docs 및 저장소 README를 따릅니다.

# DT Device (UE-DT-Device-Plugin)

DTCore 기반 가상 장치 플러그인입니다. 가상 LiDAR와 가상 카메라의 획득·전송·공통 패널을 담습니다. 과제별 화면과 업무 로직(Slab, 해치, 코일, 학습 배치, 작업 공간 등)은 사용하는 프로젝트 쪽에 둡니다.

> **상태: 0.1.0 scaffold.** 모듈 구조만 있고 소스는 아직 옮기지 않았습니다. 이관 계획과 진행 상황은 사용 프로젝트(UE-DT-Project)의 `docs/DEVICE_PLUGIN_01_DESIGN.md`를 따릅니다.

## 요구 사항

- Unreal Engine 5.3, Win64, D3D12
- **DTCore 필수:** [UE-DT-Core-Plugin](https://github.com/YonghoKwon/UE-DT-Core-Plugin)을 같은 프로젝트의 `Plugins/DTCore`에 둡니다. 이 저장소 안에 DTCore를 중첩하지 않습니다.
- 검증한 DTCore 커밋은 `Docs/Compatibility.md`에 정확한 SHA로 기록합니다.

## 모듈

| 모듈 | LoadingPhase | 역할 |
|---|---|---|
| `DeviceNative` | PostConfigInit | native 레이트레이싱 획득, 셰이더 디렉터리 매핑, native 안전 정책 |
| `DeviceRuntime` | Default | 센서·카메라, 스케줄러, 코덱, 전송, 파일 저장, 중립 계약. 화면 없는 워커에서도 동작합니다. |
| `DeviceUI` | None (명시 로드) | DTCore `UDxWidget` 기반의 공통 센서 설정·모니터·캡처 패널. UI가 없는 호스트는 이 모듈을 쓰지 않아도 됩니다. |

## 프로젝트에 연결

사용 프로젝트에는 서브모듈로 붙이고, 정확한 커밋 SHA로 고정합니다(태그 이름이 아닙니다).

```bash
git submodule add https://github.com/YonghoKwon/UE-DT-Device-Plugin.git Plugins/DTDevice
```

`.uproject`에서 `DTCore`와 `DTDevice`를 모두 활성화합니다.

## 라이선스

저작권 고지는 [LICENSE](../../LICENSE)를 따릅니다. 저작권자의 사전 서면 허가 없이 사용·복제·수정·배포할 수 없습니다.

## TestHost (플러그인만으로 확인)

`TestHost/`는 DTCore와 DTDevice만으로 장치 기능을 빌드하고 측정해 보는 최소 프로젝트입니다. UE-DT-Project 없이도 플러그인이 동작하는지 확인할 때 씁니다.

가장 간단한 방법은 `TestHost\Open_TestHost.bat`을 더블클릭하는 것입니다. 필요한 준비(DTCore 받기, DTDevice 연결, 빌드)를 한 뒤 Editor를 엽니다. 이미 준비된 PC에서는 확인만 하고 바로 엽니다.

**DTCore를 가져올 곳 지정** (위에 있는 것이 우선)
1. 명령줄: `Open_TestHost.bat <주소 또는 폴더> [브랜치/태그/커밋]`
2. `TestHost/TestHost.local.ini`: `TestHost.local.ini.example`을 복사해 `[DTCore] Source=`, `Ref=`를 적습니다. 이 파일은 git에 올라가지 않으므로 내부망 주소를 적어도 됩니다.
3. 환경 변수: `DTCORE_SOURCE`, `DTCORE_REF`
4. 기본값: 공개 UE-DT-Core-Plugin의 검증된 커밋

`Source`에는 git 주소(내부망 GitLab, Bitbucket, 파일 공유 저장소 등)나 이미 받아 둔 DTCore 폴더(`DTCore.uplugin`이 있는 폴더)를 적습니다. 폴더를 지정하면 받아 오지 않고 그대로 연결합니다. 검증된 커밋과 다른 DTCore를 쓰면 경고가 나오므로, 그때는 `run_measurements.ps1`로 다시 확인합니다.

```powershell
powershell -ExecutionPolicy Bypass -File TestHost\Scripts\setup_testhost.ps1     # 준비만 (-DTCoreSource, -DTCoreRef, -SkipBuild 사용 가능)
powershell -ExecutionPolicy Bypass -File TestHost\Scripts\run_measurements.ps1   # 최소 측정 자동화(D3D12), Saved\Measurements\<UTC>\summary.json
```

- DTCore는 `TestHost/Plugins/DTCore`에 고정 커밋으로 받습니다. 이 저장소에는 넣지 않습니다(git 제외).
- 이 저장소 자체는 `TestHost/Plugins/DTDevice` junction으로 연결되므로, 플러그인 코드를 고치면 TestHost에 바로 반영됩니다.
- `DTDeviceTestHost.uproject`를 열면 Unreal Editor에서 직접 확인할 수 있습니다.
- 기본 측정 항목: 전송 데이터 golden, LiDAR 형상, 획득 주기와 backpressure, GPU readback 재사용, PCD 20 Hz 진단, 주기 캡처, 카메라 내부 파라미터, native/RHI startup oracle, native 안전 정책, 셰이더 경로.
- 공통 에셋(semantic 재질, 점군 Niagara)과 일부 테스트 fixture는 아직 UE-DT-Project에 있습니다. 그래서 semantic PSO 회귀와 SensorReplay 테스트는 UE-DT-Project에서 실행합니다.
