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

export function runDiameterAcceptance(celestial) {
  const check = (value, message) => { if (!value) throw new Error(message); };
  const reject = (action) => {
    let rejected = false;
    try { action(); } catch (error) { rejected = error instanceof RangeError; }
    check(rejected, "diameter invalid input was not a RangeError");
  };
  const civil = { year: 2026, month: 11, day: 7, fraction: 0 };
  const targets = ["sun", "moon", "mercury", "venus", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto"];
  // Independent IAU2015/JPL SSD facts, collected 2026-11-08; no production target/radius table is imported.
  const radii = [695700, 1737.4, 2440.53, 6051.8, 3396.19, 71492, 60268, 25559, 24764, 1188.3];
  const sources = [1, 2, 3, 3, 3, 3, 3, 3, 3, 4];
  for (const [index, target] of targets.entries()) {
    const geo = celestial.diameter.query(target, civil, { scale: "utc" });
    check(geo.target === target && geo.centre === "geocentric", "diameter request identity");
    check(geo.radiusKm === radii[index] && geo.sourceCode === sources[index], "diameter source radius/provider");
    check(geo.radiusDefinitionCode === (index === 0 ? 1 : index === 1 ? 2 : 3), "diameter radius convention");
    check(geo.deltaTModel === "algo5" && geo.times.jdeTt === celestial.time.utcToJde(civil), "diameter UTC/default identity");
    const r = radii[index] / 149597870.7;
    const expected = 2 * Math.atan2(r, Math.sqrt(geo.distanceAu - r) * Math.sqrt(geo.distanceAu + r)) * 180 / Math.PI * 3600;
    check(Math.abs(geo.diameterArcsec - expected) <= 1e-10, "diameter tangent geometry/units");
    check(Object.isFrozen(geo) && Object.isFrozen(geo.times), "diameter owned/frozen values");
    for (const latitudeDeg of [-90, -33.85, 90]) {
      const options = { scale: "utc", observer: { latitudeDeg, longitudeDeg: 151.2, elevationM: 1706 } };
      const topo = celestial.diameter.query(target, civil, options);
      check(topo.centre === "topocentric" && topo.distanceAu === celestial.observer.position(target, civil, options).distanceAu,
        "diameter observer forwarding/distance");
      check(topo.sourceCode === sources[index] && topo.radiusKm === radii[index], "diameter topocentric identities");
    }
  }
  const options = { scale: "ut1", observer: { latitudeDeg: -33.85, longitudeDeg: 151.2, elevationM: 1706 } };
  const selected = celestial.diameter.query("moon", civil, { ...options, model: "algo2" });
  check(selected.deltaTModel === "algo2" && selected.times.jdeTt !== celestial.diameter.query("moon", civil, options).times.jdeTt,
    "diameter explicit model forwarding");
  reject(() => celestial.diameter.query("meanAscending", civil, { scale: "utc" }));
  reject(() => celestial.diameter.query("pluto", { year: 2100, month: 1, day: 1, fraction: 0 }, { scale: "utc" }));
  reject(() => celestial.diameter.query("sun", { year: 2036, month: 1, day: 1, fraction: 0 }, { scale: "ut1", model: "algo4" }));
  check(celestial.diameter.query("sun", { year: 1000, month: 1, day: 1, fraction: 0 }, { scale: "ut1" }).diameterArcsec > 0,
    "diameter must not inherit chart window");
  check(celestial.diameter.query("moon", civil, { scale: "utc", observer: { latitudeDeg: 0, longitudeDeg: 0, elevationM: 1e303 } }).diameterArcsec > 0,
    "diameter huge accepted observer remains positive");
  return { targets: targets.length, exactPoles: 2 };
}
