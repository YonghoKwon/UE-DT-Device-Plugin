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

이 저장소에는 라이선스 파일이 없습니다. UE-DT-Project와 같은 정책이며, 저작권자가 모든 권리를 보유합니다.
