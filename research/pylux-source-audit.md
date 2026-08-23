# Pylux source audit: direct cloud-client feasibility

Audit date: 2026-08-23

Repositories inspected:

- `ForWard-Technologies-LLC/Pylux`, commit
  `ca87a49a12bab071948f565ca53ffdad790775d3`
- `Chazq2023/CloudPad-Android`, as an Android-focused downstream comparison

No application was executed and no PSN credential or account data was used.

## Decision-changing result

Direct PlayStation cloud support is implemented in published source. It is not
only an Android UI claim and it does not require the official Windows client as
a relay.

Current Pylux places the important cross-platform implementation in
`libchiaki`, including:

- unified PS Now/PS Cloud catalogue retrieval and caching;
- NPSSO-to-OAuth authorization flows;
- Kamaji entitlement resolution for PS3/PS4 sessions;
- Gaikai authorization, datacenter selection, and session allocation;
- a stream-ready allocation result containing the host, port, handshake data,
  launch specification, session identifier, MTUs, and service type;
- Takion protocol versions for PS Now and PS Cloud;
- PS Cloud audio/FEC reassembly;
- video packet assembly and controller feedback/input transport.

This is sufficient evidence to reopen the earlier no-go decision and begin a
native Vita port feasibility track. It does not yet prove that the library can
compile or run within Vita constraints.

## Reusable boundary

The intended port boundary is the C `chiaki-lib`, not the Android APK, Kotlin
UI, JNI bridge, Qt GUI, or Android `AMediaCodec` implementation.

The public `chiaki_cloud_provision_session()` interface is especially useful:
it accepts a selected service/title and authorized NPSSO value, performs the
blocking cloud allocation flow, and returns the inputs required by the existing
native stream session.

## Native dependencies to port or supply

The core currently depends on:

- POSIX-like sockets, threads, timing, DNS, and filesystem functions;
- libcurl with HTTPS support;
- OpenSSL or mbedTLS;
- json-c;
- nanopb;
- Jerasure and GF-Complete for forward-error correction;
- Opus;
- miniupnpc for the broader library (likely removable from a cloud-only Vita
  build);
- a platform video decoder, audio sink, renderer, input provider, and secure
  token store.

The repository already supports a non-Qt library-only configuration and uses
mbedTLS on iOS and Switch. Those are encouraging precedents for a VitaSDK
target.

## Principal Vita risks

1. **Video codec:** the Android cloud path selects H.264 for `psnow` sessions
   and H.265 for `pscloud` sessions. Vita's practical hardware path is H.264.
   PS3/PS4 catalogue streaming is therefore the best initial target. PS5 cloud
   streaming requires proving that Sony accepts an H.264 launch profile or a
   viable Vita HEVC decoder exists; neither is established.
2. **Memory:** catalogue JSON, TLS, FEC buffers, compressed video queues, and
   decoder surfaces must fit within Vita application memory. The full desktop
   or Android UI cannot be carried over.
3. **Platform compatibility:** socket/thread abstractions, curl/TLS, entropy,
   filesystem caching, and CMake require a Vita-specific backend and build
   profile.
4. **Authentication UX:** Sony's current web login is unsuitable for embedding
   directly in a simple Vita application. A safe token-entry/bootstrap design
   is required, and secrets must never be committed or logged.
5. **Service stability:** Pylux is unofficial and uses Sony service behavior
   that may change or be restricted.

## Licensing and credential boundary

Pylux is AGPL-3.0-only with an OpenSSL linking exception. A derivative Vita
client must retain the applicable notices, provide corresponding source, and
use compatible dependencies.

The inspected `secrets/` tree contains credential templates and documentation,
not populated credential files. Future work must preserve this pattern. No PSN
token, cookie, device identifier, session key, or account data may enter source
control or diagnostic logs.

## Recommended first implementation milestone

Create a VitaSDK build target for a deliberately reduced `chiaki-lib` and prove
that a non-networked unit-test executable links and runs. Start with common,
crypto, protobuf, and protocol primitives; keep GUI, Android, FFmpeg, Remote
Play discovery, UPnP, and live account access disabled.

Only after that build is stable should the project add HTTPS/catalogue access,
then authentication bootstrap, allocation, transport, H.264 video, Opus audio,
and controller input in that order.
