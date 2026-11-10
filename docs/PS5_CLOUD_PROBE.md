# Native PS5 cloud desktop probe

## Evidence so far

[Ticket #48](https://github.com/Calligula87/plus-vita/issues/48) starts with a
desktop run against one eligible native PS5 edition. The pinned Pylux PS Cloud
request advertises `hevc_hw4` and SDR. The probe uses its default 1080p60,
20,000 kbps request with automatic datacenter selection. [Pylux profile](https://github.com/ForWard-Technologies-LLC/Pylux/blob/ca87a49a12bab071948f565ca53ffdad790775d3/lib/src/cloudsession_gaikai.c#L301-L325)

Asobi documents native PS5 cloud streaming and lists H.264 and HEVC support
across the app. Its published overlay fields include stream resolution and
decode time. These sources give a desktop comparison for service access, with
the negotiated codec for native PS5 cloud still unverified. The probe offers
only the default profile pending a reproducible PS Cloud H.264 request.
[Asobi features](https://asobiapp.com/features.html) · [Asobi overlay](https://asobiapp.com/performance-overlay.html)

**Current result** `inconclusive` at the live-account prerequisite. The desktop
probe has synthetic video and failure fixtures. The eligible title and live
session remain pending, so the Vita experiment gate is still closed.

## Build and check on Linux or WSL

Use a clean checkout of the pinned Pylux dependency so the tested request
matches its documented profile. The build uses Pylux's pinned curl with
WebSocket support. Use GCC, CMake, Make, pkg-config, OpenSSL, json-c, miniupnpc,
protoc, Python 3 and Python protobuf.
On Ubuntu the packages are `gcc g++ cmake make pkg-config libssl-dev
libjson-c-dev libminiupnpc-dev protobuf-compiler python3-protobuf ca-certificates`.

```sh
git submodule update --init --recursive -- external/pylux
git -C external/pylux status --short
sh tools/test-ps5-cloud-probe.sh
cmake -S tools/ps5-cloud-probe -B build/debug/ps5-cloud-probe \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/debug/ps5-cloud-probe --parallel
git rev-parse HEAD
git -C external/pylux rev-parse HEAD
sha256sum build/debug/ps5-cloud-probe/ps5-cloud-probe
```

An invocation with no arguments should print one `inconclusive` line at
`stage=setup` with `reason=missing_or_invalid_inputs` and exit 1.

## Run the default profile privately

1. With your own eligible PS Plus Premium account, verify the title and native
   PS5 edition in the PS Cloud catalogue for your account region. If needed,
   inspect the local Pylux `cloud_catalog/unified_catalog_v3.json` cache inside
   its Qt cache directory. Select an eligible row with `platform=ps5` and
   `streamServiceType=pscloud`. Use its `streamIdentifier` and the cache's
   resolved store country and language. The game language is a locale such as
   `en-US`. Keep the catalogue and its identifiers local.
2. Review the result fields below. Run the following in an interactive Bash
   terminal, replacing the placeholders with the local launch identifier and
   the account's store and game locale values. Enter only the NPSSO cookie value
   at the hidden prompt. The value goes to the probe on stdin. See steps 1–2 of
   [Account setup](ACCOUNT_SETUP.md) for the Sony sign-in page.

   ```bash
   (
     set +x
     set +a
     unset probe_npsso
     read -rs -p 'NPSSO cookie value (local only): ' probe_npsso || exit 1
     printf '\n' >&2
     printf '%s\n' "$probe_npsso" | build/debug/ps5-cloud-probe/ps5-cloud-probe \
       'PS5_STREAM_IDENTIFIER' 'US' 'en' 'en-US'
   )
   ```

3. Review and share only the one-line report and the public game name and
   edition, date, Git revisions and binary SHA-256. Keep sign-in material,
   account and session identifiers, raw bodies, video bytes, keys, launch
   specifications and upstream diagnostics local.

After provisioning, the probe waits up to 45 seconds for the active StreamInfo
header and two matching video samples starting at a random-access picture.
It reports the server's active StreamInfo dimensions and, for recognized AVC,
the coded dimensions, profile, level and buffering fields parsed from the SPS.
It stops and joins the transport after inspection or interruption. The `code`
field is a Pylux error number, or the Pylux quit reason when `reason=disconnected`.

| Outcome | Evidence |
| --- | --- |
| `hevc_delivered` | HEVC parameter-set NALs and matching video samples arrived on the default PS Cloud request. |
| `usable_h264` | H.264 headers and matching video samples met the desktop gate. It requires progressive 8-bit 4:2:0 AVC, Baseline/Main/High through Level 4.0, coded dimensions up to 1280×720, one SPS/PPS pair and a declared limit of at most five decoder slots. In-band parameter sets invalidate this check. Confirm picture, audio, controls, stop, relaunch and disconnect recovery on a physical Vita. [Decoder evidence](720P_DECODER_PLAN.md) |
| Explicit profile rejection | Record this only if a future source-backed H.264 request gets an explicit codec/profile rejection after a working baseline. |
| `inconclusive` | The reported stage and reason identify where evidence stopped. Generic provisioning errors, incomplete video and AVC outside the checked limits land here. |

Restrict the result to the tested edition and profile. A working HEVC default
leaves H.264 availability open. If a desktop run meets the H.264 gate, identify
the experimental VPK by build and hash, record the duration and results on Vita,
and compare PS Now play and stop/recovery against the accepted stream build.
