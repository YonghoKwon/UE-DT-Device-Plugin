# Sensor data channels (DATA-IF-10)

DTCore is required. This integration targets the user-approved DTCore data-channel
branch `dd7bae7be36d9d6137cb41f8fd1ec86c60306b71`; host merge waits for its main merge.

`UDxDeviceSensorChannelSubsystem` registers `sensor-bulk` for the GameInstance.
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
