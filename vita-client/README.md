# Vita client

This directory contains the native VitaSDK client. The first milestone is a
credential-free smoke application that cross-compiles selected portable
`libchiaki` primitives and packages them as a real VPK.

The Pylux source is pinned as the `external/pylux` Git submodule. Initialize it
after cloning this repository:

```sh
git submodule update --init --recursive
```

## Current smoke target

The target compiles Pylux's C base64 and logging implementations, threading,
protocol sequence/service primitives, nanopb runtime, generated Takion schema,
and Vita clock/entropy adapters. On launch it runs deterministic tests—including
a Takion protocol-negotiation round trip and synchronized worker thread—and
returns nonzero if any test fails. It performs no network access and accepts no
account data.

This deliberately small link proves the compiler, Pylux source boundary, ARM
ABI, Vita SELF generation, and VPK packaging before larger dependencies are
introduced.

## Build

With VitaSDK installed and `VITASDK` set:

```sh
cmake -S vita-client -B build/vita -DCMAKE_BUILD_TYPE=Release
cmake --build build/vita --parallel
```

The output is `build/vita/plus_vita_smoke.vpk`. GitHub Actions performs the
same build in the official `vitasdk/vitasdk` container.

## Next increments

1. Add Vita socket initialization, DNS, and loopback-only transport tests.
2. Replace the blocking timed-join compatibility path with a real Vita timeout.
3. Add mbedTLS, curl, and json-c for HTTPS and catalogue access.
4. Add cloud provisioning without logging or persisting credentials.
5. Add H.264 hardware decode, Opus audio, and Vita controls.

