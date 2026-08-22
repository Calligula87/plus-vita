# Runtime observation 1: process and endpoint metadata

Observation date: 2026-08-22

Scope: one user-authorized launch of the official PlayStation Plus PC client,
including an initially stalled game preparation, a retry, a successful short
game test, and game shutdown. The application itself remained open afterward.

Only process lifecycle and network endpoint metadata were observed. No packet
payloads, command lines, credentials, cookies, tokens, session identifiers,
keys, or authenticated messages were collected.

## Process lifecycle

Client startup created two `pspluslauncher` processes, five persistent `agl`
processes, and three persistent `QtWebEngineProcess` processes. Several other
short-lived AGL processes started and stopped during initialization.

After the game session was closed, all ten persistent client processes remained
running and responsive. This confirms that ending a game does not shut down the
catalog UI or native player process group.

## Local IPC

The native player listened on TCP `127.0.0.1:1235`. Both AGL and native-player
processes maintained loopback connections to that port. This runtime evidence
confirms the local IPC boundary identified during static analysis.

No IPC payloads or authentication values were captured.

## Remote connection behavior

The observed AGL and native-player processes used outbound TCP port 443. During
the stalled preparation and retry, `pspluslauncher` repeatedly opened and
closed connections, including short `SYN_SENT` intervals, rather than becoming
an unresponsive process. This is consistent with an active retry loop, although
endpoint metadata alone cannot identify whether the cause was allocation,
service availability, network reachability, or another session-state failure.

The observed remote IPv4 addresses were:

- `104.142.251.227`
- `104.77.40.46`
- `2.21.22.18`
- `2.21.22.43`
- `23.10.249.107`
- `23.10.249.120`

No hostname attribution is made from IP addresses alone because CDN routing can
change and reverse DNS is not sufficient proof of application ownership.

## Tooling limitation and correction

Windows denied access to `Get-NetTCPConnection` and `Get-NetUDPEndpoint` in the
non-elevated observation session. The observer switched to a metadata-only
`netstat` fallback. The initial fallback captured TCP but not UDP, so this run
does not establish whether the game stream used UDP. UDP fallback collection
was added after the run for the next controlled observation.

## Current conclusion

The observation confirms the expected AGL-to-QAS local TCP boundary, shows that
the native player actively retries remote HTTPS connections during a stalled
launch, and documents the resident process set after game shutdown. It does not
yet reveal session contracts, media transport, codec parameters, controller
messages, or encryption behavior.

The next run should use the corrected observer to determine UDP endpoint
lifecycle across a successful session and should record user-noted timestamps
for launch requested, first rendered frame, game exit, and application exit.
