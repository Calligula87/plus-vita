# Runtime observation 2: clean game lifecycle

Observation date: 2026-08-22

Scope: a user-authorized clean run consisting of full application shutdown,
fresh application startup, one successful game session, game exit, and full
application exit.

Only process lifecycle and network endpoint metadata were observed. No packet
payloads, command lines, credentials, cookies, tokens, session identifiers,
keys, or authenticated messages were collected.

## Timeline

All timestamps below are UTC.

- `12:25:11` to `12:25:15`: the previously resident AGL, Qt WebEngine, and
  native-player process group shut down.
- `12:25:36` to `12:25:42`: a fresh client process group started.
- `12:26:27` to `12:26:43`: the native player created and retired several
  short-lived UDP endpoints while preparing the game.
- `12:26:43`: the native player opened UDP local port `61767`.
- `12:34:21`: UDP local port `61767` closed after remaining open for about
  seven minutes and 38 seconds.
- `12:34:51` to `12:34:57`: the fresh AGL, Qt WebEngine, and native-player
  process group shut down completely.

The observation contains 24 process-started and 24 process-stopped events. No
matching PlayStation Plus process remained after the user fully exited the
application.

## UDP lifecycle

`pspluslauncher` opened six UDP sockets bound to all local interfaces:

| Local port | Observed lifetime | Notes |
| --- | --- | --- |
| `61759` | about 3 seconds | preparation |
| `61761` | about 1 second | preparation |
| `61762` | about 1 second | preparation |
| `61763` | about 1 second | preparation |
| `61765` | about 6 seconds | preparation |
| `61767` | about 7 minutes 38 seconds | coincides with the successful game interval |

The long-lived `61767` socket is a strong candidate for the game-session media
transport, but endpoint timing alone does not prove its message purpose. The
non-elevated Windows `netstat` fallback exposes UDP local bindings but not their
remote peers, so no UDP server address is attributed in this observation.

## TCP and local IPC

The native player again listened on TCP `127.0.0.1:1235`, confirming that the
AGL-to-QAS IPC boundary is stable across clean restarts.

AGL and `pspluslauncher` used outbound TCP port 443. As in the first
observation, the remote addresses appear to be CDN-routed and are not assigned
hostnames or protocol roles from IP metadata alone.

## Current conclusion

This run provides runtime evidence that the native player creates one
long-lived UDP endpoint coincident with a successful game session, alongside
HTTPS control traffic and the established local TCP IPC boundary. It also
distinguishes closing a game, which can leave the client resident, from fully
exiting the application, which terminates the complete process group.

The next feasibility step is to identify the UDP remote endpoint and basic
traffic characteristics without retaining payloads. Useful metadata includes
packet direction, sizes, timing, rate, and volume. Any tooling for that step
must continue to exclude packet contents and authentication material.
