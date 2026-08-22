import type { SessionLifecycleState } from "../ipc/session-schema.ts";

export interface RecoveryContext {
  readonly state: SessionLifecycleState;
  readonly elapsedMs: number;
  readonly allocationAttempts: number;
}

export type RecoveryDecision =
  | Readonly<{ action: "wait" }>
  | Readonly<{ action: "retry-allocation"; delayMs: number }>
  | Readonly<{ action: "stop-and-fail"; reason: "startup-timeout" }>
  | Readonly<{
      action: "disconnect";
      reason: "stop-timeout" | "shutdown-timeout";
    }>
  | Readonly<{ action: "require-user-decision"; reason: "queue-timeout" | "error-state" }>
  | Readonly<{ action: "fail"; reason: "allocation-attempts-exhausted" }>;

export const recoveryLimits = Object.freeze({
  allocationTimeoutMs: 30_000,
  maxAllocationAttempts: 3,
  allocationRetryDelayMs: [1_000, 2_000] as const,
  queueDecisionTimeoutMs: 30 * 60_000,
  startupTimeoutMs: 30_000,
  stopTimeoutMs: 15_000,
  shutdownTimeoutMs: 15_000,
});

export function evaluateRecovery(context: RecoveryContext): RecoveryDecision {
  const elapsedMs = Math.max(0, context.elapsedMs);

  switch (context.state) {
    case "allocation-requested":
      if (elapsedMs < recoveryLimits.allocationTimeoutMs) return { action: "wait" };
      if (context.allocationAttempts >= recoveryLimits.maxAllocationAttempts) {
        return { action: "fail", reason: "allocation-attempts-exhausted" };
      }
      return {
        action: "retry-allocation",
        delayMs:
          recoveryLimits.allocationRetryDelayMs[
            Math.min(
              context.allocationAttempts - 1,
              recoveryLimits.allocationRetryDelayMs.length - 1,
            )
          ] ?? recoveryLimits.allocationRetryDelayMs[0],
      };

    case "queued":
      return elapsedMs >= recoveryLimits.queueDecisionTimeoutMs
        ? { action: "require-user-decision", reason: "queue-timeout" }
        : { action: "wait" };

    case "starting":
      return elapsedMs >= recoveryLimits.startupTimeoutMs
        ? { action: "stop-and-fail", reason: "startup-timeout" }
        : { action: "wait" };

    case "stopping":
      return elapsedMs >= recoveryLimits.stopTimeoutMs
        ? { action: "disconnect", reason: "stop-timeout" }
        : { action: "wait" };

    case "shutting-down":
      return elapsedMs >= recoveryLimits.shutdownTimeoutMs
        ? { action: "disconnect", reason: "shutdown-timeout" }
        : { action: "wait" };

    case "error":
      return { action: "require-user-decision", reason: "error-state" };

    default:
      return { action: "wait" };
  }
}

