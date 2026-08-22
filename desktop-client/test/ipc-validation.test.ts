import assert from "node:assert/strict";
import test from "node:test";
import {
  inspectSessionEventShape,
  isConfirmedSessionCommand,
} from "../src/ipc/validation.ts";

test("event inspection returns names but not values", () => {
  const input = { type: "isStreaming", active: true, sequence: 7 };
  const result = inspectSessionEventShape(input);

  assert.deepEqual(result, {
    accepted: true,
    type: "isStreaming",
    fieldNames: ["active", "sequence", "type"],
  });
  assert.equal(JSON.stringify(result).includes("true"), true); // accepted flag only
  assert.equal("sequence" in result, false);
});

test("event inspection rejects sensitive field names", () => {
  assert.deepEqual(
    inspectSessionEventShape({ type: "isQueued", token: "synthetic-secret" }),
    { accepted: false, reason: "sensitive-field-name" },
  );
});

test("confirmed command validator accepts exact static shapes", () => {
  assert.equal(
    isConfirmedSessionCommand({
      command: "requestGame",
      params: { forceLogout: false },
      source: "synthetic-client",
      target: "QAS",
    }),
    true,
  );
  assert.equal(
    isConfirmedSessionCommand({
      command: "startGame",
      params: {},
      source: "synthetic-client",
      target: "QAS",
    }),
    true,
  );
});

test("confirmed command validator rejects extra and opaque fields", () => {
  assert.equal(
    isConfirmedSessionCommand({
      command: "requestGame",
      params: { forceLogout: false, sessionKey: "synthetic-secret" },
      source: "synthetic-client",
      target: "QAS",
    }),
    false,
  );
  assert.equal(
    isConfirmedSessionCommand({
      command: "requestSwitchGame",
      params: { switchGameSpec: {} },
      source: "synthetic-client",
      target: "QAS",
    }),
    false,
  );
});

