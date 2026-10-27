# Plus-Vita v000

The first Plus-Vita release version brings eligible PlayStation Plus cloud games
to a homebrew-enabled PlayStation Vita, using your own account and subscription.

## Included

- Native 720p video, audio and Vita controls using the accepted independent-input
  and deferred-logging profile.
- Cover catalogue, title search, game details and a graphical Play flow.
- Phone/PC Sony sign-in JSON import, remembered account, account label and Forget.
- Explicit stop/retry, connection cancellation and network/sleep recovery.
- Custom app icon, opening splash, LiveArea artwork and blue/gold menus with the
  text-only plus-vita header.
- Settings, controller help and bounded diagnostics with manual sharing.

## Install and set up

Install **plus-vita-v000.vpk** through VitaShell. Updates use title ID `PLVT00001`.
Follow the [README](https://github.com/Calligula87/plus-vita#readme) for installation,
controls, limitations and account setup. No protected-folder decryption is needed.

Sign-in requires your own eligible Sony account/subscription and a browser on a
phone or PC. Keep the saved sign-in JSON private. Direct Vita password login,
automatic credential renewal, quality tuning and same-session resume are not
included. Catalogue listing alone does not guarantee streaming eligibility.

## Artifact identity

- VPK: `plus-vita-v000.vpk`
- Size: 4,027,154 bytes
- SHA-256: `90BD483B6092FD4162D4B37083EA43D2EB2919257342AF2990B6F744369C996A`
- Executable source: `ae350183b44b663b76f7683933597fcbfdd514a7`
- Public version: **v000**; Vita's internal numeric package revision: `00.11`.

The [identity manifest](https://github.com/Calligula87/plus-vita/blob/main/release/v000.json)
records pinned dependencies and toolchain. The
[validation record](https://github.com/Calligula87/plus-vita/blob/main/docs/V000.md)
records the scope of software and hardware evidence. No hosted-CI pass is claimed
by these notes.

## Help

[Report an issue](https://github.com/Calligula87/plus-vita/issues/new/choose) with
version and reproduction steps. Copy the current diagnostic log before another
launch/Play attempt, review it privately and share only the reviewed log if
needed. Never upload account JSON, tokens, browser responses or full data folders.
