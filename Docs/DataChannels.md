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
