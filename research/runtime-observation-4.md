# Runtime observation 4: TCP/IP endpoint events

Observation date: 2026-08-22

Scope: a user-authorized game launch that displayed an account/ID warning and
stalled after the game logo, followed by one retry. The application was fully
closed after each attempt.

The observation used a dedicated Microsoft-Windows-TCPIP ETW session. Packet
capture was disabled and the provider's packet-event keyword was excluded. The
private ETL and decoded CSV remain in the ignored private-capture directory.

No packet bytes, application payload, credentials, cookies, tokens, session
identifiers, keys, or authenticated messages were collected.

## Validation

Before the game test, a DNS lookup confirmed that the observer reports local
and remote UDP endpoint tuples and process context. The validation recorded the
expected local-host-to-router UDP port 53 flow without recording DNS packet
contents.

## Game-session endpoint selection

Two separate process IDs created nearly identical UDP endpoint-selection
sequences. Their timing matches the user-reported failed launch and retry.

### Attempt 1

Candidate native-player PID: `5972`

The process briefly opened UDP endpoints to several addresses on remote port
`2053`:

- `104.142.181.9`
- `104.142.189.132`
- `104.142.177.65`
- `104.142.185.3`
- `104.142.191.144`

It then selected `104.142.181.19:2053`. The corresponding local endpoint was
`192.168.1.4:60345`, observed from `15:09:02` to `15:10:28` local time—about 86
seconds.

### Attempt 2

Candidate native-player PID: `16092`

The process briefly opened UDP endpoints to:

- `104.142.181.9`
- `104.142.189.2`
- `104.142.177.133`
- `104.142.185.27`
- `104.142.191.21`

It then selected `104.142.181.7:2053`. The corresponding local endpoint was
`192.168.1.4:53537`, observed from `15:13:14` to `15:16:55` local time—about
three minutes and 41 seconds.

## Interpretation

The repeated pattern strongly identifies UDP port `2053` and the observed
`104.142.*` address pool as game-session transport candidates. Each attempt
probed multiple endpoints from several short-lived local sockets before opening
one longer-lived endpoint. This resembles stream-server selection or connection
quality probing followed by session transport establishment.

Endpoint events alone do not prove the proprietary message role or whether the
long-lived flow carried video, audio, control, or a multiplex of those channels.
The ETW provider associates these events with the initiating process IDs, but
the trace does not embed executable names; native-player attribution is based
on the distinctive port-2053 selection behavior and its direct timing match to
the two user-observed game attempts.

The account/ID warning cannot be analyzed from endpoint metadata and no account
value was observed.

## Current conclusion

The previously unknown UDP remote tuple is now established with high
confidence at the service-pool level:

- transport: UDP;
- remote port: `2053`;
- observed pool: multiple `104.142.*` addresses;
- behavior: parallel/sequential short probes followed by one longer-lived
  selected endpoint.

The next step should correlate process lifecycle and ETW endpoints in one tool,
then use counters—not packets—to measure bytes and datagrams for only the
selected local/remote tuple. Protocol payload analysis is neither required nor
warranted at this stage.
