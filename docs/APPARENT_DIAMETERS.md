# Apparent angular diameters

Use the full spherical disk diameter to size a Sun, Moon or planet icon. C and
owned Python/JavaScript results report arcseconds; C++ returns `AngleDeg`.
The result includes the adopted radius in km, actual distance in AU, radius
definition, position source, resolved UT1/TT times and selected Delta T identity.

```python
from celestial_calendar import CivilDateTime, CivilScale, Observer, Target, apparent_diameter

civil = CivilDateTime(2026, 11, 7, 0.0)
geo = apparent_diameter(Target.MOON, civil, CivilScale.UTC)
topo = apparent_diameter(Target.MOON, civil, CivilScale.UTC, Observer(-33.85, 151.2, 50.0))
print(geo.diameter_arcsec, topo.diameter_arcsec, topo.distance_au)
```

Supplying `Observer` selects topocentric distance. Omitting it selects geocentric
distance. Sites use WGS84 geodetic latitude, east-positive longitude and signed
ellipsoidal elevation in metres.

```js
import { init, diameter } from "@0xf3cd/celestial";

await init();
const disk = diameter.query("moon", { year: 2026, month: 11, day: 7, fraction: 0 }, {
  scale: "utc",
  observer: { latitudeDeg: -33.85, longitudeDeg: 151.2, elevationM: 50 },
  model: "algo2",
});
console.log(disk.diameterArcsec, disk.radiusDefinitionCode, disk.deltaTModel);
```

JavaScript results and their time records are frozen owned values. Python
returns frozen dataclasses. The optional model follows the existing Delta T
selectors; a result records the resolved identity, never `default`.

```cpp
#include "apparent_diameter.hpp"

const auto disk = astro::apparent_diameter::query(
  calendar::Datetime { util::to_ymd(2026, 11, 7), 0.0 },
  astro::julian_day::Scale::UTC,
  astro::ephemeris::Target::MOON
);
const double diameter_arcsec = disk.diameter.deg() * astro::toolbox::SEC_PER_DEG;
```

For direct geometry use `diameter(DistanceKm, DistanceAu)`. For an existing
JDE(TT), `geocentric(jde_tt, target)` returns `Size` with distance and angle.
`topocentric(times, observer, target)` accepts an explicit same-instant UT1/TT pair.
The C export is `celestial_apparent_diameter`; only its topocentric centre
selection reads the three site fields.

## What the scalar means

The geometry is the full spherical `2 asin(R/d)`, with the radius converted to AU
before division. The Sun uses the IAU 2015 B3 nominal radius, 695700 km, as an
adopted scale rather than a measured time-varying limb. The Moon uses the JPL SSD
mean radius, 1737.4 km, rather than an almanac lunar-k convention. Planets use the
JPL SSD equatorial radius as a scalar disk. Saturn's value excludes rings; this
query does not compute an aspect-dependent ellipse, orientation or illuminated width.
The [source record](../src/test/provenance/diameter/radii.json) lists all ten radii
and the [reference notes](../statistics/diameter_reference.md) explain validation.

Distance comes from the existing apparent-position provider, followed by the
existing observer translation when requested. Its distance semantics and
approximations remain applicable; there is no new observer light-time solve,
limb or refraction correction.

## Times and failures

The position provider's own domain and the selected Delta T model's guards apply,
without the chart or longitude-rate stencil window. UTC starts at 1972-01-01;
derived UT1 is modelled, not observed DUT1. Use explicit UT1 for earlier civil input.
The supported physical targets are Sun, Moon, Mercury, Venus, Mars, Jupiter,
Saturn, Uranus, Neptune and Pluto. Lunar nodes have no physical disk.

Direct geometry rejects nonfinite/nonpositive radius or distance and `d <= R`.
A successfully resolved observer at or inside the target is invalid input for
this disk query, even if the coordinate query can return a position. Unresolved
observer geometry and unrepresentable positive angles are computation errors.
Existing provider errors retain their classification. Tiny positive angles are
preserved; output is strictly between 0 and 648000 arcseconds.

Invalid input throws `std::invalid_argument`, Python `ValueError`, or JavaScript
`RangeError`. Computation failures throw `std::runtime_error` or `CelestialError`.
The C result has `valid=false`; read `celestial_last_error_kind` and
`celestial_last_error` before another recording call. Invalid payload has no meaning.

## Browser scaling

The [small browser renderer](examples/apparent_diameter/render.mjs) draws Sun and
Moon at 10 arcseconds per CSS pixel. Jupiter alone is magnified 20 times so its
disk is visible; both the magnification and numerical diameter are labelled.
The browser package consumer loads this renderer with its installed WASM artifact,
without a second package build.
