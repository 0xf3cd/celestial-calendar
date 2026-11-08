# Observer queries

Use an observer query for the centre of the Sun, Moon or a planet as seen from a fixed
site on Earth. Existing geocentric position and chart queries are unchanged.

## Coordinates and time

The site uses WGS84 geodetic latitude in [-90, 90] degrees, east-positive longitude
in [-180, 180] degrees and signed **ellipsoidal height**, not height above the geoid.
C, Python and JavaScript take metres; C++ uses `toolbox::DistanceKm`. Height must be
finite and greater than `-a(1-f)^2` (about -6335439.3 m), where `a` is the WGS84
semi-major axis and `f` its flattening. This conservative geodetic-map admission
does not make a large positive height a supported moving-satellite model.

Declare civil UTC or UT1 explicitly. UTC input before 1972-01-01 is rejected as
invalid input; the query does not automatically substitute UT1. UTC-to-UT1 uses the
selected Delta T model and held leap-second table, not observed DUT1. The chosen
model's own guards and the target's position-provider window apply. Chart civil-year
limits and the extra time margins needed for numerical rates do not apply. Pluto's
TT position domain is [2409543.5, 2488069.5).

Results contain resolved JD(UT1)/JDE(TT), target/source/Delta T identity, topocentric
RA/Dec in the true equator/equinox of date, observer distance in AU, geometric
centre altitude and azimuth. **Azimuth is north-zero, east-positive** (east = 90°).
The older `HorizontalCoord` uses south-zero/west-positive angles and is unchanged.

RA is optional when the equatorial projection is unresolved. Azimuth is optional at
zenith/nadir or an unresolved horizontal projection, and absent at exactly either
geographic pole because geographic north has no unique direction there. For latitudes
just short of ±90°, longitude still defines local north and east; azimuth is returned
if the horizontal projection is resolved. Declination, distance and geometric altitude
remain available when those individual angles are absent.

Projected lengths are compared with 32 times double epsilon times the larger
pre-subtraction vector scale. The query reports a computation error if it cannot
resolve the observer-to-body separation at floating-point precision. These are
conditioning/resolution limits, **not angular accuracy guarantees**. Near cancellation
or a tiny projection, a returned direction can still be sensitive to input rounding.
Nonrepresentable results raise a computation error.

## Python

```python
from celestial_calendar import CivilDateTime, CivilScale, Observer, Target, topocentric_position

sky = topocentric_position(
  Target.MOON,
  CivilDateTime(2026, 11, 7, 0.0),
  CivilScale.UTC,
  Observer(latitude_deg=-33.85, longitude_deg=151.2, elevation_m=50.0),
)
assert sky.distance_au > 0.0
assert sky.apparent_altitude_deg is None
print(sky.altitude_deg, sky.azimuth_deg)
```

Python values are immutable dataclass instances. Pass
`RefractionParams(RefractionModel.BENNETT)` or
`RefractionParams(RefractionModel.SAEMUNDSSON)` as `refraction=` for apparent altitude.
`delta_t_model=` accepts `DeltaTModel` or a matching `ModelKey`. The `cele` and
`wentian` packages forward the same top-level functions and types.

## JavaScript / TypeScript

```js
import { init, observer } from "@0xf3cd/celestial";

await init();
const sky = observer.position("moon", { year: 2026, month: 11, day: 7, fraction: 0 }, {
  scale: "utc",
  observer: { latitudeDeg: -33.85, longitudeDeg: 151.2, elevationM: 50 },
  refraction: { model: "saemundsson", temperatureC: 15, pressureHpa: 1013.25 },
});
if (!(sky.distanceAu > 0)) throw new Error("invalid observer distance");
console.log(sky.altitudeDeg, sky.azimuthDeg, sky.apparentAltitudeDeg);
```

Optional angles are `null`. Each result copies its values out of WASM memory, so
later calls do not overwrite it. `model` selects Delta T, defaulting to `"default"`.
The `celestial-calendar` npm alias forwards this API.

## C++ and C

```cpp
#include "observer.hpp"
#include "ymd.hpp"

int main() {
  const auto result = astro::observer::query(
    calendar::Datetime { util::to_ymd(2026, 11, 7), 0.0 },
    astro::julian_day::Scale::UTC,
    { .location = { .latitude = astro::toolbox::AngleDeg { -33.85 },
                    .longitude = astro::toolbox::AngleDeg { 151.2 } },
      .elevation = astro::toolbox::DistanceKm { 0.05 } },
    astro::ephemeris::Target::MOON
  );
  return result.position.distance.au() > 0.0 ? 0 : 1;
}
```

The low-level `observer::observe(times, site, target)` takes caller-resolved finite
UT1 and TT of the same instant. It does not evaluate or attach a Delta T identity.
`observer::query` composes that operation with the civil resolver.

```c
#include "celestial.h"

int main(void) {
  const CelestialTopocentricPosition sky = celestial_topocentric_position(
    CELESTIAL_TARGET_MOON, 2026, 11, 7, 0.0,
    CELESTIAL_CIVIL_SCALE_UTC, CELESTIAL_DELTA_T_MODEL_DEFAULT,
    -33.85, 151.2, 50.0, CELESTIAL_REFRACTION_NONE, 0.0, 0.0
  );
  if (!sky.valid) return 1; /* Read celestial_last_error_kind/message on failure. */
  if (sky.present_fields & CELESTIAL_PRESENT_AZIMUTH) {
    return sky.azimuth_deg >= 0.0 && sky.azimuth_deg < 360.0 ? 0 : 1;
  }
  return 0;
}
```

Read optional C storage only when its presence bit is set. Unset storage has no
meaning and need not be finite. `CELESTIAL_REFRACTION_NONE` ignores weather storage.
Operation code `CELESTIAL_OPERATION_TOPOCENTRIC` supplies operation-wide capability
text for `CELESTIAL_TARGET_ALL`; EQUATORIAL target records describe its physical providers.

## Refraction and model limits

Refraction is off by default and changes only the optional apparent-altitude field.
Weather defaults are 15°C / 1013.25 hPa; the supported implementation window is
[-90, 60]°C and [300, 1100] hPa, inclusive. Invalid weather/model selection raises an
input error even at unsupported altitude. Valid weather at an unsupported geometric
altitude leaves apparent altitude absent without losing the geometric result.

Bennett takes apparent altitude, so the query inverts its weather-corrected formula
on apparent [0, 90] degrees with a bounded solve. Saemundsson is evaluated directly
at geometric altitude. Both admit their computed apparent-horizon geometric lower
bound through geometric 90°. Saemundsson preserves the signed residual at that
inclusive lower bound (down to -1e-9 degrees) and the formula's tiny negative zenith
refraction, without clamping. These support intervals do not promise atmospheric
accuracy. No weather is inferred from height.

The source is the existing apparent geocentric provider with fixed IAU1980 rotation.
Observer translation follows Meeus 40.6/40.7 with WGS84 geometry (Chapter 11), then
direct local projection (13.5/13.6). It adds no diurnal aberration, EOP/polar motion,
observer-dependent light-time solve or solar light-deflection model. Terrain, horizon
dip, limb and apparent-diameter corrections are not part of the centre coordinates.

For altitude-event consumers, apply parallax and refraction **once**. The existing
lunar geocentric rise/set altitude already folds in parallax/refraction/limb terms;
do not reuse it unchanged for a topocentric centre. This query does not change the
existing rise/set event implementation.

Numerical references and exact-input replay are in
[`statistics/observer_reference.md`](../statistics/observer_reference.md).
