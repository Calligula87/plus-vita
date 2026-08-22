# Desktop reference client

Reserved for a minimal platform-neutral protocol client. Implementation begins
only after the feasibility audit reaches a go decision.

The first clean-room interface artifact is
`src/ipc/session-schema.ts`: value-free TypeScript declarations for the
statically confirmed AGL-to-QAS session commands and event discriminators. The
proposed lifecycle and its unresolved assumptions are documented in
`../docs/session-state-machine.md`.

## Synthetic reference skeleton

The current skeleton contains an injectable transport interface, a pure session
reducer, and a controller that sends only after accepted state transitions. It
does not implement a Sony transport or connect to any external service.

With Node.js 24 or newer:

```powershell
npm test
```

All tests use synthetic messages and transports.

