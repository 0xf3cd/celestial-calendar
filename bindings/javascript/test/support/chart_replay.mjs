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

const check = (condition, label) => {
  if (!condition) throw new Error(label);
};

export const chartLabel = (name) => name.toLowerCase().replace(/_([a-z])/g, (_, letter) => letter.toUpperCase());

export const chartExampleInput = (example) => {
  const match = example.input.datetime.match(/^(\d+)-(\d+)-(\d+)T(\d+):(\d+):([\d.]+)$/);
  check(match !== null, "chart example civil format");
  const [year, month, day, hour, minute, second] = match.slice(1).map(Number);
  return {
    civil: { year, month, day, fraction: (hour * 3600 + minute * 60 + second) / 86400 },
    options: {
      scale: chartLabel(example.input.scale),
      location: { latitudeDeg: example.input.latitude_north_deg, longitudeDeg: example.input.longitude_east_deg },
      system: chartLabel(example.input.system),
      model: chartLabel(example.input.delta_t_model),
    },
  };
};

// Composition replay margins: bindings/python/test/consumer/chart_replay.py, not accuracy bounds.
export function assertChartReplay(actual, expected) {
  const residuals = {};
  const keys = (value, expectedKeys, label) => check(
    Object.keys(value).sort().join() === [...expectedKeys].sort().join(), `${label} fields`,
  );
  const close = (label, value, stored, margin, circular = false) => {
    check(Number.isFinite(value) && Number.isFinite(stored), `${label} non-finite`);
    const difference = value - stored;
    const residual = Math.abs(circular ? ((difference + 180) % 360 + 360) % 360 - 180 : difference);
    check(residual <= margin, `${label} replay residual ${residual} exceeds ${margin}`);
    residuals[label] = Math.max(residuals[label] ?? 0, residual);
  };
  keys(actual, ["times", "bodies", "houses"], "chart");
  keys(actual.times, ["jdUt1", "jdeTt"], "times");
  close("jdUt1", actual.times.jdUt1, expected.times.jd_ut1, 1e-9);
  close("jdeTt", actual.times.jdeTt, expected.times.jde_tt, 1e-9);
  check(actual.bodies.length === 14 && expected.bodies.length === 14, "chart body count");
  for (const [index, body] of actual.bodies.entries()) {
    const stored = expected.bodies[index];
    keys(body, ["target", "longitudeDeg", "latitudeDeg", "distanceAu", "longitudeRateDegPerTtDay"], `body${index}`);
    check(body.target === chartLabel(stored.target), `target identity at ${index}`);
    close("longitudeDeg", body.longitudeDeg, stored.longitude_deg, 1e-8, true);
    close("latitudeDeg", body.latitudeDeg, stored.latitude_deg, 1e-8);
    close("longitudeRateDegPerTtDay", body.longitudeRateDegPerTtDay, stored.longitude_rate_deg_per_tt_day, 1e-9);
    if (stored.distance_au === null) check(body.distanceAu === null, `node distance at ${index}`);
    else {
      check(body.distanceAu !== null, `body distance at ${index}`);
      close("distanceAu", body.distanceAu, stored.distance_au, 1e-10);
    }
  }
  keys(actual.houses, ["ascendantDeg", "midheavenDeg", "descendantDeg", "imumCoeliDeg", "cuspsDeg"], "houses");
  for (const name of ["ascendant", "midheaven", "descendant", "imumCoeli"]) {
    const storedName = name === "imumCoeli" ? "imum_coeli" : name;
    close(`${name}Deg`, actual.houses[`${name}Deg`], expected.houses[`${storedName}_deg`], 1e-5, true);
  }
  check(actual.houses.cuspsDeg.length === 12 && expected.houses.cusps_deg.length === 12, "chart cusp count");
  actual.houses.cuspsDeg.forEach((value, index) => close("cuspsDeg", value, expected.houses.cusps_deg[index], 1e-5, true));
  return residuals;
}

export function runChartAcceptance(celestial, example) {
  const { civil, options } = chartExampleInput(example);
  const first = celestial.chart.snapshot(civil, options);
  const residuals = assertChartReplay(first, example);
  const saved = JSON.stringify(first);
  const defaultOptions = { ...options };
  delete defaultOptions.model;
  check(JSON.stringify(celestial.chart.snapshot(civil, defaultOptions)) === saved, "chart default is Algo5");
  for (const model of ["algo1", "algo2", "algo3", "algo4", "algo5"]) {
    const result = celestial.chart.snapshot(civil, { ...options, scale: "ut1", model });
    const jd = celestial.time.ut1ToJd(civil);
    const start = celestial.time.ut1ToJd({ year: civil.year, month: 1, day: 1, fraction: 0 });
    const end = celestial.time.ut1ToJd({ year: civil.year + 1, month: 1, day: 1, fraction: 0 });
    const deltaT = celestial.time.deltaT(civil.year + (jd - start) / (end - start), model);
    check(result.times.jdUt1 === jd, `UT1 ingress ${model}`);
    check(Math.abs(result.times.jdeTt - (jd + deltaT / 86400)) <= 1e-9, `model dispatch ${model}`);
  }
  for (const system of ["equal", "wholeSign", "placidus"]) {
    for (const location of [options.location, { latitudeDeg: -33.87, longitudeDeg: 151.21 }]) {
      const result = celestial.chart.snapshot(civil, { ...options, system, location });
      check(JSON.stringify(result.bodies) === JSON.stringify(first.bodies), `house/body separation ${system}`);
      check(result.houses.cuspsDeg.length === 12, `house count ${system}`);
      if (system === "wholeSign") check(result.houses.cuspsDeg.every((cusp) => cusp % 30 === 0), "Whole Sign dispatch");
      if (system === "equal") check(result.houses.cuspsDeg[0] === result.houses.ascendantDeg, "Equal dispatch");
    }
  }
  const historical = { year: 1900, month: 6, day: 1, fraction: 0.5 };
  const past = celestial.chart.snapshot(historical, { ...options, scale: "ut1", system: "equal" });
  check(past.times.jdUt1 === celestial.time.ut1ToJd(historical), "historical UT1");
  for (const input of [
    { year: 1885, month: 1, day: 10, fraction: 0.5 },
    { year: 2099, month: 6, day: 1, fraction: 0.5 },
  ]) {
    const result = celestial.chart.snapshot(input, { ...options, scale: "ut1", system: "equal" });
    check(result.times.jdUt1 === celestial.time.ut1ToJd(input), `civil year boundary ${input.year}`);
  }
  celestial.chart.snapshot({ year: 1972, month: 1, day: 1, fraction: 0 }, options);
  for (const latitudeDeg of [-89.9, 89.9]) {
    for (const longitudeDeg of [-180, 180]) {
      celestial.chart.snapshot(civil, { ...options, system: "equal", location: { latitudeDeg, longitudeDeg } });
    }
  }
  const expectFailure = (action, ErrorType, recorded) => {
    try { action(); } catch (error) {
      check(error instanceof ErrorType, `chart error type: ${error}`);
      if (recorded !== undefined) check(error.recorded === recorded && error.operation === "chart.snapshot", "chart error context");
      return;
    }
    throw new Error("missing chart failure");
  };
  expectFailure(() => celestial.chart.snapshot(historical, options), RangeError);
  for (const year of [1884, 2100]) {
    expectFailure(() => celestial.chart.snapshot({ ...civil, year }, { ...options, scale: "ut1" }), RangeError);
  }
  expectFailure(() => celestial.chart.snapshot({ year: 1971, month: 12, day: 31, fraction: 0.5 }, options), RangeError);
  expectFailure(() => celestial.chart.snapshot(civil, { ...options, modle: "algo1" }), TypeError);
  expectFailure(() => celestial.chart.snapshot({ ...civil, year: 2035 }, { ...options, model: "algo4" }), celestial.CelestialError, true);
  expectFailure(() => celestial.chart.snapshot({ year: 1885, month: 1, day: 1, fraction: 0 }, { ...options, scale: "ut1" }), celestial.CelestialError, true);
  expectFailure(() => celestial.chart.snapshot(civil, { ...options, location: { latitudeDeg: 89, longitudeDeg: 0 } }), celestial.CelestialError, true);
  const recovered = celestial.chart.snapshot(civil, options);
  assertChartReplay(recovered, example);
  check(JSON.stringify(first) === saved, "later calls changed an owned chart");
  check(first !== recovered && first.times !== recovered.times && first.bodies !== recovered.bodies
    && first.bodies.every((body, index) => body !== recovered.bodies[index])
    && first.houses !== recovered.houses && first.houses.cuspsDeg !== recovered.houses.cuspsDeg, "chart ownership");
  first.bodies[2].longitudeDeg += 1;
  first.houses.cuspsDeg[0] += 1;
  assertChartReplay(recovered, example);
  return residuals;
}
