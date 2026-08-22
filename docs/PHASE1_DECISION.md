# Phase 1 feasibility decision

Decision date: 2026-08-22

## Verdict

The project is **no-go, at the current evidence boundary, for an independent
client that connects directly to PlayStation Plus and for a standalone PS Vita
port**.

The project is **go for offline clean-room engineering only**: typed interfaces,
state machines, validation, recovery logic, redacted diagnostics, synthetic
tests, and platform experiments that do not connect to Sony services.

This is a technical and security-boundary decision, not a claim that a future
legitimate implementation is impossible. The decision must be revisited only
if authorized public documentation, an official SDK/API, or other lawful
evidence resolves the blockers below without bypassing access controls.

## What the audit established

- The official PC application separates its Electron UI from a native QAS
  player and exposes a local authenticated IPC boundary.
- Static analysis identified the value-free command and event surface needed
  to model session lifecycle and controller behavior.
- Controlled runtime observations attributed a sustained, download-heavy UDP
  flow on remote port 2053 to `pspluslauncher` during a game session.
- Installed components identify the native streaming protocol as Takion and
  indicate H.264 video and Opus audio support.
- Process startup, endpoint probing/selection, active streaming, game exit, and
  application shutdown were observed without capturing packet payloads,
  credentials, tokens, session identifiers, or keys.
- The offline reference skeleton validates architecture, state transitions,
  recovery, and diagnostics, but deliberately has no Sony connectivity.

## Unresolved blockers

The audit did not establish enough information to implement a legitimate real
client. In particular, it did not establish:

- an independently usable authentication and token-lifecycle contract;
- the catalog and session-allocation API contract;
- the exact Takion negotiation and protocol version behavior;
- encryption, GMAC, key derivation, rotation, or key-delivery semantics;
- protobuf or other reliable-channel message schemas;
- controller-input and media framing on the wire;
- server-side integrity, device, entitlement, or attestation requirements.

Runtime IPC interception and payload decryption are not acceptable shortcuts:
the inspected bridge authenticates with token-bearing fields, and obtaining or
reusing protected runtime values would cross the project's stated boundary.

## Architecture consequences

| Track | Decision | Reason |
| --- | --- | --- |
| Wrap or recompile the Windows executable for Vita | No-go | The Vita cannot run the Windows/Electron/native application stack, and this would not remove its service dependencies. |
| Independent desktop client connecting to Sony | No-go for now | Authentication, allocation, cryptography, and wire contracts remain unresolved. |
| Standalone Vita client connecting to Sony | No-go for now | It inherits every desktop blocker plus Vita platform constraints. |
| Offline reference client and Vita technology tests | Go | These can validate our own architecture without accessing the service. |
| PC-to-Vita relay using the official PC client | Separate conditional track | Technically more feasible because the official client remains the authenticated endpoint, but it changes the product architecture and requires a deliberate project decision. |

## Re-entry criteria

Real Phase 2 connectivity may resume only when all of the following can be met:

1. Authentication and entitlement use an authorized, documented mechanism.
2. Session allocation and protocol contracts can be implemented without
   extracting secrets, defeating encryption, impersonating a protected client,
   or bypassing attestation or DRM.
3. Test accounts and service use comply with the applicable service terms.
4. The implementation can be tested without committing or logging account
   data, tokens, keys, session identifiers, or packet payloads.

Until then, the next productive choice is either to continue platform-neutral
offline work or explicitly evaluate the separate PC-relay architecture.
