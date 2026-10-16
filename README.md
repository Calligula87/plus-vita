# plus-vita

Research and development for a native PlayStation Vita client capable of
accessing PlayStation Plus cloud gaming with a legitimate user subscription.

The native Vita client uses pinned Pylux/libchiaki sources for direct cloud
streaming. The maintainer has tested and accepted the combined input-worker
and deferred-logging VPK as a **playable pre-release milestone**. Picture, sound,
controls/releases and stop/restart passed the hardware comparison, with better
perceived response than the original baseline retained in the combined build.

The accepted package is `plus-vita-input-worker-deferred-log-1.vpk`, build
`live-720p-refs5-input-worker-deferred-log-1`. See the
[playable milestone](docs/PLAYABLE_MILESTONE.md) for its SHA256, build options,
fallback and remaining limitations, and the
[hardware comparison](docs/LATENCY_COMBINED_RESULT.md) for measured evidence.
This closes the first input/logging improvement phase and provides the working
baseline for release preparation. A public release has not been published.

## Project boundaries

- Do not commit or redistribute Sony executables, libraries, drivers, assets,
  credentials, tokens, or authenticated packet captures.
- Do not bypass DRM, authentication, encryption, device attestation, or access
  controls.
- Use only legitimate accounts, subscriptions, and user-authorized sessions.
- Keep private analysis artifacts in ignored local directories.

## Repository layout

- `docs/` — architecture decisions, feasibility notes, and roadmap.
- `research/` — sanitized protocol and client research.
- `desktop-client/` — platform-neutral reference experiments.
- `vita-client/` — native VitaSDK feasibility client.
- `tools/` — future analysis and development utilities.
- `local-artifacts/` — ignored location for private local files; never commit it.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the gated development plan and
[research/installer-audit.md](research/installer-audit.md) for the first
completed investigation step.

