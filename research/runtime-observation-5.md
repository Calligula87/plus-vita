# Runtime observation 5: correlated process and UDP counters

Observation date: 2026-08-22

Scope: one user-authorized successful game session from clean application
startup through game exit and full application shutdown.

The combined observer synchronized PlayStation Plus process lifecycle,
packet-free Microsoft-Windows-TCPIP endpoint events, and Packet Monitor
counters filtered to IPv4 UDP port `2053`. Packet Monitor ran in counters-only
mode. No packet events or bytes were logged.

No packet payload, credentials, cookies, tokens, session identifiers, keys, or
authenticated messages were collected.

## Process attribution

The clean run started two `pspluslauncher` processes. PID `3144` owned every
observed UDP port-2053 endpoint. This directly attributes the endpoint-selection
and sustained UDP transport behavior to the native player rather than relying
only on timing inference.

All 13 observed PlayStation Plus processes later stopped cleanly.

## Endpoint selection

PID `3144` briefly opened endpoints to:

- `104.142.181.12:2053`
- `104.142.185.130:2053`
- `104.142.189.8:2053`
- `104.142.177.65:2053`
- `104.142.191.21:2053`
- `104.142.181.9:2053`

It then selected `104.142.181.24:2053` from local endpoint
`192.168.1.4:50794`. The selected endpoint was observed from `18:04:11` to
`18:09:06` local time, approximately four minutes and 55 seconds.

## Filtered UDP counters

Over the counter interval, the active Wi-Fi interface recorded the following
cumulative changes for IPv4 UDP port `2053`:

| Direction | Packets | Bytes | Average frame size | Average rate |
| --- | ---: | ---: | ---: | ---: |
| Received | 823,826 | 684,802,273 | 831.2 bytes | 16.83 Mbit/s |
| Transmitted | 19,236 | 2,523,063 | 131.2 bytes | 62.02 kbit/s |

The interval lasted approximately 325 seconds and included endpoint probing as
well as the selected session. Receive rate peaked near 24.42 Mbit/s over one
five-second sample. Counters stopped increasing when the selected endpoint
closed.

The counters are link-interface totals after an IPv4/UDP/port-2053 filter. In
this controlled run, ETW associated all observed port-2053 application
endpoints with native-player PID `3144`, providing strong correlation without
packet inspection.

## Interpretation

The approximately 272:1 receive-to-transmit byte ratio and 42.8:1 packet-count
ratio are consistent with a high-bitrate downstream media stream and compact
upstream input/control traffic. Average frame sizes also show the expected
asymmetry: large downstream datagrams and small upstream datagrams.

These measurements do not reveal codec framing, message formats, encryption,
or channel multiplexing and should not be used to infer payload semantics
beyond the observed direction and volume.

## Current conclusion

The dynamic audit has now established with high confidence that:

- `pspluslauncher` owns the game-session UDP transport;
- server selection probes a pool of `104.142.*` addresses;
- the selected service uses remote UDP port `2053`;
- one long-lived UDP flow carries a download-heavy, latency-sensitive traffic
  profile coincident with the game session;
- the complete observation can be reproduced without packet capture.

The next feasibility work should move up to the documented localhost AGL-to-QAS
boundary. A schema-only IPC observer can identify command names, state
transitions, and field names while irreversibly redacting all values. That is a
safer and more useful next step than expanding network capture.
