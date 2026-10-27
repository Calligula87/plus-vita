<p align="center">
  <img src="vita-client/assets/branding/wordmark.png" alt="plus-vita" width="336">
</p>

# Plus-Vita · v000

Play eligible PlayStation Plus cloud games directly on your PlayStation Vita.
Plus-Vita is a native homebrew client with a cover catalogue, title search,
hardware-decoded video, audio and Vita controls. No PC streaming relay is needed
during play. Initial account setup uses a phone or PC.

![Plus-Vita artwork](vita-client/assets/branding/splash-source.png)

**v000 is the first release version.** Publication is being finalized; use the
[release page](https://github.com/Calligula87/plus-vita/releases) for published
downloads. Development VPKs and older test baselines are separate artifacts.

## What you need

- A homebrew-enabled PS Vita with VitaShell and working Wi-Fi.
- Your own Sony account and a PlayStation Plus subscription that includes cloud
  streaming in your region. The selected game must be eligible for your account.
- A phone or PC browser for Sony sign-in and a way to transfer files to VitaShell.

Plus-Vita is an unofficial project and is not affiliated with Sony Interactive
Entertainment. It does not include games or grant access beyond your account's
subscription and streaming eligibility.

## Install

1. Download **plus-vita-v000.vpk** from the
   [release page](https://github.com/Calligula87/plus-vita/releases).
2. Copy the VPK to your Vita, for example into `ux0:data/`, using VitaShell USB or
   FTP transfer.
3. Select the VPK in VitaShell and install it. Open **Plus-Vita** from LiveArea.

Existing Plus-Vita installations use the same title ID, `PLVT00001`, so this
installs as an update. Keep a private backup of account files before changing an
installation. Normal installation includes the icon, splash and LiveArea art;
opening protected folders decrypted is not required.

The v000 VPK is **4,027,154 bytes**. Its SHA-256 is:

```text
90BD483B6092FD4162D4B37083EA43D2EB2919257342AF2990B6F744369C996A
```

## Connect your account

1. On a phone or PC, sign in to your own account at
   [PlayStation](https://www.playstation.com/).
2. In the same browser/profile, open the
   [Sony sign-in response](https://ca.account.sony.com/api/v1/ssocookie).
3. Save the JSON response as **plus-vita-auth.json** and transfer it to
   **ux0:data/plus-vita-auth.json**. Save the response itself; you do not need to
   extract or type the token.
4. Launch Plus-Vita. It checks your saved sign-in and opens the catalogue after
   successful authorization. If already open, choose **Get started → Continue →
   Import JSON**.

**Keep that JSON private: it grants account access.** Never attach it to an issue,
include it in a screenshot or share an entire app-data folder. The original JSON
stays on your Vita. Remember defaults On; Remember Off and Forget pause automatic
reuse until you explicitly reconnect. Local account files are readable by someone
with file access to the Vita.

See the [account setup and recovery guide](docs/ACCOUNT_SETUP.md) for Remember,
Forget, renewed sign-in and temporary network errors.

## Play and controls

Browse the cover catalogue, search with **Triangle** or the **Search** button,
open a game's details and choose **Play**. Returning from details or a game
preserves your catalogue search and selection.

| Action | Control |
| --- | --- |
| Move through menus | Left stick or touch |
| Select / go back | Cross / Circle |
| Search catalogue | Triangle or Search button |
| In-game buttons and sticks | Corresponding PlayStation controls |
| L2 / R2 | Rear touchpad top-left / top-right |
| L3 / R3 | Rear touchpad bottom-left / bottom-right |
| Move / press the controller touchpad | Front touch / two front touches |
| Stop streaming and return | Hold **START + SELECT for two seconds** |

The D-pad works in games; it does not move app-menu focus. The opening splash
can be skipped with Cross or touch. **Settings and help** contains the controller
guide, account controls, version and diagnostic status.

## Current limits

- One account at a time and an English interface.
- Sony sign-in happens in a phone/PC browser. There is no direct Vita password
  login or automatic credential renewal. If Sony rejects an expired credential,
  save and explicitly import a fresh JSON response.
- Streaming uses the fixed accepted 720p profile. Resolution/bitrate tuning and
  controller remapping are not included.
- Catalogue availability does not guarantee a title can stream. Service,
  subscription, region and validated launch information determine eligibility.
- Wi-Fi loss or sleep ends the affected cloud connection. Retry starts a fresh
  connection; same-session resume is not promised.
- There is no app-imposed ten-minute play limit. Network and service conditions
  can still interrupt play.

## Report a problem

Open a [GitHub issue](https://github.com/Calligula87/plus-vita/issues/new/choose)
with the app version, reproduction steps, expected result and actual result.
Include only non-unique device/software details.

The current diagnostic log is `ux0:data/plus-vita-allocation.log`. Copy it before
another app launch or Play attempt replaces it, review it privately, and attach
only the reviewed log if useful. Sharing is manual; the app does not upload logs.
**Never share account JSON, tokens, browser sign-in responses or whole data folders.**
See [reporting instructions](docs/REPORTING_ISSUES.md).

## Source and building

The v000 executable was built from source commit
`ae350183b44b663b76f7683933597fcbfdd514a7`, with dependency revisions and toolchain
identity recorded in [release/v000.json](release/v000.json).

```sh
git clone --recurse-submodules https://github.com/Calligula87/plus-vita.git
cd plus-vita
```

Use [the anonymous release build procedure](release/README.md) for the distribution
profile and actual-VPK privacy checks. The
[native client guide](vita-client/README.md) also documents development targets.
Only committed sources and pinned dependencies are release build inputs.

The [v000 validation record](docs/V000.md) distinguishes software checks from
reported Vita acceptance. Older baselines remain documented for rollback and
comparison. The [roadmap](docs/ROADMAP.md) retains the development history.

## Credits and third-party licenses

The native cloud implementation builds on
[Pylux](https://github.com/ForWard-Technologies-LLC/Pylux) and its libchiaki code.
Pylux supplies its [AGPL-3.0 license](https://github.com/ForWard-Technologies-LLC/Pylux/blob/ca87a49a12bab071948f565ca53ffdad790775d3/COPYING).
Vita support uses [VitaSDK](https://vitasdk.org/), with json-c, nanopb, libcurl,
mbedTLS, Opus, GF-Complete, Jerasure, stb_image, FreeType and Noto Sans among the
dependencies. Dependency code retains its own license and notices; see the
pinned source trees, [decoder notice](vita-client/third_party/README.md),
[font notices](vita-client/assets/fonts/NOTICE.txt) and VPK `licenses/` directory.
The custom artwork and its preparation are documented
[here](vita-client/assets/branding/README.md).
