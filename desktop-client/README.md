# Desktop reference client

Reserved for a minimal platform-neutral protocol client. Implementation begins
only after the feasibility audit reaches a go decision.

The first clean-room interface artifact is
`src/ipc/session-schema.ts`: value-free TypeScript declarations for the
statically confirmed AGL-to-QAS session commands and event discriminators. The
proposed lifecycle and its unresolved assumptions are documented in
`../docs/session-state-machine.md`.

