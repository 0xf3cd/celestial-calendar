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
