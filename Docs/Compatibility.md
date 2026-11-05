# 호환성과 검증 범위

| 항목 | 현재 값 |
|---|---|
| Unreal Engine | 5.3 |
| 실행 플랫폼/RHI | Win64 / D3D12 |
| native 요구 | DXR inline ray query Tier1.1, 장치 건강 상태·adapter/driver startup oracle |
| DTCore 고정 기준 | `cb9788a2338ad92e6a651be71330f2abe4b63a90` (main) |
| 소비 host | UE-DT-Project, #99~#104 완료 |
| 장치 코드 검증 pin | `dca79e341a1efdafcd3ed7831f25896429cca016` (이번 문서는 그 코드 위 문서 변경) |

DTCore cb9788a는 이전 데이터 채널 검증 tree와 동일하며 channel base/STOMP/등록/상태 API를 포함합니다. TestHost setup의 validatedCommit과 소비 host의 guard 기준을 함께 유지합니다. 임의 최신 pin이나 DTCore 소스 수정은 승인·별도 검증 없이 하지 않습니다.

## 최근 증거(새 문서 빌드가 아님)

- r55e host fb19e681 / Device458e77a: TestHost D3D12 **43 Success /1 Skip/0 Fail**, Development 공통·플랫폼·재시작 설정3/3 및 pak sensor 행 URL 편집 재시작 PASS.
- r55f host023c2444 / Devicedca79e3: Editor PASS, 유효 release **536 Success /46 Skip/0 Fail**, 고유578·누락0; 맵10개 읽기 전용 경고0; 두 guard PASS.
- 458e77a→dca79e3은 TestHost/.gitignore의 생성 빈 Game.ini 한 줄뿐이고 runtime 코드 차이0입니다. 원본 상세와 미수용 범위는 소비 host의 검증 이력·Known Issues를 따릅니다. 장시간 메모리/현장 수용까지 전체 PASS라고 주장하지 않습니다.

기본 TestHost 필터의 고정 경로 수와 확장 옵션을 준 실제 실행 수는 다릅니다. 선택한 필터·SHA·RHI·skip·summary.json을 함께 기록합니다. 이 플러그인 문서 정리에서는 새로운 실행/측정을 하지 않았습니다.

## 계약·확장 경계

- PCD33-byte little-endian, 기존 v1/v2 헤더, destination/metadata 의미, SaveGame·BP 핀·enum 순서는 유지합니다. 변경 시 명시 버전·adapter·fixture·migration을 별도로 승인받습니다.
- 공통 에셋은 이미 Content/Sensor로 이동했습니다. host 자산·UI façade·과제 정책은 host에 남고 redirects는 최종1-hop을 유지합니다.
- 지원되는 vendor/driver identity의 미검증 native 조합은 제한된 startup oracle 뒤 캐시합니다. 검증된 GPU 한 종만을 일반 지원 범위로 오해하지 않습니다.
- UI 없는 worker는 DeviceRuntime만 사용하고, 공통 패널을 쓸 때 DeviceUI를 명시적으로 로드합니다. DTCore는 항상 필수입니다.
- Linux ini 예시는 설정 형식이며 현재 Linux 실행/패키지 지원을 검증한 것이 아닙니다.
