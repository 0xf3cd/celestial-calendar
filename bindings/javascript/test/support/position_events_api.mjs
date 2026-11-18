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

const TARGETS = ["sun", "moon", "mercury", "venus", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto",
  "meanAscending", "meanDescending", "trueAscending", "trueDescending"];
const DIRECTIONS = ["increasing", "decreasing", "tangent"];
const MOTIONS = ["direct", "retrograde"];
const fromBits = (bits) => {
  const view = new DataView(new ArrayBuffer(8));
  view.setBigUint64(0, BigInt(bits), true);
  return view.getFloat64(0, true);
};
const check = (condition, message) => { if (!condition) throw new Error(`position/events: ${message}`); };
const reject = (action, ErrorType) => {
  try { action(); } catch (error) {
    check(error instanceof ErrorType, `expected ${ErrorType.name}, received ${error}`);
    return;
  }
  throw new Error(`position/events: expected ${ErrorType.name}`);
};

export function runPositionEventsAcceptance(celestial, golden) {
  let maximumDifference = 0;
  let compared = 0;
  for (const entry of golden.cases) {
    const [namespace, method] = entry.operation.split(".");
    const target = (code) => TARGETS[code] ?? "unknown";
    const args = entry.targets ? [entry.targets.map(target), entry.jde_tt]
      : entry.arguments.map((value, index) => index < (method === "longitudeDifferences" ? 2 : 1) ? target(value) : value);
    const action = () => celestial[namespace][method](...args);
    if (entry.error) {
      reject(action, entry.error === 1 ? RangeError : celestial.CelestialError);
      continue;
    }
    const result = action();
    check(Object.isFrozen(result) && result.length === entry.count, `${entry.name} count/array ownership`);
    result.forEach((record, index) => {
      check(Object.isFrozen(record), `${entry.name} record ownership`);
      const expected = entry.records[index];
      const keys = [];
      for (const [name, value] of Object.entries(expected)) {
        if (name === "present_fields") continue;
        const key = name === "target_code" ? "target" : name === "source_code" ? "sourceCode"
          : name === "direction_code" ? "direction" : name.replace(/_([a-z])/g, (_, letter) => letter.toUpperCase()).replace(/Code$/, "");
        keys.push(key);
        if (value === null) check(record[key] === null, `${entry.name} ${key} absent`);
        else if (typeof value === "string") {
          const gap = Math.abs(record[key] - fromBits(value));
          const tolerance = name === "jde_tt" ? 1e-8 : 1e-9;
          check(Number.isFinite(gap) && gap <= tolerance, `${entry.name} ${key} parity: ${gap}`);
          maximumDifference = Math.max(maximumDifference, gap);
          ++compared;
        } else {
          const decoded = name === "target_code" ? target(value) : name === "direction_code" ? DIRECTIONS[value]
            : name.startsWith("motion_") ? MOTIONS[value] : value;
          check(record[key] === decoded, `${entry.name} ${key} exact value`);
        }
      }
      check(Object.keys(record).sort().join() === keys.sort().join(), `${entry.name} result fields`);
    });
  }

  const { position, events } = celestial;
  check(Object.isFrozen(position) && Object.isFrozen(events), "frozen domains");
  const selection = ["mars", "trueAscending", "sun"];
  const owned = position.ecliptic(selection, 2451545);
  const saved = JSON.stringify(owned);
  selection.reverse();
  position.ecliptic(TARGETS, 2451546);
  position.equatorial(["moon"], 2451545);
  check(JSON.stringify(owned) === saved && owned[1].distanceAu === null, "selected input and result ownership");
  const indexed = ["sun"];
  indexed[Symbol.iterator] = () => { throw new Error("selection must not invoke an unbounded custom iterator"); };
  check(position.ecliptic(indexed, 2451545)[0].target === "sun", "bounded indexed selection");
  const sub = position.ecliptic(["sun", "mars"], 2350000);
  check(sub.length === 2, "position independent of chart joint window");
  reject(() => celestial.chart.snapshot({ year: 1737, month: 1, day: 1, fraction: 0 }, {
    scale: "ut1", location: { latitudeDeg: 0, longitudeDeg: 0 }, system: "equal",
  }), RangeError);
  const start = 2460735.5;
  const end = 2460785.5;
  const crossings = events.longitudeCrossings("mercury", 0, start, end);
  check(crossings.length === 3 && new Set(crossings.map((event) => event.direction)).size === 2, "direct/retrograde crossings");
  check(JSON.stringify(events.longitudeCrossings("mercury", 720, start, end)) === JSON.stringify(crossings),
    "whole-turn query preserves returned longitudes");
  const time = crossings[0].jdeTt;
  check(events.longitudeCrossings("mercury", 0, start, time).length === 0, "end excludes event");
  check(events.longitudeCrossings("mercury", 0, time, end)[0].jdeTt === time, "start includes event");
  const stations = events.longitudeStations("mercury", start, end);
  check(stations.length === 2, "both stations");
  const tangent = stations.some((station) => events.longitudeCrossings("mercury", station.longitudeDeg, start, end)
    .some((event) => event.direction === "tangent" && Math.abs(event.jdeTt - station.jdeTt) < 1e-8));
  check(tangent, "exact numerical tangent crossing");
  const forward = events.longitudeDifferences("moon", "sun", 60, 2460676.5, 2460707.5);
  const reverse = events.longitudeDifferences("sun", "moon", -60, 2460676.5, 2460707.5);
  check(forward.length === reverse.length && forward.length > 0, "reverse pair count");
  forward.forEach((event, index) => {
    const other = reverse[index];
    check(event.jdeTt === other.jdeTt && event.signedDifferenceDeg === -other.signedDifferenceDeg
      && event.longitudeADeg === other.longitudeBDeg && event.longitudeBDeg === other.longitudeADeg
      && event.direction !== other.direction, "directed pair reversal");
  });
  const retained = JSON.stringify(crossings);
  events.longitudeStations("venus", start, end);
  check(JSON.stringify(crossings) === retained, "event ownership after another query");
  for (const input of [null, new Set(["sun"]), "sun", [0], new Array(1)]) {
    reject(() => position.ecliptic(input, 2451545), TypeError);
  }
  for (const input of [[], ["sun", "sun"], ["unknown"]]) reject(() => position.equatorial(input, 2451545), RangeError);
  for (const invalid of [NaN, Infinity, -Infinity]) {
    reject(() => position.ecliptic(["sun"], invalid), RangeError);
    reject(() => events.longitudeCrossings("sun", invalid, start, end), RangeError);
    reject(() => events.longitudeStations("sun", invalid, end), RangeError);
    reject(() => events.longitudeDifferences("sun", "moon", 0, start, invalid), RangeError);
  }
  reject(() => position.equatorial(["sun"], "2451545"), TypeError);
  reject(() => events.longitudeStations("unknown", start, end), RangeError);
  reject(() => events.longitudeDifferences("sun", "moon", "0", start, end), TypeError);
  return { cases: golden.cases.length, compared, maximumDifference, endpointChecks: 2, tangent };
}
