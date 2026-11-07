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
