import { runFeasibilityDemo } from "./feasibility-demo.ts";

const demonstration = await runFeasibilityDemo();
process.stdout.write(`${JSON.stringify(demonstration, null, 2)}\n`);

