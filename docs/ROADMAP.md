# Roadmap

Development is gated so Vita-specific work starts only after the service can be
used by an independent desktop reference client.

## Phase 1: PC client feasibility audit

1. [x] Verify and inventory the official installer.
2. [x] Inspect the installed application, native libraries, drivers, codecs,
   and configuration without collecting account data.
3. [ ] Complete protocol feasibility observation:
   - [x] Observe process lifecycle, endpoint allocation, UDP traffic profile,
     IPC/input command surface, and shutdown behavior.
   - [ ] Establish a documented, legitimate independent authentication,
     catalog, and session-allocation contract.
   - [ ] Establish the Takion handshake, cryptographic key lifecycle, message
     schemas, media framing, and controller wire format without bypassing an
     access control.
4. [x] Make the current go/no-go decision. See
   [Phase 1 decision](PHASE1_DECISION.md): a real independent client and direct
   Vita port are **no-go with the current evidence**. Offline clean-room work
   may continue; connecting it to Sony services may not.

Proceed only if the authenticated protocol can be implemented without bypassing
DRM, authentication, encryption, attestation, or other access controls.

## Phase 2: Desktop reference client

**Blocked for real service connectivity by the Phase 1 decision.** The current
implementation is an offline simulator and architecture test, not a Sony
service client.

Implement the smallest independent client that can authenticate legitimately,
list available titles, allocate a session, receive audio/video, and send
controller input.

## Phase 3: Vita proof of concept

Port the proven protocol client to VitaSDK. Target hardware H.264 decoding,
stereo audio, one controller, touch mappings for missing DS4 inputs, and a
conservative 720p stream profile.

## Phase 4: Productization

Add token lifecycle handling, diagnostics, reconnect support, network recovery,
controller remapping, compatibility testing, packaging, and release
documentation.
