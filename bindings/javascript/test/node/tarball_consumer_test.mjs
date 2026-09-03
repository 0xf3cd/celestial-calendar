/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import { basename, dirname, resolve } from "node:path";

import { runPackageConsumer } from "../support/package_consumer.mjs";

if (process.argv.length !== 3) throw new Error("usage: node tarball_consumer_test.mjs <package.tgz>");

const tarball = resolve(process.argv[2]);
await runPackageConsumer({
  dependency: `file:${tarball}`,
  installArgs: ["--offline", "--ignore-scripts", "--no-audit", "--no-fund", "--package-lock=false"],
  prefix: "celestial-npm-consumer-",
  success: `PASS unrelated offline install ${basename(tarball)} from ${dirname(tarball)}`,
});
