import type { SessionCommand, SessionStateEvent } from "./session-schema.ts";

export type SessionEventListener = (event: SessionStateEvent) => void;

/**
 * Transport seam for a future clean-room implementation.
 *
 * Implementations must not log command values or authenticated event payloads.
 */
export interface SessionTransport {
  readonly connected: boolean;
  send(command: SessionCommand): Promise<void>;
  subscribe(listener: SessionEventListener): () => void;
}

