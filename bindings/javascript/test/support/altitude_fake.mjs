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

import assert from "node:assert/strict";

export function testAltitudeFakeNative(fixture, M, control, reset, called, layouts) {
  const site = { latitudeDeg: 0, longitudeDeg: -77, elevationM: 0 };
  const condition = { target: "sun", level: { kind: "geometric", altitudeDeg: 0 }, sense: "above" };
  const start = 2451545;
  const end = 2451546;
  const state = { tag: 0, position_source_code: 1, delta_t_model_code: 5, sense_code: 1, initially_satisfied: 1 };
  const events = [{ tag: 1, jde_tt: start + 0.25, direction_code: 1 }, { tag: 1, jde_tt: start + 0.75 }];
  const transit = { tag: 3, position_source_code: 1, delta_t_model_code: 5, jde_tt: start + 0.5 };
  const windows = [{ tag: 2, jde_tt: start, end_jde_tt: start + 0.25 }, { tag: 2, jde_tt: start + 0.75, end_jde_tt: end }];
  const cases = [
    ["celestial_altitude_events", () => fixture.observer.altitudeEvents(site, condition, start, end), [state, ...events], 3],
    ["celestial_upper_transits", () => fixture.observer.upperTransits(site, "sun", start, end), [transit], 2],
    ["celestial_observing_windows", () => fixture.observer.observingWindows(site, [condition], start, end), [state, ...events, ...windows], 3],
  ];
  const layout = layouts.CelestialAltitudeRecord;
  const set = (ptr, record) => {
    for (const field of layout.fields) {
      const address = ptr + field.offset;
      if (field.type === "double") M.HEAPF64[address >> 3] = record[field.name] ?? 0;
      else M.HEAPU32[address >> 2] = record[field.name] ?? 0;
    }
  };
  let records;
  const configure = (rows, changes = {}) => {
    records = rows;
    reset({ count: rows.length, total: rows.length, fillWritten: rows.length, ...changes });
  };
  for (const [name] of cases) {
    M[`_${name}`] = (...args) => {
      const [countPtr, slots, capacity] = args.slice(-3);
      const pass = slots ? "fill" : "count";
      if (!called(name, args.slice(0, -3), pass)) { M.HEAPU32[countPtr >> 2] = 0; return 0; }
      if (name !== "celestial_upper_transits") {
        assert.equal(layouts.CelestialAltitudeCondition.size, 40);
        const input = args[0];
        assert.deepEqual(Array.from(M.HEAPU32.subarray(input >> 2, (input >> 2) + 4)), [0, 1, 0, 0], "literal condition code order");
        assert.deepEqual(Array.from(M.HEAPF64.subarray((input + 16) >> 3, (input + 40) >> 3)), [0, 0, 0], "geometric unused weather");
      }
      assert.deepEqual(args.slice(-8, -3), [0, -77, 0, start, end], "literal site/time order");
      M.HEAPU32[countPtr >> 2] = slots ? control.total : control.count;
      if (!slots) return control.countWritten;
      records.slice(0, capacity).forEach((record, index) => set(slots + index * layout.size, record));
      return control.fillWritten;
    };
  }
  const fails = (action, recorded = false, ErrorType = fixture.CelestialError) => {
    assert.throws(action, (error) => error instanceof ErrorType && (ErrorType !== fixture.CelestialError || error.recorded === recorded));
    assert.equal(control.live.size, 0, "altitude failure leaked buffers");
  };
  let corruptions = 0;
  for (const [name, action, rows, allocationCount] of cases) {
    configure(rows, { grow: true });
    const owned = action();
    assert(Object.isFrozen(owned));
    assert.equal(control.live.size, 0, "altitude success leaked buffers");
    const before = JSON.stringify(owned);
    configure(rows); action();
    assert.equal(JSON.stringify(owned), before, "altitude result aliases reusable WASM memory");
    for (let failed = 1; failed <= allocationCount; ++failed) {
      configure(rows, { failAllocation: failed }); fails(action);
    }
    for (const changes of [{ countWritten: 1 }, { total: rows.length + 1 }, { fillWritten: rows.length - 1 }, { count: 1000000 }]) {
      configure(rows, changes); fails(action);
    }
    for (const pass of ["count", "fill"]) {
      for (const kind of [1, 2, 3, 4, 99]) {
        configure(rows, { failPass: pass, errorKind: kind, message: "" });
        fails(action, kind !== 99, kind === 1 ? RangeError : fixture.CelestialError);
      }
    }
    for (let index = 0; index < rows.length; ++index) {
      for (const field of layout.fields) {
        const changed = rows.map((row) => ({ ...row }));
        changed[index][field.name] = field.type === "double" ? NaN : 99;
        configure(changed); fails(action); ++corruptions;
      }
    }
    configure([]);
    if (name === "celestial_upper_transits") assert.deepEqual(action(), []);
    else fails(action);
  }
  for (const rows of [[state, ...events, windows[0]], [state, ...events, { ...windows[0], jde_tt: start + 0.1 }, windows[1]],
    [state, { ...events[0], jde_tt: end }, events[1], ...windows], [state, events[0], events[0], ...windows]]) {
    configure(rows); fails(cases[2][1]); ++corruptions;
  }
  configure([state, { tag: 1, direction_code: 2, jde_tt: start + 0.5 }, { tag: 2, jde_tt: start, end_jde_tt: end }]);
  const tangent = cases[2][1]();
  assert.equal(tangent.events[0].direction, "tangent");
  assert.equal(tangent.windows.length, 1);
  reset();
  for (const input of [null, { ...condition, level: 0 }, { ...condition, level: { ...condition.level, refraction: {} } },
    { ...condition, sense: 1 }, Object.create(condition)]) {
    assert.throws(() => fixture.observer.altitudeEvents(site, input, start, end), TypeError);
  }
  assert.throws(() => fixture.observer.observingWindows(site, new Array(1), start, end), TypeError);
  fails(() => fixture.observer.observingWindows(site, new Array(2049).fill(condition), start, end));
  assert.deepEqual(control.calls, [], "invalid shape must fail before native");
  console.log(`PASS altitude fake native: ${corruptions} malformed controls; 8 allocation paths; both error passes; growth; ownership`);
}
