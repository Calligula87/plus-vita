import { SessionController } from "./controller.ts";
import {
  evaluateRecovery,
  type RecoveryDecision,
} from "./recovery-policy.ts";
import type { TransitionResult } from "./reducer.ts";

export type Delay = (delayMs: number) => Promise<void>;

export interface RecoveryExecution {
  readonly decision: RecoveryDecision;
  readonly transition?: TransitionResult;
}

export async function executeRecovery(
  controller: SessionController,
  elapsedMs: number,
  delay: Delay,
): Promise<RecoveryExecution> {
  const decision = evaluateRecovery({
    state: controller.state,
    elapsedMs,
    allocationAttempts: controller.allocationAttempts,
  });

  switch (decision.action) {
    case "wait":
    case "require-user-decision":
      return { decision };

    case "retry-allocation":
      await delay(decision.delayMs);
      return {
        decision,
        transition: await controller.retryRequestGame(false),
      };

    case "stop-and-fail":
      return {
        decision,
        transition: await controller.abortStartup(),
      };

    case "disconnect":
      return {
        decision,
        transition: controller.transition({ type: "transport-disconnected" }),
      };

    case "fail":
      return {
        decision,
        transition: controller.transition({ type: "fault" }),
      };
  }
}

