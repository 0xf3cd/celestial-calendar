/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
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

await celestial.init();
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
