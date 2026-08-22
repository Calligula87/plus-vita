# Diagnostic data policy

Desktop reference-client diagnostics are allowlist-only and memory-bounded.
They may contain:

- a monotonically increasing local sequence number;
- known lifecycle states and action names;
- known command names;
- accepted/rejected status;
- fixed recovery decision names;
- fixed command outcomes.

They must never contain IPC or event values, parameters, error text, network
addresses, ports, account or game data, authentication material, session data,
or timestamps. Runtime validation rejects unknown event variants and extra
fields. The default recorder holds at most 200 entries in memory and performs
no file or network writes.

Diagnostic persistence is deliberately absent. Any future export feature must
apply the same runtime allowlist and require an explicit user action.

