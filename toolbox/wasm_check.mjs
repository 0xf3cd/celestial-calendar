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

import assert from "node:assert/strict";
import { statSync } from "node:fs";

const MAX_WASM_BYTES = 465_000;
const WASM_URL = new URL("../build/wasm/celestial-jieqi.wasm", import.meta.url);

await import("../bindings/javascript/test/abi/verify.mjs");
await import("../bindings/javascript/test/abi/raw_protocol_test.mjs");

const size = statSync(WASM_URL).size;
assert(size <= MAX_WASM_BYTES, `WASM size ${size} exceeds ${MAX_WASM_BYTES} bytes`);
console.log(`PASS raw WASM size ${size} <= ${MAX_WASM_BYTES} bytes`);
console.log("wasm_check: all green");
