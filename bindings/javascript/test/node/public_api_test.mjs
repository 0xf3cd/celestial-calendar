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
import { copyFile, mkdtemp, readFile, rename, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

if (process.argv.length !== 3) throw new Error("usage: node public_api_test.mjs <staged index.mjs>");

const entryUrl = pathToFileURL(resolve(process.argv[2]));
const wasmPath = fileURLToPath(new URL("./celestial-jieqi.wasm", entryUrl));
const heldWasmPath = `${wasmPath}.init-failure-probe`;
const celestial = await import(entryUrl);

assert.deepEqual(
  Object.keys(celestial).sort(),
  ["CelestialError", "config", "init", "jieqi", "lunar", "moon", "sun", "time"],
  "public module exports",
);
assert.deepEqual(Object.keys(celestial.config), ["setLogVerbosity"]);
assert.deepEqual(
  Object.keys(celestial.time),
  ["ut1ToJd", "ut1ToJde", "jdeToUt1", "localApparentSiderealTime", "deltaT"],
);
assert.deepEqual(
  Object.keys(celestial.sun),
  ["apparentGeocentricCoordinates", "longitudeCrossings", "equationOfTime", "apparentSolarTime"],
);
assert.deepEqual(
  Object.keys(celestial.moon),
  [
    "apparentGeocentricCoordinates",
    "illumination",
    "brightLimbPositionAngle",
    "phaseMoments",
    "newMoonsAfter",
    "newMoonsInYear",
  ],
);
assert.deepEqual(Object.keys(celestial.jieqi), ["moment", "name"]);
assert.deepEqual(Object.keys(celestial.lunar), ["supportedYearRange", "yearInfo", "fromGregorian", "toGregorian"]);

let edges = 0;
const edge = (label, action, ErrorType) => {
  assert.throws(action, ErrorType, label);
  ++edges;
};

assert.throws(
  () => celestial.moon.illumination(2448724.5),
  { name: "Error", message: "Call and await init() before moon.illumination()." },
  "pre-init call",
);
++edges;

await rename(wasmPath, heldWasmPath);
let failedInitialization;
const initializationErrors = [];
const originalConsoleError = console.error;
try {
  console.error = (...args) => initializationErrors.push(args.join(" "));
  failedInitialization = celestial.init();
  assert.strictEqual(celestial.init(), failedInitialization, "concurrent init shares one promise");
  await assert.rejects(failedInitialization);
} finally {
  console.error = originalConsoleError;
  await rename(heldWasmPath, wasmPath);
}
assert(initializationErrors.length > 0, "failed init reported its load error");

const retry = celestial.init();
assert.notStrictEqual(retry, failedInitialization, "failed init can retry");
await retry;
assert.strictEqual(celestial.init(), retry, "completed init reuses one promise");

let happy = 0;
const check = (label, action) => {
  action();
  ++happy;
  console.log(`PASS public ${label}`);
};
const finite = (...values) => values.forEach((value) => assert(Number.isFinite(value)));

check("config.setLogVerbosity", () => assert.equal(celestial.config.setLogVerbosity("none"), undefined));
check("time.ut1ToJd", () => assert.equal(
  celestial.time.ut1ToJd({ year: 2000, month: 1, day: 1, fraction: 0.5 }),
  2451545.0,
));
check("time.ut1ToJde", () => finite(
  celestial.time.ut1ToJde({ year: 2024, month: 6, day: 1, fraction: 0.5 }),
));
check("time.jdeToUt1", () => {
  const value = celestial.time.jdeToUt1(2451545.0);
  assert.deepEqual([value.year, value.month, value.day], [2000, 1, 1]);
  finite(value.fraction);
});
check("time.localApparentSiderealTime", () => {
  const value = celestial.time.localApparentSiderealTime(2451545.0, 0);
  assert(value >= 0 && value < 360);
});
check("time.deltaT", () => {
  for (const model of ["default", "algo1", "algo2", "algo3", "algo4", "algo5"]) {
    finite(celestial.time.deltaT(2024.5, model));
  }
});
check("sun.apparentGeocentricCoordinates", () => {
  const value = celestial.sun.apparentGeocentricCoordinates(2451545.0);
  finite(value.longitudeDeg, value.latitudeDeg, value.radiusAu);
});
check("sun.longitudeCrossings", () => {
  assert.equal(celestial.sun.longitudeCrossings(2024, 0).length, 1);
  const roots = celestial.sun.longitudeCrossings(2024, 280.1);
  assert.equal(roots.length, 2);
  assert(roots[0] < roots[1]);
});
check("sun.equationOfTime", () => finite(celestial.sun.equationOfTime(2451545.0)));
check("sun.apparentSolarTime", () => {
  const value = celestial.sun.apparentSolarTime({ year: 2024, month: 6, day: 1, fraction: 0.5 }, 116.4);
  assert.deepEqual([value.year, value.month, value.day], [2024, 6, 1]);
  finite(value.fraction);
});
check("moon.apparentGeocentricCoordinates", () => {
  const value = celestial.moon.apparentGeocentricCoordinates(2451545.0);
  finite(value.longitudeDeg, value.latitudeDeg, value.distanceKm);
});
check("moon.illumination", () => {
  const value = celestial.moon.illumination(2448724.5);
  assert(Math.abs(value.fraction - 0.6786) < 5e-5);
  finite(value.elongationDeg);
});
check("moon.brightLimbPositionAngle", () => finite(celestial.moon.brightLimbPositionAngle(2448724.5)));
check("moon.phaseMoments", () => {
  for (const phase of ["new", "firstQuarter", "full", "lastQuarter"]) {
    assert(celestial.moon.phaseMoments(2024, phase).length >= 12);
  }
});
check("moon.newMoonsAfter", () => assert.equal(celestial.moon.newMoonsAfter(2451545.0, 3).length, 3));
check("moon.newMoonsInYear", () => assert(celestial.moon.newMoonsInYear(2024).length >= 12));
check("jieqi.moment", () => {
  const value = celestial.jieqi.moment(401, 0);
  assert.deepEqual([value.index, value.year, value.month, value.day], [0, 401, 2, 3]);
});
check("jieqi.name", () => assert.equal(celestial.jieqi.name(0), "立春"));
check("lunar.supportedYearRange", () => {
  assert.deepEqual(celestial.lunar.supportedYearRange("algo1"), { start: 1901, end: 2099 });
  assert.deepEqual(celestial.lunar.supportedYearRange("algo2"), { start: 410, end: 2500 });
  assert.deepEqual(celestial.lunar.supportedYearRange("algo3"), { start: 1600, end: 2199 });
});
// The HKO-backed 2024-02-10 new-year anchor is sourced in lunar/common_test.cpp::ParseLunarYear.
check("lunar.yearInfo", () => {
  const value = celestial.lunar.yearInfo("algo3", 2024);
  assert.deepEqual(value.firstDay, { year: 2024, month: 2, day: 10 });
  assert.equal(value.leapMonth, null);
  assert.deepEqual(value.monthLengths, [29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29]);

  const algo2 = celestial.lunar.yearInfo("algo2", 2024);
  assert.deepEqual(algo2.firstDay, { year: 2024, month: 2, day: 10 });

  // Same HKO-backed leap-year anchor as lunar/common_test.cpp.
  const leap = celestial.lunar.yearInfo("algo1", 2023);
  assert.equal(leap.leapMonth, 2);
  assert.deepEqual(leap.monthLengths, [29, 30, 29, 29, 30, 30, 29, 30, 30, 29, 30, 29, 30]);
});
check("lunar.fromGregorian", () => {
  for (const algorithm of ["algo2", "algo3"]) {
    assert.deepEqual(
      celestial.lunar.fromGregorian(algorithm, { year: 2024, month: 2, day: 10 }),
      { year: 2024, month: 1, day: 1, isLeap: false },
    );
  }
});
check("lunar.toGregorian", () => {
  for (const algorithm of ["algo2", "algo3"]) {
    assert.deepEqual(
      celestial.lunar.toGregorian(algorithm, { year: 2024, month: 1, day: 1, isLeap: false }),
      { year: 2024, month: 2, day: 10 },
    );
  }
});

{
  const lichun = celestial.jieqi.moment(2024, 0);
  finite(celestial.time.ut1ToJd(lichun), celestial.time.ut1ToJde(lichun));
  finite(celestial.sun.apparentSolarTime(lichun, 116.4).fraction);

  const ut1 = celestial.time.jdeToUt1(2451545.0);
  const fromMoment = celestial.lunar.fromGregorian("algo3", ut1);
  assert.equal(fromMoment.year, 1999);
  assert.equal(celestial.lunar.fromGregorian("algo3", fromMoment).isLeap, false);
  assert.deepEqual(
    celestial.lunar.fromGregorian(
      "algo3",
      celestial.lunar.toGregorian("algo3", { ...fromMoment, source: "public output" }),
    ),
    fromMoment,
  );
  console.log("PASS composed public records");
}

// Keep boundary categories aligned with bindings/python/test/consumer/smoke.py::run_acceptance_boundaries;
// each package runner remains independent.
const acceptedBoundaries = [
  ["civil year lower", () => finite(celestial.time.ut1ToJd({ year: 1, month: 1, day: 1, fraction: 0 }))],
  ["civil year upper", () => finite(celestial.time.ut1ToJd({ year: 32767, month: 1, day: 1, fraction: 0 }))],
  ["civil fraction lower", () => assert.equal(
    celestial.time.ut1ToJd({ year: 2000, month: 1, day: 1, fraction: 0 }),
    2451544.5,
  )],
  ["phase year upper", () => assert(celestial.moon.phaseMoments(32766, "new").length >= 12)],
  ["new moons year upper", () => assert(celestial.moon.newMoonsInYear(32766).length >= 12)],
  ["Jieqi year lower", () => assert.equal(celestial.jieqi.moment(401, 0).year, 401)],
  ["Jieqi year upper", () => assert.equal(celestial.jieqi.moment(32766, 0).year, 32766)],
  ["longitude lower", () => finite(celestial.time.localApparentSiderealTime(2451545.0, -180))],
  ["longitude upper", () => finite(celestial.time.localApparentSiderealTime(2451545.0, 180))],
  ["delta T algo1 lower", () => finite(celestial.time.deltaT(-4000, "algo1"))],
  ["algo1 year lower", () => assert.equal(celestial.lunar.yearInfo("algo1", 1901).firstDay.year, 1901)],
  ["algo1 year upper", () => assert.equal(celestial.lunar.yearInfo("algo1", 2099).firstDay.year, 2099)],
  ["algo2 year lower", () => assert.equal(celestial.lunar.yearInfo("algo2", 410).firstDay.year, 410)],
  ["algo2 year upper", () => assert.equal(celestial.lunar.yearInfo("algo2", 2500).firstDay.year, 2500)],
  ["algo3 year lower", () => assert.equal(celestial.lunar.yearInfo("algo3", 1600).firstDay.year, 1600)],
  ["algo3 year upper", () => assert.equal(celestial.lunar.yearInfo("algo3", 2199).firstDay.year, 2199)],
];
for (const [label, action] of acceptedBoundaries) assert.doesNotThrow(action, label);

assert.deepEqual(celestial.sun.longitudeCrossings(1, 281.3), [], "valid no-root result");
assert.deepEqual(celestial.moon.newMoonsAfter(2451545.0, 0), [], "zero requested count");

edge("log level type", () => celestial.config.setLogVerbosity(true), TypeError);
edge("log level value", () => celestial.config.setLogVerbosity("trace"), RangeError);
edge("date object", () => celestial.time.ut1ToJd(null), TypeError);
edge("missing date field", () => celestial.time.ut1ToJd({ year: 2024, month: 1, day: 1 }), TypeError);
assert.doesNotThrow(
  () => celestial.time.ut1ToJd({ year: 2024, month: 1, day: 1, fraction: 0, utc: true }),
  "additional date field",
);
edge(
  "real Gregorian date",
  () => celestial.time.ut1ToJd({ year: 2023, month: 2, day: 29, fraction: 0 }),
  RangeError,
);
edge("fraction range", () => celestial.time.ut1ToJd({ year: 2024, month: 1, day: 1, fraction: 1 }), RangeError);
edge("civil year range", () => celestial.time.ut1ToJd({ year: 0, month: 1, day: 1, fraction: 0 }), RangeError);
edge("number type", () => celestial.time.jdeToUt1("1"), TypeError);
edge("finite number", () => celestial.time.jdeToUt1(Number.NaN), RangeError);
edge("geographic longitude", () => celestial.time.localApparentSiderealTime(2451545.0, 181), RangeError);
edge("finite delta T year", () => celestial.time.deltaT(Number.POSITIVE_INFINITY), RangeError);
edge("delta T model type", () => celestial.time.deltaT(2024, 5), TypeError);
edge("delta T model value", () => celestial.time.deltaT(2024, "future"), RangeError);
edge("algo1 domain", () => celestial.time.deltaT(-4001, "algo1"), RangeError);
edge("algo3 domain", () => celestial.time.deltaT(3000, "algo3"), RangeError);
edge("algo4 domain", () => celestial.time.deltaT(2035, "algo4"), RangeError);
edge("solar year domain", () => celestial.sun.longitudeCrossings(0, 0), RangeError);
edge("solar longitude domain", () => celestial.sun.longitudeCrossings(2024, 360), RangeError);
edge("phase type", () => celestial.moon.phaseMoments(2024, 0), TypeError);
edge("count boolean", () => celestial.moon.newMoonsAfter(2451545.0, true), TypeError);
edge("count integer", () => celestial.moon.newMoonsAfter(2451545.0, 1.5), TypeError);
edge("count non-negative", () => celestial.moon.newMoonsAfter(2451545.0, -1), RangeError);
edge("count resource bound", () => celestial.moon.newMoonsAfter(2451545.0, 4097), RangeError);
edge("Jieqi year", () => celestial.jieqi.moment(400, 0), RangeError);
edge("Jieqi index", () => celestial.jieqi.name(24), RangeError);
edge("lunar algorithm", () => celestial.lunar.yearInfo("algo4", 2024), RangeError);
edge(
  "lunar boolean",
  () => celestial.lunar.toGregorian("algo3", { year: 2024, month: 1, day: 1, isLeap: null }),
  TypeError,
);

let recordingError;
try {
  celestial.time.localApparentSiderealTime(1000000.0, 0);
} catch (error) {
  recordingError = error;
}
assert(recordingError instanceof celestial.CelestialError);
assert.equal(recordingError.operation, "time.localApparentSiderealTime");
assert.equal(recordingError.recorded, true);
++edges;

let lunarError;
try {
  celestial.lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 });
} catch (error) {
  lunarError = error;
}
assert(lunarError instanceof celestial.CelestialError);
assert.equal(lunarError.operation, "lunar.fromGregorian");
assert.equal(lunarError.recorded, true);
assert.match(lunarError.message, /cannot be represented/);
++edges;

assert.equal(celestial.jieqi.name(0), "立春", "module survives translated errors");
assert.equal(happy, 22, "public method denominator");
assert.equal(edges, 30, "public edge denominator");
assert.equal(acceptedBoundaries.length, 16, "public acceptance denominator");
console.log(`PASS public methods ${happy}/22; edge/error cases ${edges}/30`);
console.log(`PASS inclusive public boundaries ${acceptedBoundaries.length}/16`);

const fixtureDirectory = await mkdtemp(resolve(tmpdir(), "celestial-js-contract-"));
try {
  const fixtureEntry = resolve(fixtureDirectory, "index.mjs");
  const source = await readFile(fileURLToPath(entryUrl), "utf8");
  await writeFile(fixtureEntry, source.replace("./celestial-jieqi.mjs", "./mock-module.mjs"), "utf8");
  await copyFile(fileURLToPath(new URL("./bindings.mjs", entryUrl)), resolve(fixtureDirectory, "bindings.mjs"));
  await writeFile(
    resolve(fixtureDirectory, "mock-module.mjs"),
    `export default async () => {
  const buffer = new ArrayBuffer(65_536);
  const M = {
    HEAPU8: new Uint8Array(buffer),
    HEAPU16: new Uint16Array(buffer),
    HEAP32: new Int32Array(buffer),
    HEAPU32: new Uint32Array(buffer),
    HEAPF64: new Float64Array(buffer),
  };
  let next = 8;
  let lastError = "";
  M._malloc = (bytes) => {
    if (globalThis.__celestialFailAllocation === bytes) return 0;
    const ptr = next;
    next += bytes;
    return ptr;
  };
  M._free = () => {};
  M._new_moons_after_jde = (_jde, _slots, count) => count;
  M._moon_phase_moments = (_year, _phase, countPtr, slots) => {
    if (slots === 0) {
      M.HEAPU32[countPtr >> 2] = 2;
      return 0;
    }
    lastError = "native phase fill failed";
    M.HEAPU32[countPtr >> 2] = 0;
    return 0;
  };
  M.ccall = () => lastError;
  return M;
};
`,
    "utf8",
  );

  const fixture = await import(pathToFileURL(fixtureEntry));
  await fixture.init();
  assert.equal(fixture.moon.newMoonsAfter(2451545.0, 4096).length, 4096, "count 4096 accepted");

  globalThis.__celestialFailAllocation = 4096 * 8;
  assert.throws(
    () => fixture.moon.newMoonsAfter(2451545.0, 4096),
    {
      name: "CelestialError",
      message: "moon.newMoonsAfter failed to allocate the WASM output buffer.",
      operation: "moon.newMoonsAfter",
      recorded: false,
    },
    "allocation failure",
  );
  delete globalThis.__celestialFailAllocation;

  assert.throws(
    () => fixture.moon.phaseMoments(2024, "new"),
    {
      name: "CelestialError",
      message: "native phase fill failed",
      operation: "moon.phaseMoments",
      recorded: true,
    },
    "recording fill failure",
  );
  console.log("PASS count boundary 4096/4097; allocation failure; recording fill reason");
} finally {
  delete globalThis.__celestialFailAllocation;
  await rm(fixtureDirectory, { recursive: true, force: true });
}
