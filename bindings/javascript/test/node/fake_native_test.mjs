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

import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { testFakeNative } from "../support/fake_native.mjs";

if (process.argv.length !== 3) throw new Error("usage: node fake_native_test.mjs <index.mjs>");
await testFakeNative(pathToFileURL(resolve(process.argv[2])));
