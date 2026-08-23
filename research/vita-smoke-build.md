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

The current hardware-test VPK is 63,491 bytes. Build products remain ignored and
are not committed.

## Test coverage

The embedded smoke executable tests:

- Pylux base64 encode/decode and round-trip behavior;
- 16-bit and 32-bit protocol sequence-number wrapping;
- PS Now, PS Cloud, and Remote Play service-type classification.

It performs no network operation and handles no credentials. The cross-build
proves the compiler, ARM ABI, pinned Pylux source boundary, Vita SELF tooling,
and VPK packaging. Execution of the embedded assertions on physical hardware
remains a separate test.

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

## Vita platform primitives

On 2026-08-23 the target added Pylux logging and threading plus Vita-specific
monotonic-clock and cryptographic-entropy adapters. The entropy adapter honors
Vita's 64-byte `sceKernelGetRandomNumber()` call limit by filling larger
requests in chunks.

The expanded ARM target again built through VPK packaging. The matching Ubuntu
executable passed all 28 assertions, covering log filtering/formatting,
monotonic time, an 80-byte secure entropy request, mutex contention, and an
actual Chiaki thread create/join cycle with protected shared state.

Vita's pthread implementation lacks GNU `pthread_clockjoin_np`. The current
compatibility shim preserves correct joining but cannot enforce the requested
timeout. Before production recovery logic relies on timed joins, the thread
backend must migrate that operation to a native SceKernel thread wait with a
real timeout.

## Network primitives

On 2026-08-23 the Vita target added explicit `SCE_SYSMODULE_NET` ownership,
one MiB of aligned `sceNetInit()` memory, deterministic shutdown, and Pylux's
socket nonblocking helper. The Vita ARM target links `SceNet` and
`SceSysmodule` and packages successfully.

The matching Ubuntu executable passed all 32 assertions. Its network probe
uses `getaddrinfo()` with `AI_NUMERICHOST` for `127.0.0.1`, creates a TCP
listener and client, toggles nonblocking mode, and verifies a byte-for-byte
loopback exchange. This exercises the resolver ABI and socket operations while
guaranteeing that no DNS query or external connection is made. Physical Vita
execution remains required to verify runtime behavior on the device.

## Native secure HTTP lifecycle

On 2026-08-23 the target added a Vita adapter around Sony's native HTTPS and
SSL facilities. It loads `SCE_SYSMODULE_HTTPS`, initializes separate one MiB
HTTP and SSL pools, tracks whether it owns the underlying network lifecycle,
and tears each layer down in reverse order. The ARM target links
`SceHttp_stub` and `SceSsl_stub` and packages successfully.

The host lifecycle test verifies both ownership cases: HTTPS releases a network
instance it created itself, but preserves a network instance supplied by its
caller. No request, TLS handshake, certificate, hostname, credential, or
external endpoint is used by this milestone. Actual SceHttp/SceSsl runtime
initialization still needs confirmation on physical Vita hardware.

## Next milestone

Install and run the VPK on a physical Vita to confirm the 40 embedded
assertions, especially native network and HTTPS/SSL initialization. After that,
add a certificate-verification test with a controlled local endpoint before
any PlayStation service request or account credential is enabled.

The hardware build now renders its assertions through VitaSDK's debug-screen
facility and waits for the Cross button after the final result, allowing the
tester to photograph failures. Installation and reporting steps are documented
in `docs/HARDWARE_TEST.md`.

## Physical hardware results

On 2026-08-23 all 40 baseline assertions passed on physical Vita hardware.
Native `sceNetInit`, numeric resolution, TCP loopback, nonblocking sockets,
`sceHttpInit`, and `sceSslInit` all executed successfully on the device.

A second controlled LAN test connected only to a temporary TLS 1.2 server at
`192.168.1.4:8443`, presenting an ephemeral self-signed certificate for
`plus-vita-local.invalid`. The Vita returned HTTP error `0x80431075`, HTTPS
certificate error `0x80435060`, and verification detail `0x24`. Detail bit
`0x20` confirms unknown-CA rejection and bit `0x04` confirms hostname mismatch.
The embedded rejection assertion passed and the final result was zero failures.
No DNS query, Sony endpoint, account credential, token, or Internet service was
used. The temporary server was stopped after the test.

The follow-up custom-CA experiment showed that `sceHttpsLoadCert()` could make
the controlled certificate pass verification, but SceHttp returned
`0x80431060` before transmitting HTTP request bytes. This optional path remains
diagnostic only and is not the production trust strategy.

The first public hostname request initially failed before TLS with
`0x80412101`. Comparing the adapter with VitaSDK's official HTTPS sample exposed
the missing `sceNetCtlInit()` lifecycle. After adding `SceNetCtl_stub`, NetCtl
initialization, and reverse-order shutdown, a credential-free request to
`https://example.com/` passed on physical hardware: `sceHttpSendRequest()`
returned zero, certificate detail was zero, HTTP status was 200, and the first
256 response bytes were read successfully. The complete embedded suite ended
with zero failures.

This proves native DNS, Internet routing, the Vita system CA store, TLS, HTTP
status parsing, and response-body reads. The request used no Sony endpoint,
account, token, cookie, or submitted data and was explicitly authorized by the
tester.

## Pylux-compatible SceHttp backend

On 2026-08-23 the target added a Vita implementation of Pylux's
`cc_http_perform()` boundary without linking libcurl. It supports the GET and
POST methods currently used by the cloud modules, request headers and bodies,
redirect policy, timeout conversion, status codes, raw response headers,
`Location` extraction, NUL-terminated response bodies, and deterministic
cleanup. Response bodies are capped at four MiB and headers at 64 KiB so a
remote response cannot exhaust Vita memory without bound. SceHttp errors map to
Chiaki transport or timeout errors.

The Vita target builds and packages with warnings treated as errors. The
matching Ubuntu suite passes all 46 offline assertions, including Pylux-format
bearer/cookie headers, response ownership cleanup, and invalid request
rejection.

The exact compatibility boundary was then tested on physical Vita hardware
using the tester-authorized, credential-free `https://example.com/` GET. The
call to `cc_http_perform()` returned success with HTTP status 200, a 559-byte
NUL-terminated body, and 299 bytes of captured response headers. Transport,
status, body, header capture, and cleanup assertions all passed; the complete
suite ended with zero failures. This confirms that the API consumed by Pylux
works through the native SceHttp implementation on the device, rather than
only through the earlier low-level probe.

No Sony endpoint, account credential, token, cookie, or submitted data was
used by this test.

A following controlled LAN test exercised the same compatibility boundary's
POST path against a one-request echo server at `192.168.1.4:18080`. The server
accepted only the predefined 24-byte `plus-vita-post=takion-12` payload with
the expected content type and `X-Plus-Vita-Test` header, then echoed the body
and returned `X-Plus-Vita-Echo: ok`. On physical Vita hardware,
`cc_http_perform()` returned success with HTTP status 200, the exact 24-byte
echo, and 121 bytes of captured headers. All four POST assertions and the
complete suite passed with zero failures. The one-request server exited after
the test; the temporary private-network firewall rule was then removed.

## First Pylux cloud module

The unmodified Pylux `cloudcatalog_consts.c` module now links into the Vita
target through its public `chiaki/cloudcatalog.h` API. This pure module performs
no HTTP, authentication, cache I/O, or Sony service access. Five assertions on
physical Vita hardware passed: Gaikai language conversion, default language,
supported-locale count, first-locale ordering, and out-of-range handling. The
complete suite again ended with zero failures.

The source's private umbrella header normally imports json-c for its sibling
catalog modules even though this locale/region module does not use json-c. Its
body is skipped for this one translation unit through the existing include
guard, allowing the exact upstream source to compile without introducing a
fake or unused JSON dependency.

## Vita json-c and catalog merge pipeline

The repository now pins Pylux's own json-c dependency version,
`json-c-0.17-20230812`, as a submodule. Its static library cross-compiles for
Vita after explicitly declaring the C/math facilities supplied by VitaSDK's
newlib and disabling static PIC, whose relocation type is not accepted by the
Vita SELF converter. Pylux's unmodified `cloudcatalog_util.c` and
`cloudcatalog_merge.c` modules link against that port.

An embedded offline fixture contains one owned PS5 title and one unowned,
stream-capable PS5 title. On physical Vita hardware, json-c parsed both fixture
documents; Pylux assembled a non-NULL schema-v8 envelope with exactly two
games, sorted the owned title first, and categorized the unowned title as
purchaseable. All six merge assertions and the complete suite passed with zero
failures. No network request, Sony endpoint, credential, token, or cookie was
used.

## Vita catalog cache layer

Pylux's unmodified `cloudcatalog_cache.c` now links into the Vita target. A
physical hardware test used the isolated `ux0:data/plus-vita-cache-test`
directory and a synthetic schema-v8 object. Directory creation, fixture parse,
atomic cache write, cache read, content integrity, and key removal all passed;
the complete suite ended with zero failures. The test removed its cache file
before exit and performed no network request or authentication.

## Offline unified-catalog public API

Pylux's unmodified `cloudcatalog_unified.c` coordinator now links into the Vita
target. The physical test preseeded a complete schema-v8 unified cache, called
the public `chiaki_cloudcatalog_fetch_unified()` API, parsed its returned JSON,
verified the contract and game identity, then invalidated the cache. Both the
coordinator fetch guard and native HTTP boundary recorded exactly zero calls.
All eight assertions and the complete suite passed with zero failures.

Network-producing fetch implementations are deliberately not linked in this
milestone. Fail-closed stubs count and reject any coordinator fall-through,
while the native HTTP adapter independently counts and rejects any request.
This makes the cache-hit proof robust against an accidental missed branch.

The hardware harness now clears its scrolling diagnostic output at completion
and presents a compact final screen with result, assertion totals, and network
attempt counters so future photographs are unambiguous.

## Offline Apollo fetch gate

Pylux's real `cloudcatalog_fetch.c` now compiles for Vita with its public Apollo
fallback symbol enabled. A small compatibility implementation supplies the
four libcurl URL-escaping calls used by this source without linking libcurl.
Authenticated PS Now, Imagic, and owned-library entry points are renamed away
and remain satisfied by fail-closed guards; client device-ID generation also
fails closed.

On physical Vita hardware, RFC 3986 escaping of reserved characters, valid
Sony container country/language extraction, and malformed container rejection
all passed. The clean summary reported 74 checks, zero failures, zero HTTP
attempts, and zero coordinator fetch attempts. No network request or account
data was used.

## Next milestone

With explicit tester approval, invoke only the real public Apollo fallback for
one region through the tested native HTTP backend. Keep all authenticated paths
renamed/blocked, use no credential or token, cap the returned data as already
enforced by the HTTP adapter, and report only status/count—not catalog content.
