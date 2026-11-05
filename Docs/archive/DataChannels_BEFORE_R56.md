> **이전 문서 원문, 현재 기준 아님.** 현재 문서는 상위 Docs 및 저장소 README를 따릅니다.

# Sensor data channels (DATA-IF-10)

DTCore is required. This integration targets the user-approved DTCore data-channel
branch `dd7bae7be36d9d6137cb41f8fd1ec86c60306b71`; host merge waits for its main merge.

`UDxDeviceSensorChannelSubsystem` registers `sensor` and `sensor-bulk` for the GameInstance.
Default registration starts no connection, worker, subscription, or extra tick.
The existing World subsystem remains the socket worker owner and public compatibility
facade. Travel/World teardown stops and detaches that worker; channel shutdown does
not retain a dead World.

The worker loop, TCP/STOMP parser, shared immutable bodies, queue reservations,
receipt handling, and diagnostic receiver selection are unchanged. The channel's
`PublishFrame` accepts the existing immutable frame without copying its payload.
Generic `PublishBinary` rejects a body without acquisition metadata/reservation
ownership; use the typed frame API or the existing World facade. Generic Subscribe
does not silently enable self-consumption: retain explicit `ConfigureReceiver`.

Named `DTCoreSettings.DataChannels` values take precedence over the legacy profile
at connection preparation. Without a named entry the component's existing
`SensorTransport.ini` / map/UI profile and session credentials pass through unchanged.
Positive channel Reconnect* fields override the immutable raw profile; empty/zero
fields preserve Device reconnect defaults (1s / 10s / multiplier 2).

The reserved name must use `UDxDeviceSensorBulkChannel`. An auto-created generic
`UDxStompChannel` for an empty ChannelClass is replaced during Device registration;
an incompatible custom class is rejected rather than silently repurposed. Set the
correct ChannelClass explicitly when enabling named auto-connect settings.

Status counts mean: Published = worker submission (not broker acceptance), Dropped =
rejected frame or terminal delivery failure, Backpressure = original overload or
send-delayed events. Broker receipt and consumer validation remain separate in the
existing telemetry. No body/header/destination conversion is performed.

`sensor` is `UDxDeviceSensorStompChannel : UDxStompChannel`. The existing transport
component retains its Blueprint API, result structure, messages, HTTP/file modes,
and topic selection; it delegates JSON/manual binary connection and sending to this
channel. DTCore owns connection generations, reconnect timers, and subscriptions.
The ACK subscription is renewed per connection and stale receipts cannot mutate
the current result. Session credentials are forwarded in memory through a thin
IStompClient adapter; neither DTCore global settings nor files receive a copy.
Multiple matching owners share the channel. A conflicting legacy profile is
rejected without replacing another owner's connection. The final legacy owner
disconnects at teardown; explicit named auto-connect settings retain GI ownership.

For a named entry, URL/credential resolution follows DTCore's documented override
rules (a blank named URL inherits the default DTCore URL). Without an entry,
generic Connect reads the same legacy INI parser as the component, preserving
first-file precedence and URLs containing `//`. Existing component map/UI overrides
are passed at connection preparation. Channel Reconnect* zero values are the
special bulk case described above, retaining Device defaults rather than the
default WebSocket channel's maximum delay.

The stage-9 opt-in low-frequency JSON mode sends through `FindChannel("Default")`;
it never redirects 20Hz binary PCD to the shared connection. Existing connection
status reads `GetAllChannelStatus`; it does not open a connection to query status.

Default PCD destinations agree with `UDxDataChannelLibrary::MakeDestination`.
Custom legacy destinations are retained verbatim: the new helper's restricted
alphabet must not reject existing server configuration. `MakeStandardHeaders` is
not applied because adding/replacing dx-* headers would change the existing wire
contract. The existing EnginePcd/RawPcd header builders and 33-byte records remain.

Validation distinguishes transport submission, broker receipt, and consumer ACK.
The mock lifecycle regression covers reconnect timer execution, ACK re-subscription,
receipt deadline, late receipt/generation rejection, and final-owner cleanup.
It does not claim live-broker or GPU acquisition acceptance.

## Project topic policy (r55b)

`UDeviceDataChannelSettings` is a Game-config DeveloperSettings class. Its `Channels`
array contains `ChannelName` and `Destinations`: `Alias`, `Destination`, `bSubscribe`,
and `Purpose`. An absent/empty alias destination preserves the legacy INI/map/UI
profile. The default list is empty, preserving all existing server names and the
default disabled self-consumption policy. `camera`, `lidar`, `pointcloud`, `imu`,
and `ack` are the code-facing aliases; arbitrary extra receive aliases are allowed.
Connection settings remain exclusively in DTCore's existing settings.
The reserved `sensor/ack` interest shares the existing ACK callback/key, honors an
explicit disabled subscription, and is retained across reconnect and legacy-owner
teardown. Its callback remains `UDxDeviceSensorStompChannel::AckReceived`; it is not
duplicated through the generic topic callback.

The GI registry registers multiple sensor interests via `UDxStompChannel::Subscribe`;
DTCore retains keys and renews them on reconnect. The Default adapter uses the
existing shared connection and reissues interests for each new connection generation.
Registration never opens a connection. `OnTopicMessage` exposes channel and alias to
the project without adding a second TC dispatcher. Same-destination aliases in one
channel share one interest (the first subscribed alias identifies the callback).
Cross-channel overlaps log a warning and appear in the monitor/status getter without
blocking connections. The registry owns bindings; they weakly reference their owner
and channel and are removed at GI shutdown.

Bulk configured interests use the existing worker socket. They are unioned with
explicit diagnostic receiver interests and renewed after reconnect. They do not
automatically enable decoding, PCD validation or a second large-body GT delivery
path: the existing receiver selection still controls those operations. Default
project interests are empty, and SEND/receipt/parser/reservation algorithms are
unchanged. Alias selection occurs at the existing destination-selection boundary.

Common configuration is `Config/DefaultGame.ini`; platform overrides are
`Config/Windows/WindowsGame.ini` or `Config/Linux/LinuxGame.ini`. A deployed
`DefaultGame.ini` that lists an array replaces the packaged list. In a platform file, a `+` row
edits the channel of the same name or adds a new one; `!Channels=ClearArray` plus complete rows
replaces the list. Restart after changing deployed
loose configuration. Credentials remain Runtime Override key references, not values
inside topic policy. The host's `docs/ARTEMIS_PCD_SETTINGS.md` has concrete examples.

Cooked packages apply those two physical loose files at module startup because pak
config otherwise masks this deployment layout. Only topic policy and an explicit
connection-property whitelist are loaded, common then platform, above the existing
config values. The Runtime Override credential/URL rules still apply afterwards.
The overlay parses a separately owned config file and never copies or changes the
global config cache; unrelated fields,
direct credentials and map configuration are excluded. Each file is capped at 2MiB.
Editor configuration continues to use UE's standard hierarchy.

Development-only `-DeviceChannelConfigProbe` writes a read-only redacted snapshot
after engine initialization and exits. No work is registered without that flag;
the probe body is compiled out of Shipping. Package deployment acceptance uses
NullRHI and is distinct from GPU/performance or direct UI acceptance.
