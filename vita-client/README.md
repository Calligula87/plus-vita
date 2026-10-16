# Vita client

This directory contains the native VitaSDK client. Its current accepted build
streams cloud games on the Vita with native H.264 decode, audio and controls.
The combined input-worker/deferred-log VPK is the maintainer's playable
pre-release baseline. See the [milestone](../docs/PLAYABLE_MILESTONE.md) for the
exact accepted package, build configuration and remaining limitations.

The original smoke application established portable `libchiaki` primitives and
VPK packaging. The smoke-target description below records that early stage;
it does not describe the full current streaming application.

The Pylux source is pinned as the `external/pylux` Git submodule. Initialize it
after cloning this repository:

```sh
git submodule update --init --recursive
```

## Current smoke target

New builds open the graphical app with session-only account import/manual
entry and an initial catalogue. Live account setup still needs its
[Vita check](../docs/ACCOUNT_SETUP_TEST.md); see the
[setup guide](../docs/ACCOUNT_SETUP.md) and
[startup check](../docs/GRAPHICAL_STARTUP_TEST.md). Stream integration follows
in later release tickets. Select
`-DPLUS_VITA_DIAGNOSTIC_APP=ON` to build the historical smoke/stream interface
described below. Retain accepted stream artifacts separately.

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

## Original increments (historical)

1. Add cloud authentication without logging or persisting credentials.
2. Add authenticated catalogue and entitlement resolution.
3. Add cloud session provisioning and datacenter selection.
4. Add H.264 hardware decode, Opus audio, and Vita controls.

These capabilities are now present in the tested client. The current next phase
is release preparation using the accepted playable package; see the
[roadmap status](../docs/ROADMAP.md) and
[completed latency phase](../docs/LATENCY_COMBINED_RESULT.md).

