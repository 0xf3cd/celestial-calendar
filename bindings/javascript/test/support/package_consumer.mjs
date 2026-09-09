/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";

export async function runPackageConsumer({ dependency, expectedVersion, installArgs, prefix, success }) {
  const consumer = await mkdtemp(join(tmpdir(), prefix));
  const cache = join(consumer, "npm-cache");
  const run = (command, args) => {
    const completed = spawnSync(command, args, {
      cwd: consumer,
      encoding: "utf8",
      env: { ...process.env, npm_config_cache: cache },
    });
    if (completed.status !== 0) {
      throw new Error(
        `${command} ${args.join(" ")} failed (${completed.status})\n${completed.stdout}\n${completed.stderr}`,
      );
    }
    return completed;
  };

  try {
    await writeFile(
      join(consumer, "package.json"),
      JSON.stringify({
        name: "celestial-package-consumer",
        version: "0.0.0",
        private: true,
        type: "module",
        dependencies: { "@0xf3cd/celestial": dependency },
      }, null, 2),
    );
    run("npm", ["install", ...installArgs]);
    const installed = JSON.parse(
      await readFile(join(consumer, "node_modules", "@0xf3cd", "celestial", "package.json"), "utf8"),
    );
    assert.equal(installed.name, "@0xf3cd/celestial");
    if (expectedVersion !== undefined) assert.equal(installed.version, expectedVersion);

    await writeFile(
      join(consumer, "consumer.mjs"),
      `import assert from "node:assert/strict";
import * as celestial from "@0xf3cd/celestial";
import {
  dateToCivilUtc, civilUtcToDate, dateToCivilAtOffset, civilAtOffsetToDate,
} from "@0xf3cd/celestial/date";

const date = new Date("2024-02-03T16:00:00.000Z");
const utc = dateToCivilUtc(date);
assert.deepEqual(utc, {
  year: 2024, month: 2, day: 3, fraction: 2 / 3, hour: 16, minute: 0, second: 0,
});
assert.equal(civilUtcToDate(utc).getTime(), date.getTime());
const local = dateToCivilAtOffset(date, 480);
assert.deepEqual(local, {
  year: 2024, month: 2, day: 4, fraction: 0, hour: 0, minute: 0, second: 0,
});
assert.equal(civilAtOffsetToDate(local, 480).getTime(), date.getTime());

await celestial.init();
const lichun = celestial.jieqi.moment(2024, celestial.Jieqi.LICHUN);
assert.deepEqual(Object.keys(lichun).sort(), ["jieqi", "momentUt1"]);
assert.equal(lichun.jieqi, celestial.Jieqi.LICHUN);
assert.equal(lichun.momentUt1.year, 2024);
assert(Number.isFinite(celestial.time.ut1ToJd(lichun.momentUt1)));
const value = celestial.moon.illumination(2448724.5);
assert(Math.abs(value.fraction - 0.6786) < 5e-5);
console.log(JSON.stringify({ fraction: value.fraction, operation: "moon.illumination" }));
`,
    );
    const executed = run(process.execPath, ["consumer.mjs"]);
    const result = JSON.parse(executed.stdout);
    assert.equal(result.operation, "moon.illumination");
    console.log(success);
  } finally {
    await rm(consumer, { recursive: true, force: true });
  }
}
