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
Vita clock/entropy adapters, json-c, and the Pylux catalogue modules. On launch
it runs deterministic tests—including a Takion protocol-negotiation round trip
and synchronized worker thread—and returns nonzero if any test fails. The
optional public Apollo test performs credential-free, read-only Sony catalogue
GETs; the application accepts no account data.

This deliberately small link proves the compiler, Pylux source boundary, ARM
ABI, Vita SELF generation, and VPK packaging before larger dependencies are
introduced.

## Build

With VitaSDK installed and `VITASDK` set, install the maintained transport
packages once:

```sh
vdpm zlib zstd mbedtls curl-mbedtls
```

Then build:

```sh
cmake -S vita-client -B build/vita -DCMAKE_BUILD_TYPE=Release
cmake --build build/vita --parallel
```

The output is `build/vita/plus_vita_smoke.vpk`. Enable the explicitly
authorized public CH/EU catalogue hardware test with
`-DPLUS_VITA_PUBLIC_APOLLO_TEST=ON`; it is off by default.

## Next increments

1. Add cloud authentication without logging or persisting credentials.
2. Add authenticated catalogue and entitlement resolution.
3. Add cloud session provisioning and datacenter selection.
4. Add H.264 hardware decode, Opus audio, and Vita controls.

