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
import { copyFile, mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

if (process.argv.length < 3 || process.argv.length > 4 ||
    (process.argv.length === 4 && process.argv[3] !== "--exhaustive") || process.argv[2].startsWith("--")) {
  throw new Error("usage: node date_test.mjs <staged date.mjs> [--exhaustive]");
}

const entryUrl = pathToFileURL(resolve(process.argv[2]));
const exhaustive = process.argv[3] === "--exhaustive";
const millisecondsPerDay = 86_400_000;
const started = performance.now();

// Only the date entry and its pure helpers exist here: a root/glue import cannot resolve.
const fixtureDirectory = await mkdtemp(resolve(tmpdir(), "celestial-date-only-"));
const originalFetch = globalThis.fetch;
const originalWebAssembly = globalThis.WebAssembly;
try {
  await copyFile(fileURLToPath(entryUrl), resolve(fixtureDirectory, "date.mjs"));
  await copyFile(fileURLToPath(new URL("./validation.mjs", entryUrl)), resolve(fixtureDirectory, "validation.mjs"));
  globalThis.fetch = () => assert.fail("date-only import/conversion must not fetch");
  globalThis.WebAssembly = new Proxy(originalWebAssembly, {
    get: () => assert.fail("date-only import/conversion must not use WebAssembly"),
  });
  const isolated = await import(pathToFileURL(resolve(fixtureDirectory, "date.mjs")));
  const instant = new Date("2024-02-29T00:13:00.001Z");
  assert.equal(isolated.civilUtcToDate(isolated.dateToCivilUtc(instant)).getTime(), instant.getTime());
  assert.equal(
    isolated.civilAtOffsetToDate(isolated.dateToCivilAtOffset(instant, 480), 480).getTime(),
    instant.getTime(),
  );
} finally {
  globalThis.fetch = originalFetch;
  globalThis.WebAssembly = originalWebAssembly;
  await rm(fixtureDirectory, { recursive: true, force: true });
}
console.log("PASS date-only import and all four conversions without root, WASM or init");

const date = await import(entryUrl);
assert.deepEqual(Object.keys(date).sort(), [
  "civilAtOffsetToDate", "civilUtcToDate", "dateToCivilAtOffset", "dateToCivilUtc",
]);

let directed = 0;
const check = (label, action) => {
  action();
  ++directed;
  console.log(`PASS date ${label}`);
};
const civilResult = (value, year, month, day, hour, minute, milliseconds) => {
  assert.deepEqual(value, {
    year, month, day, fraction: milliseconds / millisecondsPerDay, hour, minute,
    second: milliseconds / 1000 - hour * 3600 - minute * 60,
  });
};

for (const year of [1, 99, 100, 9999, 32767]) {
  check(`year ${year} exact UTC and offset round trips`, () => {
    const instant = new Date(0);
    instant.setUTCFullYear(year, 5, 15);
    instant.setUTCHours(12, 34, 56, 789);
    const value = date.dateToCivilUtc(instant);
    civilResult(value, year, 6, 15, 12, 34, 45_296_789);
    assert.equal(date.civilUtcToDate(value).getTime(), instant.getTime());
    for (const offset of [-1439, -480, 0, 480, 1439]) {
      const local = date.dateToCivilAtOffset(instant, offset);
      assert.equal(date.civilAtOffsetToDate(local, offset).getTime(), instant.getTime());
      assert.equal(local.year, year);
    }
  });
}

check("negative epoch -1 ms", () => {
  const value = date.dateToCivilUtc(new Date(-1));
  civilResult(value, 1969, 12, 31, 23, 59, millisecondsPerDay - 1);
  assert.equal(date.civilUtcToDate(value).getTime(), -1);
});

check("minute boundary 00:13:00", () => {
  const instant = new Date("2024-02-29T00:13:00.000Z");
  const value = date.dateToCivilUtc(instant);
  assert.equal(value.fraction * 86400, 779.9999999999999, "naive day-fraction decomposition undershoots");
  civilResult(value, 2024, 2, 29, 0, 13, 780_000);
  assert.equal(date.civilUtcToDate(value).getTime(), instant.getTime());
});

check("Gregorian leap days", () => {
  for (const year of [4, 400, 2000, 2024]) {
    const civil = { year, month: 2, day: 29, fraction: 0 };
    const instant = date.civilUtcToDate(civil);
    civilResult(date.dateToCivilUtc(instant), year, 2, 29, 0, 0, 0);
  }
});

check("east-positive offsets and both day carries", () => {
  const cases = [
    ["2024-12-31T23:30:00.000Z", 60, 2025, 1, 1, 0, 30, 1_800_000],
    ["2024-03-01T00:30:00.000Z", -60, 2024, 2, 29, 23, 30, 84_600_000],
    ["2024-02-29T00:00:00.000Z", 1439, 2024, 2, 29, 23, 59, 86_340_000],
    ["2024-02-29T00:00:00.000Z", -1439, 2024, 2, 28, 0, 1, 60_000],
  ];
  for (const [iso, offset, year, month, day, hour, minute, milliseconds] of cases) {
    const instant = new Date(iso);
    const local = date.dateToCivilAtOffset(instant, offset);
    civilResult(local, year, month, day, hour, minute, milliseconds);
    assert.equal(date.civilAtOffsetToDate(local, offset).getTime(), instant.getTime());
  }
});

check("nearest millisecond and exact half toward next instant", () => {
  for (const [milliseconds, rounded] of [[0.49, 0], [0.5, 1], [0.51, 1], [1.5, 2]]) {
    const civil = { year: 1969, month: 12, day: 31, fraction: milliseconds / millisecondsPerDay };
    assert.equal(civil.fraction * millisecondsPerDay, milliseconds, "represented rounding input");
    assert.equal(date.civilUtcToDate(civil).getTime(), -millisecondsPerDay + rounded);
    assert.equal(date.civilAtOffsetToDate(civil, 60).getTime(), -millisecondsPerDay + rounded - 3_600_000);
  }
});

check("rounding carries into next civil day", () => {
  for (const [year, month, day, next] of [
    [2024, 2, 28, "2024-02-29T00:00:00.000Z"],
    [2024, 2, 29, "2024-03-01T00:00:00.000Z"],
    [2024, 12, 31, "2025-01-01T00:00:00.000Z"],
  ]) {
    const fraction = (millisecondsPerDay - 0.5) / millisecondsPerDay;
    assert.equal(fraction * millisecondsPerDay, millisecondsPerDay - 0.5, "exact half-ms before midnight");
    const civil = { year, month, day, fraction };
    assert.equal(date.civilUtcToDate(civil).getTime(), new Date(next).getTime());
    assert.equal(date.civilAtOffsetToDate(civil, -60).getTime(), new Date(next).getTime() + 3_600_000);
  }
});

check("upper civil year rounding carry rejected before offset", () => {
  const civil = { year: 32767, month: 12, day: 31, fraction: (millisecondsPerDay - 0.5) / millisecondsPerDay };
  assert.throws(() => date.civilUtcToDate(civil), RangeError);
  for (const offset of [-1439, 0, 1439]) {
    assert.throws(() => date.civilAtOffsetToDate(civil, offset), RangeError);
  }
  const last = { ...civil, fraction: (millisecondsPerDay - 1) / millisecondsPerDay };
  assert.equal(date.dateToCivilUtc(date.civilUtcToDate(last)).year, 32767);
});

check("UTC carrier years 0 and 32768 at local domain edges", () => {
  for (const [civil, offset, iso] of [
    [{ year: 1, month: 1, day: 1, fraction: 0 }, 1439, "0000-12-31T00:01:00.000Z"],
    [{ year: 32767, month: 12, day: 31, fraction: (millisecondsPerDay - 1) / millisecondsPerDay }, -1439,
      "+032768-01-01T23:58:59.999Z"],
  ]) {
    const carrier = date.civilAtOffsetToDate(civil, offset);
    assert.equal(carrier.toISOString(), iso);
    const local = date.dateToCivilAtOffset(carrier, offset);
    assert.deepEqual({ year: local.year, month: local.month, day: local.day, fraction: local.fraction }, civil);
    assert.equal(date.civilAtOffsetToDate(local, offset).getTime(), carrier.getTime());
    assert.throws(() => date.dateToCivilUtc(carrier), RangeError, "UTC output year remains outside the domain");
    assert.throws(() => date.dateToCivilAtOffset(carrier, 0), RangeError);
  }
  assert.throws(() => date.dateToCivilAtOffset(new Date("0001-01-01T00:00:00.000Z"), -1), RangeError);
  assert.throws(() => date.dateToCivilAtOffset(new Date("+032767-12-31T23:59:59.999Z"), 1), RangeError);
});

const validCivil = { year: 2024, month: 2, day: 29, fraction: 0.5 };
const validDate = new Date("2024-02-29T12:00:00.000Z");
check("civil records use explicit fields", () => {
  for (const input of [new Date(0), new Date(NaN), Object.create(null)]) {
    Object.assign(input, validCivil, {
      getTime: () => assert.fail("civil conversion must not read a Date timestamp"),
    });
    assert.equal(date.civilUtcToDate(input).getTime(), validDate.getTime());
    assert.equal(date.civilAtOffsetToDate(input, 480).getTime(), validDate.getTime() - 28_800_000);
  }
});
check("invalid civil records", () => {
  const cases = [
    [null, TypeError], [[], TypeError], [new Date(), TypeError], [{}, TypeError],
    [{ year: 2024, month: 2, day: 29 }, TypeError],
    [{ ...validCivil, year: "2024" }, TypeError], [{ ...validCivil, year: 1.5 }, TypeError],
    [{ ...validCivil, year: 0 }, RangeError], [{ ...validCivil, year: -1 }, RangeError],
    [{ ...validCivil, year: 32768 }, RangeError], [{ ...validCivil, year: Number.MAX_SAFE_INTEGER + 1 }, RangeError],
    [{ ...validCivil, year: 1900 }, RangeError], [{ ...validCivil, year: 2023 }, RangeError],
    [{ ...validCivil, month: 0 }, RangeError], [{ ...validCivil, month: 13 }, RangeError],
    [{ ...validCivil, month: 4, day: 31 }, RangeError], [{ ...validCivil, day: 0 }, RangeError],
    [{ ...validCivil, day: 30 }, RangeError], [{ ...validCivil, day: 1.5 }, TypeError],
    [{ ...validCivil, fraction: undefined }, TypeError], [{ ...validCivil, fraction: "0" }, TypeError],
    [{ ...validCivil, fraction: -Number.EPSILON }, RangeError], [{ ...validCivil, fraction: 1 }, RangeError],
    [{ ...validCivil, fraction: Number.NaN }, RangeError], [{ ...validCivil, fraction: Infinity }, RangeError],
    [Object.create(validCivil), TypeError],
  ];
  for (const [civil, ErrorType] of cases) {
    assert.throws(() => date.civilUtcToDate(civil), ErrorType);
    assert.throws(() => date.civilAtOffsetToDate(civil, 480), ErrorType);
  }
  for (const tag of [false, undefined]) {
    for (const civil of [
      { ...validCivil, isLeap: tag },
      Object.assign(Object.create({ isLeap: tag }), validCivil),
    ]) {
      assert.throws(() => date.civilUtcToDate(civil), TypeError, "semantic tag present, even inherited or undefined");
      assert.throws(() => date.civilAtOffsetToDate(civil, 480), TypeError);
    }
  }
  for (const extra of [{ source: "extra field" }, { hour: 0, minute: 0, second: 0 }]) {
    assert.equal(
      date.civilUtcToDate({ ...validCivil, ...extra }).getTime(),
      validDate.getTime(),
      "fraction is authoritative",
    );
    assert.equal(
      date.civilAtOffsetToDate({ ...validCivil, ...extra }, 480).getTime(),
      validDate.getTime() - 28_800_000,
    );
  }
});

check("invalid Date and out-of-domain UTC years", () => {
  for (const value of [null, {}, "2024-02-29", 0, { getTime: () => 0 }]) {
    assert.throws(() => date.dateToCivilUtc(value), TypeError);
    assert.throws(() => date.dateToCivilAtOffset(value, 480), TypeError);
  }
  for (const value of [
    new Date(Number.NaN), new Date(Infinity),
    new Date("0000-06-15T00:00:00.000Z"), new Date("-000001-06-15T00:00:00.000Z"),
    new Date("+032768-06-15T00:00:00.000Z"), new Date(8_640_000_000_000_000),
  ]) {
    assert.throws(() => date.dateToCivilUtc(value), RangeError);
    assert.throws(() => date.dateToCivilAtOffset(value, 480), RangeError);
  }
});

check("offset endpoints and rejections", () => {
  for (const offset of [-1439, 1439]) {
    assert.equal(
      date.civilAtOffsetToDate(date.dateToCivilAtOffset(validDate, offset), offset).getTime(),
      validDate.getTime(),
    );
  }
  for (const [offset, ErrorType] of [
    [-1440, RangeError], [1440, RangeError], [-1439.5, TypeError], [1439.5, TypeError],
    [Number.MAX_SAFE_INTEGER + 1, RangeError], [Number.NaN, RangeError], [Infinity, RangeError],
    ["480", TypeError], [true, TypeError], [null, TypeError], [undefined, TypeError],
  ]) {
    assert.throws(() => date.dateToCivilAtOffset(validDate, offset), ErrorType);
    assert.throws(() => date.civilAtOffsetToDate(validCivil, offset), ErrorType);
  }
});

assert.equal(directed, 17, "directed date case groups");
console.log(`PASS directed date groups ${directed}/17 in ${((performance.now() - started) / 1000).toFixed(3)} s`);

if (exhaustive) {
  const exhaustiveStarted = performance.now();
  const start = new Date("2024-02-29T00:00:00.000Z").getTime();
  let checked = 0;
  for (let milliseconds = 0; milliseconds < millisecondsPerDay; ++milliseconds) {
    const instant = new Date(start + milliseconds);
    const civil = date.dateToCivilUtc(instant);
    const returned = date.civilUtcToDate(civil);
    assert.equal(returned.getTime(), instant.getTime(), `public round trip at millisecond ${milliseconds}`);
    assert.equal(civil.year, 2024);
    assert.equal(civil.month, 2);
    assert.equal(civil.day, 29);
    assert.equal(civil.fraction, milliseconds / millisecondsPerDay);
    assert.equal(civil.hour, Math.floor(milliseconds / 3_600_000));
    assert.equal(civil.minute, Math.floor(milliseconds / 60_000) % 60);
    assert.equal(Math.round(civil.second * 1000), milliseconds % 60_000);
    ++checked;
  }
  assert.equal(checked, 86_400_000, "every millisecond position of the representative UTC day");
  console.log(
    `PASS exhaustive public Date -> civil -> Date ${checked}/86400000 in ` +
    `${((performance.now() - exhaustiveStarted) / 1000).toFixed(3)} s`,
  );
} else {
  console.log("SKIP exhaustive millisecond sweep (enable --exhaustive)");
}
