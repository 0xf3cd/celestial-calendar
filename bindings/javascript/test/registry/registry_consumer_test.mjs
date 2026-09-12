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

import { runPackageConsumer } from "../support/package_consumer.mjs";

if (process.argv.length !== 3 || !/^\d+\.\d+\.\d+$/.test(process.argv[2])) {
  throw new Error("usage: node registry_consumer_test.mjs <version>");
}

const version = process.argv[2];
for (const dependencies of [
  { "@0xf3cd/celestial": version },
  { "celestial-calendar": version },
  { "@0xf3cd/celestial": version, "celestial-calendar": version },
]) {
  await runPackageConsumer({
    dependencies,
    packageName: "celestial-calendar" in dependencies ? "celestial-calendar" : "@0xf3cd/celestial",
    expectedVersion: version,
    installArgs: [
      "--ignore-scripts",
      "--no-audit",
      "--no-fund",
      "--package-lock=false",
      "--registry=https://registry.npmjs.org",
    ],
    prefix: "celestial-npm-registry-consumer-",
    success: `PASS npm registry install ${Object.keys(dependencies).join(" + ")}@${version} (not PyPI)`,
  });
}
