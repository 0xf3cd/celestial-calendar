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

export function runObserverAcceptance(celestial) {
  const check = (value, message) => { if (!value) throw new Error(message); };
  const reject = (action) => {
    let rejected = false;
    try { action(); } catch (error) { rejected = error instanceof RangeError; }
    check(rejected, "observer invalid input was not a RangeError");
  };
  const civil = { year: 2026, month: 11, day: 7, fraction: 0 };
  const options = { scale: "utc", observer: { latitudeDeg: -33.85, longitudeDeg: 151.2, elevationM: 50 } };
  // C-export replay from 51884b0, collected 2026-11-07: the civil/site above with 1706 m elevation.
  // Bounds cover native/WASM replay, not physical accuracy: 1e-10 deg, 1e-14 AU; times are exact.
  // Columns: target, RA deg, Dec deg, distance AU, geometric altitude deg, north-zero/east-positive azimuth deg.
  const references = [
    ["sun",     222.12615856751188, -16.213109030796183,   0.9911635565669776,  61.63759904136321,  57.67774600838513],
    ["moon",    195.03105800858756, -10.300589822905488, 0.002570966784869903,  66.34950329910117,  354.1481686728121],
    ["jupiter",  147.5358674521412,  13.854651842844639,    5.407887555564511, 22.720528757348692, 306.40109412671984],
  ];
  const elevated = { ...options, observer: { ...options.observer, elevationM: 1706 }, model: "algo5" };
  const fields = ["rightAscensionDeg", "declinationDeg", "distanceAu", "altitudeDeg", "azimuthDeg"];
  const tolerances = [1e-10, 1e-10, 1e-14, 1e-10, 1e-10];
  for (const [target, ...expected] of references) {
    const result = celestial.observer.position(target, civil, elevated);
    check(result.times.jdUt1 === 2461351.4999999567 && result.times.jdeTt === 2461351.500800741,
      "observer native replay times");
    for (const [index, field] of fields.entries()) {
      check(Math.abs(result[field] - expected[index]) <= tolerances[index], `observer native replay ${target}.${field}`);
    }
  }
  for (const [model, expected] of [["bennett", 66.35608979437563], ["saemundsson", 66.35621056297853]]) {
    const result = celestial.observer.position("moon", civil, {
      ...elevated, refraction: { model, temperatureC: 5, pressureHpa: 900 },
    });
    check(Math.abs(result.altitudeDeg - references[1][4]) <= 1e-10, "observer refraction preserves geometry");
    check(result.apparentAltitudeDeg !== null && Math.abs(result.apparentAltitudeDeg - expected) <= 1e-10,
      `observer native replay ${model} apparent altitude`);
  }
  const selected = celestial.observer.position("moon", civil, { ...elevated, scale: "ut1", model: "algo2" });
  const defaultModel = celestial.observer.position("moon", civil, { ...elevated, scale: "ut1", model: "default" });
  check(selected.deltaTModel === "algo2", "observer explicit ALGO2 identity");
  check(selected.times.jdeTt !== defaultModel.times.jdeTt, "observer explicit model changes resolved TT");
  const targets = ["sun", "moon", "mercury", "venus", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto"];
  for (const target of targets) {
    const result = celestial.observer.position(target, civil, options);
    check(result.target === target && result.distanceAu > 0, "observer target/distance");
    check(result.azimuthDeg !== null && result.rightAscensionDeg !== null && result.apparentAltitudeDeg === null,
      "observer ordinary presence");
    check(result.times.jdeTt === celestial.time.utcToJde(civil), "observer strict UTC composition");
    check(result.deltaTModel === "algo5", "observer default identity");
  }
  for (const latitude of [-90, 90]) {
    const result = celestial.observer.position("moon", civil, {
      ...options, observer: { ...options.observer, latitudeDeg: latitude },
    });
    check(result.azimuthDeg === null && Number.isFinite(result.altitudeDeg), "observer exact pole");
  }
  const seen = new Set();
  for (let hour = 0; hour < 24; ++hour) {
    const result = celestial.observer.position("sun", { ...civil, fraction: hour / 24 }, {
      ...options, refraction: { model: "bennett" },
    });
    seen.add(result.apparentAltitudeDeg === null);
  }
  check(seen.size === 2, "observer supported/unsupported refraction altitudes");
  reject(() => celestial.observer.position("sun", civil, { ...options, refraction: { model: "saemundsson", temperatureC: -91 } }));
  reject(() => celestial.observer.position("meanAscending", civil, options));
  reject(() => celestial.observer.position("sun", { year: 1971, month: 12, day: 31, fraction: 0 }, options));
  reject(() => celestial.observer.position("pluto", { year: 2100, month: 1, day: 1, fraction: 0 }, options));
  check(celestial.observer.position("sun", { year: 1000, month: 1, day: 1, fraction: 0 }, { ...options, scale: "ut1" }).distanceAu > 0,
    "observer must not inherit the chart civil window");
  return { targets: targets.length, exactPoles: 2, refractionStates: seen.size };
}
