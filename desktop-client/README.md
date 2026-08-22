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

Raw event inspection returns only a supported discriminator and sorted field
names; it never returns input values and rejects sensitive identifier names.
The synthetic event adapter uses the project-owned `{ type, active }` test
format, which must not be treated as the unresolved Sony event schema.

Run all offline scenarios with:

```powershell
npm run simulate
```

Simulator output contains state, operation, accepted status, and command names
only. Parameter and event values are never included.

