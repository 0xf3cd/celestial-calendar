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

import { basename, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { runPackageConsumer } from "../support/package_consumer.mjs";

if (process.argv.length !== 4) throw new Error("usage: node tarball_consumer_test.mjs <primary.tgz> <alias.tgz>");

const tarball = resolve(process.argv[2]);
const alias = resolve(process.argv[3]);
await runPackageConsumer({
  dependencies: { "@0xf3cd/celestial": `file:${tarball}`, "celestial-calendar": `file:${alias}` },
  packageName: "celestial-calendar",
  installArgs: ["--offline", "--ignore-scripts", "--no-audit", "--no-fund", "--package-lock=false"],
  prefix: "celestial-npm-consumer-",
  typeCompiler: fileURLToPath(new URL("../../node_modules/typescript/bin/tsc", import.meta.url)),
  success: `PASS unrelated offline pair, installed types, shared state, and /date without WASM: ${basename(tarball)} + ${basename(alias)}`,
});
console.log("PASS installed version drift rejected");
