<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v1.2.0]

Unreleased.

### Observer queries and events

- Query topocentric Sun, Moon and planet positions from a WGS84 site, including altitude, azimuth,
  distance and optional atmospheric refraction. See the [observer guide](OBSERVER.md).
- Get daily rise, upper transit and set in C++, C, Python and JavaScript/TypeScript. Solar queries
  are centred on a local transit; Moon and planet queries select events by UT1 date. Returned event times are TT,
  with missing events distinct from failure and polar topology; see the [daily query guide](RISE_SET.md).
- Search TT intervals for altitude crossings, upper transits and combined observing windows.
  These sampled searches can miss events even on success; see [altitude events and observing windows](ALTITUDE_EVENTS.md).
- Query full spherical angular diameters using geocentric or topocentric distance. Results identify the
  adopted radius and position provider; Saturn's scalar disk excludes rings. See [apparent angular diameters](APPARENT_DIAMETERS.md).

### JavaScript queries

`position.ecliptic` and `position.equatorial` return selected geocentric positions without calculating
a chart, houses or longitude rates. `events.longitudeCrossings`, `longitudeStations` and
`longitudeDifferences` expose the existing interval longitude searches, preserving native directions,
raw longitudes and half-open interval ownership.
See the [package guide](../bindings/javascript/README.md#independent-positions) and [longitude-event guide](EPHEMERIS_EVENTS.md#javascript).
The existing annual `sun.longitudeCrossings` query is unchanged.

### Delta T and input guards

Algo5 coefficients and the right-edge anchor use refreshed IERS Bulletin A inputs, extending the final
observations to 2026-10-01. Values change in the fitted and anchored future segments; the model structure,
pre-2005 delegation and model identity are unchanged.

UT1-to-JDE and apparent-solar-time calls reject year-zero inputs, including late December 31 inputs
that previously passed despite the declared year range.

### Compatibility

Existing C layouts and function signatures, package names and platform minimums are unchanged.
New query interfaces are additive. UTC conversion policies, including the historical UT1 substitute,
remain unchanged; use explicit time scales and convert TT results before clock display.

The [changelog](CHANGELOG.md) retains the full development history.
