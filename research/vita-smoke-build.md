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

## Next milestone

Exercise the compatibility backend against a controlled POST echo endpoint,
then link the first credential-free Pylux cloud module. Authentication and
Sony service requests remain disabled until those request semantics and module
integration pass independently.
