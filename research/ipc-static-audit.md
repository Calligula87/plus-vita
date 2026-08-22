# Static AGL-to-QAS IPC schema audit

Audit date: 2026-08-22

Scope: identifier-only analysis of the installed `preload.js`,
`preloadNotifier.js`, and `websocket_client.js` bridge files. The reproducible
extractor records source hashes, literal command names, event names, and
parameter-field names. It does not run or modify the official client, connect
to localhost IPC, read runtime values, or copy Sony source code.

## Transport boundary

The bridge supports a legacy WebSocket connection to localhost and the current
Electron IPC path. The WebSocket transport authenticates with an IPC token
before ordinary commands. Because a runtime interceptor would necessarily see
that token and potentially other authenticated values, live payload interception
is not warranted at this stage.

The static extractor flags sensitive identifier names but never reads values.
Its committed output contains no token, credential, account, session, or game
identifier.

## Extracted surface

The current installed sources expose:

- 52 literal command names;
- 12 literal event/listener names;
- 29 literal parameter-field names.

The commands cover these major areas:

- session lifecycle: client-ID request, game request, game start/switch/stop,
  availability, queue, streaming, and shutdown state;
- controller behavior: routing, connection reporting, disconnect, swap,
  rumble, XMB commands, and analog-stick rate limiting;
- client configuration: settings, privacy, version, microphone, and volume;
- window and shell behavior: focus, position, URLs, notifications, tray,
  splash screen, and deep links.

The complete sorted identifier inventory is stored in
`ipc-static-schema.json`. That JSON is generated from the locally installed
files and includes hashes so future PlayStation Plus updates can be compared
without committing vendor files.

## Security-relevant result

The transport sources contain the sensitive identifier names `authToken` and
`token`. Their presence confirms that even localhost IPC capture would handle
authentication material. A schema-only static approach is therefore the
appropriate boundary until there is a specific unanswered feasibility question
that cannot be resolved without runtime messages.

## Current conclusion

The IPC command surface is sufficiently visible to design a clean-room desktop
reference-client interface without collecting live authenticated messages. The
next useful step is to turn this inventory into typed, value-free command
schemas and state-machine documentation, beginning with the non-sensitive
session lifecycle: `requestClientId`, `requestGame`, `startGame`, streaming and
queue events, `stop`, and shutdown.
