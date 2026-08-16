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
import { statSync } from "node:fs";

const MAX_WASM_BYTES = 465_000;
const WASM_URL = new URL("../build/wasm/celestial-jieqi.wasm", import.meta.url);

await import("../bindings/javascript/test/abi/verify.mjs");
await import("../bindings/javascript/test/abi/raw_protocol_test.mjs");

const size = statSync(WASM_URL).size;
assert(size <= MAX_WASM_BYTES, `WASM size ${size} exceeds ${MAX_WASM_BYTES} bytes`);
console.log(`PASS raw WASM size ${size} <= ${MAX_WASM_BYTES} bytes`);
console.log("wasm_check: all green");
