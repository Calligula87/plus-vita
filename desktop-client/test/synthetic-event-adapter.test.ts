import assert from "node:assert/strict";
import test from "node:test";
import { adaptSyntheticSessionEvent } from "../src/session/synthetic-event-adapter.ts";

test("synthetic events normalize into reducer actions", () => {
  assert.deepEqual(
    adaptSyntheticSessionEvent({ type: "isQueued", active: true }),
    { type: "queue-state", queued: true },
  );
  assert.deepEqual(
    adaptSyntheticSessionEvent({ type: "isStreaming", active: false }),
    { type: "streaming-state", streaming: false },
  );
  assert.deepEqual(
    adaptSyntheticSessionEvent({ type: "isShuttingDown", active: true }),
    { type: "shutdown-state", shuttingDown: true },
  );
});

