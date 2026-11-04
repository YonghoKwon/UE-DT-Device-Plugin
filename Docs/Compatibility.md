# 호환성

| 항목 | 값 |
|---|---|
| Unreal Engine | 5.3 |
| 플랫폼 / RHI | Win64 / D3D12 (native 레이트레이싱은 DXR Tier 1.1 필요) |
| DTCore | `1604ee72a52362c7e39d969ca1c3e631cfdaa3c3` (UE-DT-Core-Plugin) |
| 검증 호스트 | UE-DT-Project (기준 커밋은 첫 이관 PR에서 기록) |
| native 실측 장치 | NVIDIA GeForce RTX 4070 Ti / 드라이버 621.01 |

- DTCore 버전을 올리는 일은 별도 승인이 필요하고, 이 플러그인과 호스트 양쪽에서 다시 검증해야 합니다.
- 전송 데이터(PCD 33바이트 little-endian 레코드, v1/v2 헤더), Topic, SaveGame 슬롯·버전, Blueprint 핀, enum 순서는 구조를 옮기는 이관 작업에서 바꾸지 않습니다. 바꿀 때는 major 버전과 명시적인 migration으로 다룹니다.
- 공통 에셋(센서 semantic 재질, 점군 Niagara, 스프라이트 재질)은 첫 이관에서 호스트 프로젝트에 그대로 남겨 두고, 플러그인은 경로 설정으로 참조합니다. 이 에셋을 플러그인으로 옮기는 일은 별도 승인을 받은 뒤에 진행합니다.

2026-11-04 r54c: DTCore main `1604ee72a52362c7e39d969ca1c3e631cfdaa3c3`을 검증 기준으로 사용합니다. 승인된 660740f 기능과 54050fd 신규 테스트를 포함하며, 이후 host/TestHost/release 검증은 이 main pin을 기준으로 기록합니다.
