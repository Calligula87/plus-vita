# plus-vita

Research and development for a native PlayStation Vita client capable of
accessing PlayStation Plus cloud gaming with a legitimate user subscription.

The project is in a native Vita feasibility phase, using the pinned
Pylux/libchiaki source as its direct-cloud implementation basis. The current
VitaSDK smoke target packages successfully and covers protocol serialization,
platform primitives, network lifecycle, local TCP sockets, and the public Sony
CH/EU Apollo catalogue. A physical Vita has completed the verified public
catalogue traversal using an on-device curl/mbedTLS transport; authenticated
PlayStation sessions are not implemented yet.

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

