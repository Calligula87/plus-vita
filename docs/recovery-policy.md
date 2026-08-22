# Desktop reference-client recovery policy

This is a client-local, value-free policy for deterministic testing. Its limits
are hypotheses, not observed Sony service requirements, and can be changed
without altering protocol schemas.

| State | Limit | Decision |
| --- | ---: | --- |
| allocation requested | 30 seconds | retry at most twice, after 1 and 2 seconds |
| allocation requested, third timeout | 30 seconds | fail closed |
| queued | 30 minutes | require an explicit user decision |
| starting | 30 seconds | request stop and enter error handling |
| stopping | 15 seconds | disconnect locally |
| shutting down | 15 seconds | disconnect locally |
| error | immediate | require an explicit user decision |

Queueing is deliberately not retried automatically because doing so could lose
a valid queue position. Authentication failures are not retryable by this
policy and must never trigger automated credential or token reuse outside the
transport's legitimate lifecycle.

The implementation accepts explicit elapsed milliseconds and attempt counts;
it does not read wall-clock time. This keeps simulation and tests deterministic.

