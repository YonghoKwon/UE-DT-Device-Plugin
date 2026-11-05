# 장치 데이터 채널과 프로젝트 토픽 정책

DTCore main **f05f25f6ebb8bacda0e5b0dc6e1c5d5d88ae44ee** 기반입니다. 연결·등록·상태·재연결 메커니즘은 DTCore, 어떤 destination을 사용하는지는 소비 프로젝트가 소유합니다. 과제 TC/DataTable을 공통 장치 코드로 복사하지 않습니다.

## 채널·수명

| 이름 | 구현 | 범위 |
|---|---|---|
| Default | DTCore 기존 공유 채널 adapter | 명시 선택한 저빈도 상태/수동1회/결과 JSON |
| sensor | UDxDeviceSensorStompChannel : UDxStompChannel | 기존 JSON/수동 binary/receipt·ACK |
| sensor-bulk | UDxDeviceSensorBulkChannel : UDxDataChannelBase | Worker 정책, 기존 raw TCP STOMP worker 위임 |

UDxDeviceSensorChannelSubsystem이 GameInstance에서 sensor/sensor-bulk를 등록합니다. 기본 등록은 연결·tick·thread·구독을 시작하지 않습니다. 기존 World subsystem이 worker/socket을 소유하고 World 종료 시 detach/stop합니다. 채널이 죽은 World를 보유하지 않습니다.

PublishFrame은 기존 immutable frame·공유 body·reservation을 전달합니다. generic PublishBinary에는 획득 metadata/token이 없으므로 bulk 송신에 사용하지 않습니다. parser·SEND·큐 상한·receipt·종료 알고리즘은 그대로입니다. 큰 body 조립·socket IO·검증을 GT로 이동하지 않습니다.

sensor의 matching legacy owner는 연결을 공유하고 충돌 profile은 다른 owner를 바꾸지 않고 거절합니다. 마지막 legacy owner 해제는 기존 수명대로 disconnect하며 명시 named auto-connect는 GI 소유입니다. 연결 세대/weak callback·receipt deadline·late 결과 보호를 유지합니다.

## 연결과 토픽 설정

```ini
[/Script/DTCore.DTCoreSettings]
+DataChannels=(ChannelName="sensor",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorStompChannel",Url="ws://127.0.0.1:61614",bAutoConnect=False,bUseDefaultCredentials=False,LoginOverrideKey="SensorLogin",PasscodeOverrideKey="SensorPasscode")
+DataChannels=(ChannelName="sensor-bulk",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorBulkChannel",Url="tcp://127.0.0.1:61616",bAutoConnect=False,bUseDefaultCredentials=False,LoginOverrideKey="BulkLogin",PasscodeOverrideKey="BulkPasscode")

[/Script/DeviceRuntime.DeviceDataChannelSettings]
+Channels=(ChannelName="sensor",Destinations=((Alias="status",Destination="topic.site.status",bSubscribe=True,Purpose="status"),(Alias="result",Destination="topic.site.result",bSubscribe=True,Purpose="result")))
+Channels=(ChannelName="sensor-bulk",Destinations=((Alias="pointcloud",Destination="topic.virtual.sensor.export.0",bSubscribe=False,Purpose="binary PCD")))
```

Default는 DataChannels의 예약 이름이므로 넣지 않습니다. 기존 WebSocketUrl 등 공유 설정과 Channels의 Default 토픽 정책을 사용합니다. URL의 `//`가 ini 주석이 되지 않게 인용합니다. 실제 비밀번호·토큰은 넣지 않고 기존 Runtime Override 키만 참조합니다.

UDeviceDataChannelSettings(Config=Game)의 Channels에는 ChannelName, Destinations(Alias/Destination/bSubscribe/Purpose)가 있습니다. 코드 별칭은 camera/lidar/pointcloud/imu/ack이고 추가 수신 별칭을 둘 수 있습니다. 새 항목/값이 비면 기존 SensorTransport.ini와 map/UI profile을 유지합니다. 기본 목록은 빈 상태여서 기존 server 이름과 기본 self-consumption off를 보존합니다.

같은 채널의 같은 destination은 한 번만 구독합니다(일반 callback은 첫 수신 별칭). sensor/ack는 기존 AckReceived callback/key를 공유하며 명시 bSubscribe=False를 존중합니다. absent/empty ack는 기존 AckTopic 동작을 따릅니다. 서로 다른 채널의 명시 구독 설정이 겹치면 시작 경고·상태 문자열을 제공하지만 연결을 막지 않습니다. 후발 임의 구독을 역추적하는 전체 broker 감사는 아닙니다.

sensor는 DTCore Subscribe를 반복하고 자동 재구독합니다. Default는 공유 연결의 새 세대마다 다시 요청합니다. registry의 OnTopicMessage는 ChannelName/Alias/Message를 host에 전달하며 별도 업무 TC dispatcher를 만들지 않습니다. bulk 구독 목록은 worker의 기존 receiver 구독과 합칩니다. 기존 receiver selection이 디코딩·PCD 검증을 결정하며 두 번째 큰 body GT 전달 경로를 추가하지 않습니다.

## 공통·플랫폼·패키지 loose

- Config/DefaultGame.ini
- Config/Windows/WindowsGame.ini (플랫폼 우선)
- Config/Linux/LinuxGame.ini는 설정 형식 예시이며 Linux 패키지 검증이 아님
- Windows 배포: Windows/<프로젝트>/Config/DefaultGame.ini와 Config/Windows/WindowsGame.ini

게임 종료 → loose 편집 → 재시작합니다. 지원 속성은 플랫폼 loose > 공통 loose > 기존 pak/사용자 설정 순서입니다. **공통 파일에 DataChannels/Channels 목록이 있으면 pak 목록을 대체**합니다. **같은 채널 이름은 뒤 행이 대체**하므로 URL만 고친 행이 옛 pak 행에 가려지지 않습니다. 플랫폼 파일은 이름별 부분 override를 할 수 있고, `!DataChannels=ClearArray`/`!Channels=ClearArray`는 전체 목록을 비우려는 경우에만 씁니다.

DTDevice는 두 물리 파일(각2MiB 이하)에서 다음 whitelist만 읽고 별도 소유 config에서 해석합니다. 전역 GConfig는 복사·교체·저장하지 않습니다.

- DTCoreSettings: DataChannels, WebSocketUrl, MaxReconnectAttempts, ReconnectInitialDelaySeconds, ReconnectMaxDelaySeconds, ReconnectBackoffMultiplier, SubscribeReceiptTimeoutSeconds.
- DeviceDataChannelSettings: Channels.

다른 DTCore 섹션·직접 인증값·맵 설정 전체 loose 적용은 범위 밖입니다. Runtime Override URL/credential 해석은 이후에도 기존 규칙을 유지합니다. named entry가 없으면 legacy profile이 유지되고, named URL이 비면 DTCore 기본 URL 상속 규칙을 따릅니다. positive Reconnect*는 채널 우선이며 bulk의 zero/empty는 DeviceSensorAssetSettings의 기존1s/10s/×2 기본을 유지합니다.

Development 전용 -DeviceChannelConfigProbe는 registry에 반영된 설정 snapshot을 Saved/Reports/device-channel-config.json에 기록하고 종료합니다. loopback 외 URL은 가리고 인증값은 쓰지 않습니다. 기본 실행에는 작업을 등록하지 않으며 Shipping에는 probe body가 없습니다. r55e에서 pak sensor 행의 URL만 바꾼 loose 사본으로 같은 UserDir 재시작 후 실제 channel 설정 변경을 확인했습니다. live broker handshake 수용을 뜻하지 않습니다.

## 결과 의미·불변 계약

Published는 worker 제출, Dropped는 접수 거절/terminal delivery 실패, Backpressure는 기존 밀림/overload 관측입니다. broker receipt와 소비자 ACK는 별도입니다. 정상 FIFO·전역128MiB/스트림64MiB·Camera8/기타20프레임 상한을 유지합니다. 장애 전환의 무유실을 보장하지 않습니다.

PCD destination/기존 EnginePcd·RawPcd 헤더·33-byte body를 자동 정규화하거나 dx-* header로 교체하지 않습니다. make-destination helper는 기존 기본값과의 일치를 확인하는 데 사용하며 custom destination을 임의 제한하지 않습니다. Default 모드는 연속 camera/telemetry/pointcloud JSON을 거절하고20Hz PCD를 공유 연결로 우회하지 않습니다. receipt 슬롯16개와 timeout 후 late 결과·세대 보호는 기존 어댑터 정책을 유지합니다.

## 현장 Game.ini 경로와 우선순위 (STRUCTURE-12B)

기존 `Config/DefaultGame.ini` 읽기를 유지하며, 채널·토픽도 DTCore `TryReadRuntimeOverride`와 같은 런타임 파일 후보를 읽는다. 패키지를 종료한 뒤 수정하고 재시작한다. 이 기능은 연결 속성 allowlist와 `UDeviceDataChannelSettings.Channels`에만 적용하며 인증값·다른 DTCore 섹션·전역 GConfig는 가져오거나 저장하지 않는다.

높은 우선순위부터 아래 순서다. 중복 절대경로는 한 번만 읽는다.

1. `<ProjectDir>/Config/Game.ini`
2. `<LaunchDir>/Config/Game.ini`
3. `<LaunchDir>/../../Config/Game.ini`
4. `<LaunchDir>/<ProjectName>/Config/Game.ini`
5. 위 네 루트의 `<Platform>/<Platform>Game.ini` (Windows/WindowsGame.ini 또는 Linux/LinuxGame.ini)
6. `<ProjectConfigDir>/DefaultGame.ini`
7. 기존 pak/사용자 설정. 새 채널/토픽 설정이 비어 있으면 기존 SensorTransport.ini/profile 대체 경로를 유지한다.

Game.ini의 공통 목록은 낮은 우선순위 목록을 대체한다. 플랫폼 목록은 기존처럼 지정한 채널 행만 덮어쓰며 나머지를 유지한다. 같은 채널 이름의 마지막 행이 앞 행을 대체한다. 전체 목록 초기화가 필요하면 `!DataChannels=ClearArray` 또는 `!Channels=ClearArray`를 명시한다. `Game.ini`가 없으면 기존 DefaultGame.ini→플랫폼 동작과 동일하다. URL 문자열은 ini에서 `WebSocketUrl="ws://127.0.0.1:61616"`처럼 인용한다.

Windows 현장 권장 배치는 `Windows/ma0t10_dt/Config/Game.ini`다. 기존 DefaultGame.ini·Windows/WindowsGame.ini 배치도 계속 지원한다. 기존 채널/토픽 예제의 섹션을 Game.ini에 그대로 둘 수 있다. URL/키 이름만 설정하며 비밀번호·토큰은 ini에 저장하지 않고 기존 Override/session 전달만 사용한다. Linux는 같은 경로 규칙을 사용하며 실제 패키지 검증 여부는 실행 보고서를 확인한다.

## JSON을 Default로 공유하는 선택 (STRUCTURE-12D)

기본 전송 모드는 바꾸지 않는다. 기존 `EVirtualSensorTransportMode::DTCoreWebSocket`을 선택하면 저빈도 JSON은 Default, 20Hz PCD는 sensor-bulk로 보낸다. 이 모드의 준비/재연결 요청은 sensor 채널을 획득하지 않으며 Default의 재연결·재구독 정책을 DTCore에 맡긴다. 기존 StompWebSocket 모드는 sensor를 계속 사용한다.

공유 구성을 선택할 때 sensor 연결을 자동으로 열지 않도록 다음 행을 명시한다(기존 채널 목록의 같은 이름 행을 수정한다).

```ini
[/Script/DTCore.DTCoreSettings]
+DataChannels=(ChannelName="sensor",ChannelClass="/Script/DeviceRuntime.DxDeviceSensorStompChannel",bAutoConnect=False,bUseDefaultCredentials=False)
```

장비 전송 방식은 DTCoreWebSocket, Default 주소와 sensor-bulk 주소는 기존 연결 설정을 사용한다. sensor에 별도 자동 연결/구독 작업을 설정하지 않은 이 조합에서는 실제 제품 연결은 **Default + sensor-bulk = 2개**다. 등록된 상태 목록에는 연결을 열지 않은 sensor도 남는다. 독립 sensor 연결이 필요한 수동/기존 STOMP 업무와 혼용하면 2개를 보장하지 않는다. 공통 연결에 binary PCD를 Base64로 넣거나 전용 worker를 없애지 않는다.

설정·모드 변경 후 재시작하고 상태 목록의 Default/sensor-bulk 연결과 sensor 미연결을 확인한다. 인증값은 ini에 직접 넣지 않는다. Docker 기능 검수는 Tools/ArtemisClusterTest의 shared 모드를 사용하며, 외부 소비자/HAProxy health check 연결을 제품의 두 연결과 구분해 보고한다.

### STRUCTURE-12 r58 중지 상태

PCD 연결 중단 중 사용자가 전송을 중지하거나 World 세션이 끝나면 `전송 중지로 관측 종료`로 표시합니다. 이때까지의 기간과 확인된 유실은 고정되며 복구 Warning을 만들지 않습니다. 다음 실패는 새 구간으로 집계하고 누적 유실은 유지합니다. 정상 연결 중 중지는 유실 구간을 만들지 않습니다.
