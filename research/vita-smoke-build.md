# Native Vita smoke-build result

Build date: 2026-08-23

## Result

The first Pylux-derived native Vita target cross-compiled and packaged
successfully with the official VitaSDK Windows autobuild dated 2026-05-24.

The build completed every expected stage:

1. compile `vita-client/src/main.c` for `arm-vita-eabi`;
2. compile Pylux `lib/src/base64.c` directly from the pinned submodule;
3. link the Vita executable;
4. convert it to Sony ELF;
5. create the Vita SELF;
6. generate `param.sfo` for title ID `PLVT00001`;
7. package `plus_vita_smoke.vpk`.

The resulting VPK contains exactly the required initial package entries:

- `eboot.bin`
- `sce_sys/param.sfo`

The VPK is 29,424 bytes. Build products remain ignored and are not committed.

## Test coverage

The embedded smoke executable tests:

- Pylux base64 encode/decode and round-trip behavior;
- 16-bit and 32-bit protocol sequence-number wrapping;
- PS Now, PS Cloud, and Remote Play service-type classification.

It performs no network operation and handles no credentials. The cross-build
proves the compiler, ARM ABI, pinned Pylux source boundary, Vita SELF tooling,
and VPK packaging. Execution of the embedded assertions on physical hardware
remains a separate test.

## Next milestone

## Takion/nanopb expansion

On 2026-08-23 the target was expanded with Pylux's pinned nanopb runtime and C
code generated from Pylux's `takion.proto` schema. The Vita cross-build again
completed through VPK packaging with warnings treated as errors.

The same source set was also compiled and executed natively under Ubuntu 26.04.
All 15 assertions passed, including encoding and decoding a
`TAKIONPROTOCOLREQUESTACK` message and preserving protocol version `12`.

Generated `takion.pb.c/.h` files are committed so Vita builds do not require
Python or protoc. They must be regenerated whenever the pinned Pylux schema or
nanopb revision changes.

## Next milestone

Add the first Vita platform-adapter layer for logging, monotonic time, random
entropy, and thread synchronization. Keep sockets, HTTPS, account access, and
credentials disabled until those primitives build and have deterministic
tests.
