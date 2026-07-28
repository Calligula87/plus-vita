# plus-vita

Research and development for a native PlayStation Vita client capable of
accessing PlayStation Plus cloud gaming with a legitimate user subscription.

The project is currently in the feasibility phase. The first objective is to
document the official PC client's architecture, authentication flow, session
negotiation, media transport, codecs, and controller protocol. A desktop
reference client will be attempted before Vita implementation begins.

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
- `desktop-client/` — future platform-neutral reference client.
- `vita-client/` — future VitaSDK client.
- `tools/` — future analysis and development utilities.
- `local-artifacts/` — ignored location for private local files; never commit it.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the gated development plan and
[research/installer-audit.md](research/installer-audit.md) for the first
completed investigation step.

