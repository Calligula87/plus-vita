# Installed PlayStation Plus PC client: static audit

Audit date: 2026-07-28

Scope: read-only inspection of the installed application before an
authenticated streaming session. No Sony binaries, cookies, credentials,
tokens, account data, or packet captures are stored in this repository.

## Installation inventory

- Install directory: `C:\Program Files (x86)\PlayStationPlus`
- File count: 139
- Installed size: 275,914,059 bytes
- No PlayStation Plus, PS Now, or Gaikai Windows service or kernel driver was
  found.
- All inspected executables are 32-bit x86 and have valid Sony Interactive
  Entertainment Authenticode signatures.

| Component | Purpose inferred from static evidence | Version | SHA-256 |
| --- | --- | --- | --- |
| `pspluslauncher.exe` | Native QAS player, streaming, input, and local IPC | 12.0.0.0 | `A4AA3982166B56296763CE6BCDCC804A7873958D4472E9FA7D65B8DB958CFE27` |
| `agl\agl.exe` | Electron/Chromium AGL storefront and account UI | 0.0.0 | `88BA3322DF5462B4CDBDD953E4465D0ECFB638D01CF8EE498B57446BEDA14300` |
| `QtWebEngineProcess.exe` | Qt WebEngine helper used by QAS | n/a | `82499F7452DA92B59153E8E8559C59EFF654FCAB1E8035CBF12DA43E5CEADE0B` |
| `unidater.exe` | Sony updater | 1.0.0.0 | `FBD3BC2E6CA5956F0C5ADBA2CE54B31239AF1A16B596CBFB42FDB2FEB314259F` |

The native player identifies itself as:

`Gaikai Player v.12.7.0` built on 2026-07-13, commit
`fa8cd3c04303e2718d3023797378767679bd5c54`.

## Architecture

The application is not a single web app and cannot be converted into a Vita
binary:

1. **AGL** is an Electron shell whose package name is still
   `playstation-now`. It loads the remote PlayStation catalog, account, and
   session UI.
2. **QAS** is the signed native Qt application in `pspluslauncher.exe`. It owns
   the streaming engine, video/audio decoding, network transport, encryption,
   and controller handling.
3. AGL and QAS exchange JSON commands over an authenticated local IPC channel.
   The legacy transport is a WebSocket on `127.0.0.1:1235`; the current
   Electron renderer is bridged through Electron IPC. A random `ipc-token`
   authenticates the local connection.

Representative AGL-to-QAS operations include:

- client-ID and game-session requests;
- starting, switching, and stopping a game;
- streaming and queue state;
- connection tests and settings;
- controller connection, disconnection, swap, rumble, and input routing;
- PS Home/XMB commands and analog-stick rate limiting.

This separation is useful: a standalone client would replace both layers, but
the desktop reference client can first reproduce the documented local command
boundary while the native transport is studied independently.

## Service and session endpoints

Static strings identify the legacy Gaikai/PS Now service family:

- `https://psnow.playstation.com`
- `https://psnow.e1-np.playstation.com`
- `https://psnow.mgmt.playstation.com`
- `https://cc.prod.gaikai.com`
- `https://client.cc.prod.gaikai.com/v1/dump`

The updater metadata URL is:

`https://download-psnow.playstation.com/downloads/psnow/pc/meta`

The native client contains request paths for `/v1/config`, `/v1/sessions`,
`/v1/events`, `/v1/logs`, and `/v1/dump`. Static session fields include a cloud
auth code, client IDs, entitlement ID, session ID, session key, encrypted key,
and stream-server endpoint. These names establish the high-level flow but are
not enough to implement authentication safely.

## Media stack

Static and import evidence identifies:

- H.264 video;
- Windows Media Foundation hardware decoding with a D3D11/DXVA path;
- FFmpeg software decoding fallback through `avcodec-56.dll`;
- NV12/YUV rendering and OpenGL/Direct3D presentation;
- Opus audio decoding and encoding;
- OpenAL audio output.

No HEVC requirement was found. H.264 and Opus are both practical on Vita, with
hardware H.264 decode being essential for acceptable battery use and latency.

## Network transport

The streaming protocol is a proprietary Gaikai transport named **Takion**.
Evidence includes:

- protobuf-encoded control and media messages;
- a reliable channel plus UDP media traffic;
- session handshake and protocol-version negotiation;
- RTT and bandwidth tests through a component named Senkusha;
- audio/video NAL units, frame queues, packet-loss reporting, and FEC;
- stream encryption, encrypted session keys, and GMAC integrity checks;
- reconnect, queue, session switch, feedback, microphone, and controller
  message types.

This is not WebRTC and not the Remote Play protocol implemented by Chiaki.
Green Vita's user interface, Vita rendering, input, and packaging patterns may
be reusable, but its Xbox service protocol is not.

## Controller path

The PC client imports Windows HID and SetupAPI functions, dynamically uses
`xinput1_4.dll`, and recognizes at least:

- DualShock 3;
- DualShock 4;
- XInput controllers;
- a Gaikai controller type.

The streaming protocol has controller-connection payloads and rumble events.
Therefore a Vita client should not emulate a physical USB DualShock 4 at the
operating-system level. It should map Vita buttons, sticks, touch surfaces, and
motion data directly into the controller messages expected by Takion. The
missing L2/R2/L3/R3 and touchpad inputs are primarily a UI/mapping problem
after the message format is known.

## Security and feasibility signals

Positive findings:

- no separate Sony kernel driver or mandatory Windows service;
- no static PlayReady, Widevine, TPM, or hardware-attestation dependency found;
- ordinary user-mode networking, media, HID, Qt, OpenSSL, and Windows crypto
  components;
- protocol and protobuf type names remain present in the native binary;
- H.264 and Opus match capabilities already demonstrated by Vita homebrew.

Unresolved blockers:

- obtaining a legitimate cloud auth code and service client identity;
- exact `/v1/sessions` request/response contract;
- Takion handshake, version negotiation, key derivation, and GMAC details;
- encrypted-key acceptance and stream-key lifecycle;
- protobuf schemas and wire-level controller/media formats;
- server-side client-version, integrity, VM, or device-policy checks;
- Sony terms, service changes, and account-ban risk.

No static evidence proves that server-side attestation is absent. The executable
does contain a virtual-machine warning and uses Windows `MachineGuid` as a
device identifier in the AGL layer. Only a carefully redacted, user-authorized
runtime observation can establish what the server actually validates.

## Current feasibility verdict

- **PC relay to Vita:** high feasibility. The official PC client remains the
  authenticated endpoint while a Vita app receives a locally re-encoded or
  forwarded stream and returns controls.
- **Independent desktop reference client:** plausible but unproven.
- **Direct standalone Vita client:** technically plausible, with medium-to-low
  confidence until authentication and the Takion encrypted handshake are
  understood.
- **Direct EXE port/wrapper:** not feasible.

The audit produces a conditional go decision for one controlled dynamic
observation. Vita implementation should not begin until an independent desktop
client can establish a legitimate session.

## Next controlled observation

1. Start the official client normally and let the user authenticate directly.
2. Record only process creation, command-line shape, listening/remote
   endpoints, DNS names, connection protocol/ports, and redacted IPC message
   schemas.
3. Launch one short game session and record state transitions, media transport,
   codec parameters, controller events, and clean shutdown.
4. Never store passwords, cookies, bearer values, auth codes, session IDs,
   session keys, encrypted keys, or full packet payloads.
5. Decide whether a clean-room desktop reference client can be built without
   bypassing access controls.
