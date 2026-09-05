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

import { runPackageConsumer } from "../support/package_consumer.mjs";

if (process.argv.length !== 3 || !/^\d+\.\d+\.\d+$/.test(process.argv[2])) {
  throw new Error("usage: node registry_consumer_test.mjs <version>");
}

const version = process.argv[2];
await runPackageConsumer({
  dependency: version,
  expectedVersion: version,
  installArgs: [
    "--ignore-scripts",
    "--no-audit",
    "--no-fund",
    "--package-lock=false",
    "--registry=https://registry.npmjs.org",
  ],
  prefix: "celestial-npm-registry-consumer-",
  success: `PASS registry install @0xf3cd/celestial@${version}`,
});
