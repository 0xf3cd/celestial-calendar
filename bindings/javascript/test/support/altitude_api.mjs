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

export function runAltitudeAcceptance(celestial) {
  const check = (value, message) => { if (!value) throw new Error(message); };
  const reject = (action, ErrorType, recorded) => {
    let caught = false;
    try { action(); } catch (error) {
      caught = error instanceof ErrorType && (recorded === undefined || error.recorded === recorded);
    }
    check(caught, "altitude failure type/recorded contract");
  };
  const site = { latitudeDeg: 0, longitudeDeg: -77, elevationM: 0 };
  const condition = { target: "sun", level: { kind: "geometric", altitudeDeg: 0 }, sense: "above" };
  const start = 2451545;
  const end = 2451546;
  const targets = ["sun", "moon", "mercury", "venus", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto"];
  for (const target of targets) {
    const query = { ...condition, target, level: { kind: "geometric", altitudeDeg: 10 } };
    const result = celestial.observer.altitudeEvents({ ...site, latitudeDeg: 90 }, query, start, start + 0.25);
    check(result.condition.condition.target === target && result.condition.geometricLevelDeg === 10, "altitude physical identity");
    check(result.condition.deltaTModel === "algo5" && typeof result.condition.initiallySatisfied === "boolean", "altitude state/clock");
    check(Object.isFrozen(result) && Object.isFrozen(result.condition) && Object.isFrozen(result.events), "altitude owned result");
    query.level.altitudeDeg = 20;
    check(result.condition.condition.level.altitudeDeg === 10, "altitude result aliased mutable input");
  }
  const result = celestial.observer.altitudeEvents(site, condition, start, end);
  check(result.condition.initiallySatisfied && result.events.length === 2, "Sun initial state/census");
  check(result.events[0].direction === "decreasing" && result.events[1].direction === "increasing", "Sun event directions");
  const split = celestial.observer.altitudeEvents(site, condition, result.events[0].jdeTt, end);
  check(split.events[0].jdeTt === result.events[0].jdeTt && !split.condition.initiallySatisfied, "inclusive start replay");
  const windows = celestial.observer.observingWindows(site, [condition, condition], start, end);
  check(windows.conditions.length === 2 && windows.events.length === 4 && windows.windows.length === 2, "equal-time duplicate stream");
  check(windows.events[0].conditionIndex === 0 && windows.events[1].conditionIndex === 1, "equal-time association order");
  check(celestial.observer.observingWindows(site, [condition, { ...condition, sense: "below" }], start, end).windows.length === 0,
    "opposing equal-time conditions");
  const empty = celestial.observer.observingWindows(site, [], 13689325.25, 13689325.5);
  check(empty.windows.length === 1 && empty.windows[0].endJdeTt === 13689325.5, "empty conjunction/exclusive calendar end");
  for (const model of ["bennett", "saemundsson"]) {
    const refracted = { ...condition, level: { kind: "refracted", altitudeDeg: 10, refraction: { model } } };
    const state = celestial.observer.altitudeEvents({ ...site, latitudeDeg: 90 }, refracted, start, start + 0.25).condition;
    check(state.geometricLevelDeg > 9 && state.geometricLevelDeg < 10, "refracted level inverse");
    check(state.condition.level.refraction.temperatureC === 15 && state.condition.level.refraction.pressureHpa === 1013.25,
      "explicit resolved weather defaults");
  }
  const transits = celestial.observer.upperTransits(site, "mars", start, end);
  check(transits.length === 1 && transits[0].target === "mars" && transits[0].sourceCode === 3, "distinct transit metadata");
  const unresolved = { ...condition, target: "pluto", level: { kind: "geometric", altitudeDeg: 48.605252016244656 } };
  const criticalSite = { ...site, latitudeDeg: 30 };
  reject(() => celestial.observer.altitudeEvents(criticalSite, unresolved, start, start + 0.25), celestial.CelestialError, true);
  reject(() => celestial.observer.observingWindows(criticalSite, [condition, unresolved], start, start + 0.25), celestial.CelestialError, true);
  reject(() => celestial.observer.upperTransits(site, "meanAscending", start, end), RangeError);
  reject(() => celestial.observer.upperTransits(site, "pluto", 2488069.5, 2488069.75), RangeError);
  reject(() => celestial.observer.altitudeEvents(site, { ...condition, level: { ...condition.level, refraction: {} } }, start, end), TypeError);
  return { targets: targets.length, refractionModels: 2, unresolved: 2 };
}
