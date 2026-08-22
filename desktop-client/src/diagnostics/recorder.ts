import type { SessionLifecycleState } from "../ipc/session-schema.ts";

export type DiagnosticEvent =
  | Readonly<{
      event: "state-transition";
      from: SessionLifecycleState;
      to: SessionLifecycleState;
      action: string;
      accepted: boolean;
    }>
  | Readonly<{
      event: "command-outcome";
      command: string;
      outcome: "sent" | "rejected" | "failed";
    }>
  | Readonly<{
      event: "recovery-decision";
      decision: string;
    }>;

export type DiagnosticEntry = DiagnosticEvent & Readonly<{ sequence: number }>;

export interface DiagnosticSink {
  record(event: DiagnosticEvent): boolean;
}

const states = new Set([
  "disconnected",
  "ready",
  "allocation-requested",
  "queued",
  "starting",
  "streaming",
  "stopping",
  "shutting-down",
  "error",
]);

const actions = new Set([
  "transport-connected",
  "transport-disconnected",
  "request-game-sent",
  "allocation-retry-sent",
  "queue-state",
  "start-game-sent",
  "streaming-state",
  "stop-sent",
  "shutdown-state",
  "startup-timeout",
  "fault",
]);

const commands = new Set([
  "requestClientId",
  "requestGame",
  "startGame",
  "stop",
  "isStreaming",
  "isQueued",
  "isShuttingDown",
]);

const recoveryDecisions = new Set([
  "wait",
  "retry-allocation",
  "stop-and-fail",
  "disconnect",
  "require-user-decision",
  "fail",
]);

const hasExactKeys = (value: Record<string, unknown>, keys: string[]): boolean => {
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  return actual.length === expected.length && actual.every((key, index) => key === expected[index]);
};

export function isSafeDiagnosticEvent(value: unknown): value is DiagnosticEvent {
  if (typeof value !== "object" || value === null || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;

  switch (record.event) {
    case "state-transition":
      return (
        hasExactKeys(record, ["event", "from", "to", "action", "accepted"]) &&
        states.has(record.from as string) &&
        states.has(record.to as string) &&
        actions.has(record.action as string) &&
        typeof record.accepted === "boolean"
      );
    case "command-outcome":
      return (
        hasExactKeys(record, ["event", "command", "outcome"]) &&
        commands.has(record.command as string) &&
        new Set(["sent", "rejected", "failed"]).has(record.outcome as string)
      );
    case "recovery-decision":
      return (
        hasExactKeys(record, ["event", "decision"]) &&
        recoveryDecisions.has(record.decision as string)
      );
    default:
      return false;
  }
}

export class DiagnosticRecorder implements DiagnosticSink {
  readonly #entries: DiagnosticEntry[] = [];
  readonly #capacity: number;
  #sequence = 0;

  constructor(capacity = 200) {
    if (!Number.isInteger(capacity) || capacity < 1 || capacity > 10_000) {
      throw new RangeError("Diagnostic capacity must be an integer from 1 to 10000");
    }
    this.#capacity = capacity;
  }

  record(event: DiagnosticEvent): boolean {
    if (!isSafeDiagnosticEvent(event)) return false;
    this.#sequence += 1;
    this.#entries.push(Object.freeze({ sequence: this.#sequence, ...event }));
    if (this.#entries.length > this.#capacity) this.#entries.shift();
    return true;
  }

  snapshot(): readonly DiagnosticEntry[] {
    return this.#entries.map((entry) => ({ ...entry }));
  }

  toJSON(): string {
    return JSON.stringify(this.#entries);
  }
}

export const nullDiagnosticSink: DiagnosticSink = Object.freeze({
  record: () => true,
});
