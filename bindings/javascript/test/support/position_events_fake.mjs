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

export function testPositionEventsFakeNative(fixture, M, control, reset, called, layouts) {
  const start = 2451545;
  const end = start + 1;
  const position = { target_code: 0, present_fields: 3, longitude_deg: 120, latitude_deg: 1, distance_au: 2, source_code: 1 };
  const equatorial = { target_code: 0, present_fields: 3, right_ascension_deg: 121, declination_deg: 2, distance_au: 2, source_code: 1 };
  const longitude = { jde_tt: start + 0.5, longitude_deg: -361, longitude_rate_deg_per_tt_day: 0.5, direction_code: 0 };
  const station = { jde_tt: start + 0.5, longitude_deg: 721, longitude_rate_deg_per_tt_day: 1e-12,
    motion_before_code: 0, motion_after_code: 1 };
  const difference = { jde_tt: start + 0.5, longitude_a_deg: -721, longitude_b_deg: 720,
    longitude_rate_a_deg_per_tt_day: 1, longitude_rate_b_deg_per_tt_day: -1, signed_difference_deg: 60, direction_code: 2 };
  const cases = [
    ["celestial_positions", "CelestialPositionRecord", () => fixture.position.ecliptic(["sun"], start), position, 3],
    ["celestial_equatorial_positions", "CelestialEquatorialPositionRecord", () => fixture.position.equatorial(["sun"], start), equatorial, 3],
    ["celestial_longitude_events", "CelestialLongitudeEventRecord", () => fixture.events.longitudeCrossings("sun", 720, start, end), longitude, 2],
    ["celestial_longitude_stations", "CelestialStationEventRecord", () => fixture.events.longitudeStations("sun", start, end), station, 2],
    ["celestial_longitude_difference_events", "CelestialLongitudeDifferenceEventRecord",
      () => fixture.events.longitudeDifferences("moon", "sun", 60, start, end), difference, 2],
  ];
  let rows;
  const configure = (records, changes = {}) => {
    rows = records;
    reset({ count: records.length, total: records.length, fillWritten: records.length,
      omit: undefined, omitCount: undefined, guardAbsentDistance: false, ...changes });
  };
  for (const [name, layoutName] of cases) {
    const layout = layouts[layoutName];
    M[`_${name}`] = (...args) => {
      const [count, slots, capacity] = args.slice(-3);
      if (!called(name, args.slice(0, -3), slots ? "fill" : "count")) { M.HEAPU32[count >> 2] = 0; return 0; }
      if (name.endsWith("positions")) {
        assert.equal(args[1], rows.length || 1, "literal selection count");
        assert.equal(args[2], start, "literal TT instant");
        if (slots) assert.equal(M.HEAPU32[args[0] >> 2], rows[0].target_code, "input retained across count/fill and growth");
      } else {
        const expected = name === "celestial_longitude_events" ? [0, 720, start, end]
          : name === "celestial_longitude_stations" ? [0, start, end] : [1, 0, 60, start, end];
        assert.deepEqual(args.slice(0, -3), expected, "literal event argument order");
      }
      if ((slots ? "fill" : "count") !== control.omitCount) {
        M.HEAPU32[count >> 2] = slots ? control.total : control.count;
      }
      if (!slots) return control.countWritten;
      rows.slice(0, capacity).forEach((record, index) => {
        for (const field of layout.fields) {
          if (control.omit === field.name) continue;
          const address = slots + index * layout.size + field.offset;
          if (field.type === "double") M.HEAPF64[address >> 3] = record[field.name];
          else M.HEAPU32[address >> 2] = record[field.name];
        }
      });
      if (control.guardAbsentDistance) {
        const field = layout.fields.find((field) => field.name === "distance_au");
        const forbidden = (slots + field.offset) >> 3;
        M.HEAPF64 = new Proxy(M.HEAPF64, {
          get(target, key) {
            assert.notEqual(String(key), String(forbidden), "absent distance storage must not be read");
            const value = Reflect.get(target, key, target);
            return typeof value === "function" ? value.bind(target) : value;
          },
        });
      }
      return control.fillWritten;
    };
  }
  const fails = (action, ErrorType = fixture.CelestialError, recorded = false) => {
    assert.throws(action, (error) => error instanceof ErrorType && (ErrorType !== fixture.CelestialError || error.recorded === recorded));
    assert.equal(control.live.size, 0, "position/event failure leaked memory");
  };
  let corruptions = 0;
  let missingCounts = 0;
  for (const [name, layoutName, action, clean, allocations] of cases) {
    configure([clean], { grow: true });
    const owned = action();
    assert(Object.isFrozen(owned) && Object.isFrozen(owned[0]));
    assert.equal(control.live.size, 0);
    assert(control.calls.every(([calledName]) => calledName === name), "query must not call chart, rates or other operations");
    const saved = JSON.stringify(owned);
    configure([clean]); action();
    assert.equal(JSON.stringify(owned), saved, "owned values survive reuse");
    for (let failure = 1; failure <= allocations; ++failure) {
      configure([clean], { failAllocation: failure }); fails(action);
    }
    for (const changes of [{ countWritten: 1 }, { total: 2 }, { fillWritten: 0 }, { count: 0xFFFFFFFF }]) {
      configure([clean], changes); fails(action);
    }
    for (const pass of ["count", "fill"]) {
      configure([clean], { omitCount: pass }); fails(action); ++missingCounts;
    }
    for (const pass of ["count", "fill"]) {
      for (const kind of [1, 2, 3, 4, 99]) {
        configure([clean], { failPass: pass, errorKind: kind, message: "" });
        fails(action, kind === 1 ? RangeError : fixture.CelestialError, kind !== 99);
      }
    }
    for (const field of layouts[layoutName].fields) {
      configure([{ ...clean, [field.name]: field.type === "double" ? NaN : 99 }]);
      if (field.name !== "target_code") fails(action);
      else {
        // Keep the admitted input valid while corrupting only the output target.
        const fn = M[`_${name}`];
        M[`_${name}`] = (...args) => {
          const result = fn(...args);
          if (args.at(-2)) M.HEAPU32[(args.at(-2) + field.offset) >> 2] = 99;
          return result;
        };
        configure([clean]); fails(action);
        M[`_${name}`] = fn;
      }
      ++corruptions;
      configure([clean], { omit: field.name }); fails(action); ++corruptions;
    }
    configure([]);
    if (name.endsWith("positions")) fails(action);
    else assert.deepEqual(action(), []);
  }
  for (const [name, layout, action, clean] of cases.slice(0, 2)) {
    for (const absent of [NaN, Infinity, -123, 0]) {
      configure([{ ...clean, target_code: 10, present_fields: 1, source_code: 5, latitude_deg: 0, distance_au: absent }]);
      const values = fixture.position[name === "celestial_positions" ? "ecliptic" : "equatorial"](["meanAscending"], start);
      assert.equal(values[0].distanceAu, null);
      assert.equal(control.live.size, 0);
    }
    const heap = M.HEAPF64;
    try {
      configure([{ ...clean, target_code: 10, present_fields: 1, source_code: 5, latitude_deg: 0, distance_au: NaN }],
        { guardAbsentDistance: true });
      assert.equal(fixture.position[name === "celestial_positions" ? "ecliptic" : "equatorial"](["meanAscending"], start)[0].distanceAu, null);
    } finally { M.HEAPF64 = heap; }
    for (const change of [{ distance_au: 0 }, { present_fields: 1 }, { source_code: 2 }]) {
      configure([{ ...clean, ...change }]); fails(action); ++corruptions;
    }
    configure([clean], { total: 0 }); fails(action);
  }
  for (const [, , action, clean] of cases.slice(2)) {
    for (const time of [start - 1, end, end + 1]) {
      configure([{ ...clean, jde_tt: time }]); fails(action); ++corruptions;
    }
    for (const later of [clean.jde_tt, clean.jde_tt - 0.1]) {
      configure([clean, { ...clean, jde_tt: later }]); fails(action); ++corruptions;
    }
  }
  configure([difference]);
  assert.equal(cases[4][2]()[0].longitudeADeg, -721, "difference longitudes stay raw");
  configure([longitude]);
  assert.equal(cases[2][2]()[0].longitudeDeg, -361, "crossing longitude stays raw");
  configure([station]);
  assert.equal(cases[3][2]()[0].longitudeDeg, 721, "station longitude stays raw");
  reset();
  console.log(`PASS position/events fake native: ${corruptions} malformed controls; ${missingCounts} unwritten-count controls; 12 allocation paths; errors; growth; ownership; raw longitudes`);
}
