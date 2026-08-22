import assert from "node:assert/strict";
import test from "node:test";
import type { SessionCommand, SessionStateEvent } from "../src/ipc/session-schema.ts";
import type {
  SessionEventListener,
  SessionTransport,
} from "../src/ipc/transport.ts";
import { SessionController } from "../src/session/controller.ts";

class SyntheticTransport implements SessionTransport {
  connected = true;
  failNextSend = false;
  readonly sent: SessionCommand[] = [];

  async send(command: SessionCommand): Promise<void> {
    if (this.failNextSend) {
      this.failNextSend = false;
      throw new Error("synthetic failure");
    }
    this.sent.push(command);
  }

  subscribe(_listener: SessionEventListener): () => void {
    return () => {};
  }

  emit(_event: SessionStateEvent): void {}
}

test("controller sends only after an accepted transition", async () => {
  const transport = new SyntheticTransport();
  const controller = new SessionController(transport);

  controller.transition({ type: "transport-connected" });
  const rejected = await controller.startGame();
  assert.equal(rejected.accepted, false);
  assert.equal(transport.sent.length, 0);

  const requested = await controller.requestGame(false);
  assert.equal(requested.accepted, true);
  assert.deepEqual(transport.sent, [
    {
      command: "requestGame",
      params: { forceLogout: false },
      source: "reference-client",
      target: "QAS",
    },
  ]);
});

test("disconnected transport fails closed", async () => {
  const transport = new SyntheticTransport();
  const controller = new SessionController(transport);
  controller.transition({ type: "transport-connected" });
  transport.connected = false;

  await assert.rejects(controller.requestGame(), /transport is disconnected/);
  assert.equal(controller.state, "disconnected");
});

test("transport send failure moves the controller to error", async () => {
  const transport = new SyntheticTransport();
  const controller = new SessionController(transport);
  controller.transition({ type: "transport-connected" });
  transport.failNextSend = true;

  await assert.rejects(controller.requestGame(), /transport send failed/);
  assert.equal(controller.state, "error");
  assert.equal(transport.sent.length, 0);
});
