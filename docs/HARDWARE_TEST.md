# Physical Vita smoke test

This test initializes native Vita networking, HTTPS, and SSL, but makes no DNS
query or external connection and uses no PlayStation account credentials.

## Install with VitaShell over USB

1. On the Vita, open VitaShell.
2. Press **Start**, set **SELECT button** to **USB**, then press **Circle**.
3. Connect the Vita to the PC by USB and press **Select** in VitaShell.
4. On the PC, copy `build/vita/plus_vita_smoke.vpk` to a convenient folder on
   the mounted Vita drive, such as `data`.
5. Safely eject the Vita drive on the PC, then press **Circle** on the Vita to
   leave USB mode.
6. In VitaShell, open the folder containing the VPK, highlight it, and press
   **X**. Confirm the installation prompt.
7. Return to LiveArea and launch **plus-vita smoke**.

## Record the result

Wait until the final `RESULT` line appears. Do not close the application early.
Take a clear photograph of the complete final screen, especially any `FAIL`
line and the final failure count, then press **X** to exit.

The expected result is `RESULT PASS (0 failures)`. If the application freezes,
turns black, or closes by itself, report the last visible line and what the
network/Wi-Fi state was. Do not enter credentials or launch another game during
this test.

## Controlled certificate-rejection build

An optional physical test can be enabled at configure time with
`PLUS_VITA_TLS_PROBE_URL`. The URL must point to a controlled local HTTPS
server using a deliberately untrusted certificate. This test passes only when
SceHttps reports `SCE_HTTPS_ERROR_SSL_UNKNOWN_CA`; accepting the certificate,
failing to connect, or receiving an unrelated TLS error fails the assertion.

The repository includes `tools/local_https_probe_server.ps1`, which generates
an ephemeral self-signed certificate in memory and serves TLS 1.2. The server
does not persist its private key or collect request data. Any temporary inbound
firewall rule must be restricted to the Vita's LAN address and removed after
the test.

The optional `PLUS_VITA_PUBLIC_HTTPS_URL` setting builds a credential-free
system-trust probe. It performs a GET, requires a verified TLS connection, HTTP
status 200, and a nonempty response body. Use it only with a URL explicitly
approved for the test; it must never inject credentials, cookies, or tokens.
