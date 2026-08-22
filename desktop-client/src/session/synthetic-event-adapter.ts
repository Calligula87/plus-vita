import type { SessionStateEventType } from "../ipc/session-schema.ts";
import type { SessionAction } from "./reducer.ts";

/** This is a test-only contract and is not a Sony IPC payload schema. */
export interface SyntheticSessionEvent {
  readonly type: SessionStateEventType;
  readonly active: boolean;
}

export function adaptSyntheticSessionEvent(
  event: SyntheticSessionEvent,
): SessionAction {
  switch (event.type) {
    case "isQueued":
      return { type: "queue-state", queued: event.active };
    case "isStreaming":
      return { type: "streaming-state", streaming: event.active };
    case "isShuttingDown":
      return { type: "shutdown-state", shuttingDown: event.active };
  }
}

