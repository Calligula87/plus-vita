# Session lifecycle state machine

This document describes a value-free clean-room model for the statically
visible AGL-to-QAS session boundary. It contains no authenticated message
values. Confirmed bridge facts are separated from inferred sequencing.

## Confirmed command schemas

All listed commands target `QAS`, include a `source` string, and carry a
`params` object.

| Command | Statically visible parameter fields |
| --- | --- |
| `requestClientId` | none |
| `requestGame` | `forceLogout: boolean` |
| `startGame` | none |
| `requestSwitchGame` | `switchGameSpec: unknown` |
| `stop` | none |
| `isStreaming` | none |
| `isQueued` | none |
| `isShuttingDown` | none |

The bridge recognizes incoming event discriminators named `isStreaming`,
`isQueued`, and `isShuttingDown`. Their value-field names and response
correlation rules are not established by the installed static sources and must
remain opaque.

## Proposed lifecycle model

The following transition order is a clean-room implementation hypothesis, not
a claim about the complete service protocol:

```text
disconnected
    -> ready
    -> allocation-requested       requestGame
    -> queued                     isQueued event, value shape unresolved
    -> starting                   startGame
    -> streaming                  isStreaming event, value shape unresolved
    -> stopping                   stop
    -> ready

any active state
    -> shutting-down              isShuttingDown event, value shape unresolved
    -> disconnected

any state
    -> error                      error event
```

`requestClientId` is modeled as a prerequisite operation in `ready`, but the
static bridge does not prove whether it must complete before every allocation.
`requestSwitchGame` is modeled as an alternative transition from `streaming`
back to `allocation-requested`; its opaque specification prevents a stronger
contract.

## Implementation invariants

- Never log or serialize unknown event values.
- Never persist IPC authentication material.
- Reject commands whose target is not exactly `QAS` in the session module.
- Treat unknown commands and fields as unsupported, not as permission to infer
  their meaning.
- Keep transport authentication separate from lifecycle state.
- Require an explicit state transition before starting or stopping media and
  controller subsystems.

## Remaining questions

1. Which value-field name represents the boolean state in the three state
   events?
2. Are state events unsolicited, query responses, or both?
3. How are request and response messages correlated?
4. Does `requestGame` allocate immediately or only prepare a later
   `startGame` request?
5. Which error categories return the state machine to `ready`, and which force
   shutdown?

These questions should first be pursued through additional static artifacts or
officially emitted diagnostics. Live authenticated IPC interception remains out
of scope unless a specific feasibility decision cannot be made otherwise.

