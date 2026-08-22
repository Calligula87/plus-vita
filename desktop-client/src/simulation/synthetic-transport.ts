import type { SessionCommand } from "../ipc/session-schema.ts";
import type {
  SessionEventListener,
  SessionTransport,
} from "../ipc/transport.ts";

/** Offline-only transport that records command names, never parameter values. */
export class SyntheticTransport implements SessionTransport {
  connected = true;
  failNextSend = false;
  readonly commandNames: string[] = [];
  readonly #listeners = new Set<SessionEventListener>();

  async send(command: SessionCommand): Promise<void> {
    if (this.failNextSend) {
      this.failNextSend = false;
      throw new Error("synthetic transport failure");
    }
    this.commandNames.push(command.command);
  }

  subscribe(listener: SessionEventListener): () => void {
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }
}

