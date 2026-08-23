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

Expand the reduced native target with nanopb and selected Takion message
primitives, still without networking or account data. This will test the first
generated-code dependency and more of the real streaming protocol before Vita
platform adapters are introduced.
