# Tools

Reproducible analysis and development utilities that do not embed Sony
binaries, secrets, credentials, or protected material.

## Runtime metadata observer

`observe-runtime.ps1` records lifecycle and network endpoint metadata for the
installed PlayStation Plus processes. It does **not** capture packet payloads,
command lines, credentials, tokens, cookies, session identifiers, or keys.

Run it from PowerShell before starting PlayStation Plus:

```powershell
.\tools\observe-runtime.ps1
```

Start the official client normally, perform the test, close it cleanly, and
then press Ctrl+C in the observer window. To stop automatically after ten
minutes:

```powershell
.\tools\observe-runtime.ps1 -DurationSeconds 600
```

Output is JSON Lines under `captures-private/`. That directory is ignored by
Git and its raw contents must never be committed. Only manually reviewed,
redacted findings belong under `research/`.

## Header-only UDP observer

`observe-udp-headers.ps1` uses Windows Packet Monitor from an Administrator
PowerShell session. It filters for IPv4 UDP and logs only 42 bytes per frame:
the ordinary Ethernet, IPv4, and UDP headers, truncated before application
payload. It refuses to run if Packet Monitor is active or has existing filters.

The private ETL output is written under `captures-private/`. Create the
`captures-private/stop-udp-observer` marker to stop it before its time limit.

