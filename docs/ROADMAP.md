# Roadmap

Development is gated so Vita-specific work starts only after the service can be
used by an independent desktop reference client.

## Phase 1: PC client feasibility audit

1. [x] Verify and inventory the official installer.
2. [x] Inspect the installed application, native libraries, drivers, codecs,
   and configuration without collecting account data.
3. Observe one user-authorized session and document authentication, catalog,
   allocation, media, input, and shutdown behavior.
4. Make a go/no-go decision.

Proceed only if the authenticated protocol can be implemented without bypassing
DRM, authentication, encryption, attestation, or other access controls.

## Phase 2: Desktop reference client

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
