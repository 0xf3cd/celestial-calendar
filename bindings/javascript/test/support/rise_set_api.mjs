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

export function runRiseSetAcceptance(celestial, golden) {
  const check = (value, message) => { if (!value) throw new Error(message); };
  const targets = ["sun", "moon", "mercury", "venus", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto"];
  const sources = [1, 2, 3, 3, 3, 3, 3, 3, 3, 4];
  const bits = (hex) => {
    const bytes = new ArrayBuffer(8);
    new BigUint64Array(bytes)[0] = BigInt(hex);
    return new Float64Array(bytes)[0];
  };
  check(golden.schema === "celestial-calendar/rise-set-parity@1" && golden.entries.length === 360, "daily parity fixture");
  let maximumMomentDifferenceDays = 0;
  for (const entry of golden.entries) {
    const [year, month, day, latitudeDeg, longitudeDeg, elevationM] = golden.days[entry.day_index];
    const date = { year, month, day };
    const location = { latitudeDeg, longitudeDeg };
    const target = targets[entry.target];
    const weather = entry.custom
      ? { model: "saemundsson", temperatureC: -20, pressureHpa: 800 } : { model: "bennett" };
    const h0Deg = entry.custom ? (entry.target === 0 ? -18 : 0) : (entry.target === 0 ? -50 / 60 : -34 / 60);
    const query = () => entry.target === 0 ? celestial.riseSet.solar(date, location, { h0Deg })
      : entry.target === 1 ? celestial.riseSet.lunar(date, location, { refraction: weather })
        : celestial.riseSet.planet(target, date, { ...location, elevationM }, { h0Deg });
    if (entry.error) {
      let rejected = false;
      try { query(); } catch (error) { rejected = error instanceof RangeError; }
      check(rejected, "daily provider/year rejection");
      continue;
    }
    const result = query();
    check(result.target === target && result.sourceCode === sources[entry.target], "daily target/source");
    check(result.deltaTModel === "algo5" && result.nutationModel === "iau1980", "daily fixed models");
    check(result.polar === ["none", "day", "night"][entry.fields.polar_code], "daily polar topology");
    check(Object.isFrozen(result), "daily owned/frozen result");
    if (entry.target === 1) {
      check(result.refraction.model === weather.model && Object.isFrozen(result.refraction), "daily atmosphere ownership");
    } else check(result.h0Deg === h0Deg, "daily h0 forwarding");
    for (const [publicName, field, bit] of [
      ["riseJdeTt", "rise_jde_tt", 32], ["transitJdeTt", "transit_jde_tt", 64], ["setJdeTt", "set_jde_tt", 128],
    ]) {
      const present = Boolean(entry.fields.present_fields & bit);
      check((result[publicName] !== null) === present, `daily ${field} presence`);
      if (present) {
        const difference = Math.abs(result[publicName] - bits(entry.fields[field]));
        maximumMomentDifferenceDays = Math.max(difference, maximumMomentDifferenceDays);
        // Same native-vs-musl moment parity cap as the existing raw binding tests.
        check(difference <= 1e-8, `daily ${field} parity: ${difference}`);
      } else check(bits(entry.fields[field]) === 0, "daily canonical absent storage");
    }
  }
  const date = { year: 2025, month: 1, day: 1 };
  const cold = celestial.riseSet.lunar(date, { latitudeDeg: 40, longitudeDeg: 0 }, {
    refraction: { model: "bennett", temperatureC: -100, pressureHpa: 200 },
  });
  check(cold.refraction.temperatureC === -100, "lunar horizon weather domain");
  return { targets: targets.length, cases: golden.entries.length, maximumMomentDifferenceDays };
}
