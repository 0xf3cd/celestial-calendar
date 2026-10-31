# CelestialCalendar Python Package

`celestial-calendar` provides Python bindings for CelestialCalendar: astronomical calculations, Gregorian / Chinese
Lunar date conversion, and Jieqi (节气, the 24 solar terms) moments. Each wheel contains its target platform's native
library, so using it requires no compiler or separate CelestialCalendar installation. Python 3.11 or newer is supported.

[中文](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/python/README_ZH.md)

This guide describes the current source, including unreleased changes. The install command gets the latest published
version; use its matching documentation. To run current-source examples, [build a local wheel](#build-a-local-wheel).
For upgrades from 0.x, see [Migrating to 1.0](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/MIGRATING_1_0.md).

<a id="install"></a>
## Install

Install the wheel for your platform from PyPI into your application's Python environment:

```sh
python -m pip install celestial-calendar
```

Each Python block below is a standalone program for `example.py`; run it with `python example.py` in that
environment. Apart from this package, the examples use only the Python standard library.

<a id="examples"></a>
## Examples

<a id="todays-lunar-date"></a>
### Today's lunar date

Choose today's civil date at a fixed UTC+8 offset first, then convert that date label with `ALGO3`. "Today" therefore
does not depend on the host timezone. This does not involve UT1, and the clock's UTC is not treated as UT1.

```python
from datetime import datetime, timedelta, timezone

import celestial_calendar as celestial

east_eight = timezone(timedelta(hours=8))
today = datetime.now(east_eight).date()
gregorian = celestial.GregorianDate.from_date(today)
lunar = celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO3, gregorian)
print(today, lunar)
```

`GregorianDate.from_date()` accepts a standard-library `date`, not a `datetime`. Calling `.date()` after selecting
the offset is deliberate: the library does not silently discard a time or timezone. The lunar algorithms use
different date bases and year windows; see [Lunar algorithms](#lunar-algorithms).

<a id="utc-conversions"></a>
### UTC and JDE(TT) conversions

`utc_to_jde()` and `jde_to_utc()` are available from 1.1.0. Before 1972 they treat UTC civil fields as UT1
using Algo5, the fixed ΔT model; chart queries still reject pre-1972 UTC. Leap-second boundaries are not generally
invertible. After the leap table's last entry, these conversions retain its final TT-minus-UTC offset.

```python
import celestial_calendar as celestial

utc = celestial.CivilDateTime(2026, 10, 28, (19 * 3600 + 35 * 60 + 37) / 86400)
jde_tt = celestial.utc_to_jde(utc)
print(celestial.jde_to_utc(jde_tt))
```

`fraction` is a fraction of a day (`0.5` is noon). JDE is a floating-point day count, so the converted civil
fields can differ slightly from the input.

For UTC input → next Jieqi → UTC+8 display, see the
[guide with runnable Python and JavaScript programs](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/UTC_CONVERSIONS.md#next-jieqi-from-a-utc-input-displayed-at-utc8).
It also gives the separate forward/inverse input domains and noninvertible boundaries.

<a id="next-jieqi"></a>
### Next Jieqi and remaining days

The reference instant is explicitly supplied as **2026-12-31 00:00 UT1**, not taken from the current clock.
Query every Jieqi in both 2026 and 2027, sort by actual UT1 moment, and select the first strictly later event.
The enum starts at Lichun (立春), not the first event in a Gregorian year.

```python
import celestial_calendar as celestial

cutoff_ut1 = celestial.CivilDateTime(2026, 12, 31, 0)
cutoff_jd = celestial.ut1_to_jd(cutoff_ut1)
events = [celestial.jieqi_moment(year, jieqi) for year in (2026, 2027) for jieqi in celestial.Jieqi]
ordered = sorted(events, key=lambda event: celestial.ut1_to_jd(event.moment_ut1))
upcoming = next(event for event in ordered if celestial.ut1_to_jd(event.moment_ut1) > cutoff_jd)
remaining_days = celestial.ut1_to_jd(upcoming.moment_ut1) - cutoff_jd
print(celestial.jieqi_name(upcoming.jieqi))
print("UT1:", upcoming.moment_ut1)
print("Remaining UT1 days:", remaining_days)
```

The difference between two UT1 Julian Days retains fractional days; subtracting calendar dates would lose the
time of day. This is a fixed contemporary query across one year boundary, not a search algorithm for the entire
API domain. The Jieqi year domain and failure rules are under [API contract](#api-contract).

In the UT1 examples above, `CivilDateTime` is explicitly UT1, and its last field is the day fraction: `0.5` means half a
day. `ut1_to_jde()` converts the same UT1 input to a TT-based JDE. `moment_ut1` is not UTC or a UTC+8 wall clock.
Establish the time-scale conversion before displaying either.

<a id="chart-snapshot-example"></a>
### Complete chart snapshot of one instant

The input is explicitly **2026-01-01 12:00 UTC**: the host timezone is not read, and UTC is not treated as UT1.

```python
import celestial_calendar as celestial

snapshot = celestial.chart_snapshot(
  celestial.CivilDateTime(2026, 1, 1, 0.5),
  celestial.CivilScale.UTC,
  celestial.GeoLocation(51.5, 0.0),
  celestial.HouseSystem.PLACIDUS,
)
print(snapshot.times.jd_ut1, snapshot.times.jde_tt)
for body in snapshot.bodies:
  print(body.target.name, body.longitude_deg, body.longitude_rate_deg_per_tt_day, body.distance_au)
print(snapshot.houses.cusps_deg)
```

The result is an owned, frozen dataclass; the body and cusp collections are tuples. The fourteen bodies are, in
order, the Sun, the Moon, Mercury through Pluto, the mean ascending/descending nodes, and the true
ascending/descending nodes; each has apparent geocentric longitude, latitude, and a signed longitude rate.
Positions use the true ecliptic and equinox of date, and angles are degrees. Distances are AU for every body,
including the Moon; the standalone `moon_apparent_geocentric_coordinate()` gives the Moon's distance in kilometres,
as `MoonCoordinate.distance_km`. The longitude rate is instantaneous, in degrees per TT day: a negative value means
the longitude is decreasing at that instant (retrograde motion). Nodes have known-zero latitude and `None` distance.
The four axes and twelve cusps are all in `[0, 360)` degrees, and the cusp tuple starts at house 1. Longitude is
east-positive and latitude north-positive; location affects houses only. See the
[shared replay example](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/examples/chart_snapshot/README.md)
for full values.

<a id="capabilities"></a>
### Capabilities and model identities

`capabilities()` describes native-owned TT target domains for POSITION, LONGITUDE_RATE, EVENTS and EQUATORIAL.
It rejects HOUSES, CHART and DELTA_T with `ValueError`. NaN endpoints mean no declared hard bound;
`FINITE_INPUT_EVALUATION` permits evaluation at finite epochs, which may still fail. Neither finite
evaluation nor coordinate-validity checks establish accuracy or physical plausibility at remote epochs.
`house_capability(delta_t_model=...)` instead returns a `CivilHouseCapability`: civil years, allowed scales,
UTC start date, resolved Delta T key and an optional exclusive derived-TT cutoff. It does not promise
geometric solvability. Civil-house policy is queried only through `house_capability()`, not `capabilities()`.
`capability_text()` requires a concrete `Target` for target operations, and `subject=None` for HOUSES,
CHART and DELTA_T descriptions. An inapplicable Target is rejected rather than ignored.

```python
import celestial_calendar as celestial

records = celestial.capabilities(celestial.CapabilityOperation.EVENTS)
pluto = next(r for r in records if r.subject is celestial.Target.PLUTO)
print(pluto.domain_start_jde_tt, pluto.domain_end_jde_tt, pluto.maximum_span_tt_days)
print(celestial.capability_text(celestial.CapabilityOperation.EVENTS, celestial.Target.PLUTO))
identities = celestial.model_identities()
algo5 = next(i for i in identities if i.key == celestial.ModelKey(celestial.ModelKind.DELTA_T, 5))
print(celestial.model_identity_text(algo5.key))
```

<a id="independent-queries"></a>
### Independent position and house queries

`positions()` evaluates only the targets you select at one JDE(TT) instant — no rate stencil,
no snapshot joint window — so a subset succeeds where a complete snapshot rejects an excluded
target's window. `house_query()` runs the snapshot's civil conversion without the joint window
and records which models it resolved. Both fail whole on any error; there is no partial result.

```python
import celestial_calendar as celestial

selected = celestial.positions(
  [celestial.Target.PLUTO, celestial.Target.SUN],
  2409543.5,  # Pluto's window start
)
for position in selected:
  print(position.target, position.longitude_deg, position.latitude_deg, position.distance_au, position.source)

location = celestial.GeoLocation(latitude_deg=51.5, longitude_deg=0.0)
civil = celestial.CivilDateTime(year=1885, month=1, day=1, fraction=0.0)  # before the snapshot window
houses = celestial.house_query(civil, celestial.CivilScale.UT1, location, celestial.HouseSystem.PLACIDUS)
print(houses.times, houses.houses.ascendant_deg, houses.delta_t_model, houses.nutation_model)
again = celestial.house_query(
  civil,
  celestial.CivilScale.UT1,
  location,
  celestial.HouseSystem.PLACIDUS,
  delta_t_model=houses.delta_t_model,
  nutation_model=houses.nutation_model,
)
equatorial = celestial.equatorial_positions([celestial.Target.SUN, celestial.Target.TRUE_ASCENDING], 2451545.0)
for position in equatorial:
  print(position.right_ascension_deg, position.declination_deg, position.distance_au, position.source)
```

`Target` is shared by positions, events, capabilities and chart bodies. Ecliptic and equatorial results are distinct frozen values,
preserve selection order, and reject empty or duplicate selections. RA is `[0, 360)` degrees and declination
`[-90, 90]` degrees, geocentric and of date; the projection uses fixed IAU1980 longitude nutation and true obliquity.
Neither query computes rates, civil times or houses. Physical distances are AU; mathematical nodes have no distance.

`ModelKey(kind, identity)` is immutable and kind-qualified; zero is never a resolved identity. Registry entries
expose `.key`, position/chart bodies expose `.source`, and house/chart results expose `.delta_t_model` and
`.nutation_model`. Matching keys can replace selector enums without converting registry ordinals. DEFAULT is
input-only and native-resolved; use operation text to inspect defaults. Reusing keys preserves model selection,
while exact coefficients also require the library/source version. `delta_t(..., model=...)` still returns a float
and validates the native resolved key. Source fields use `ModelKey`.
`SunCoordinate.distance_au` is in AU; `MoonCoordinate.distance_km` is in kilometres.

<a id="interval-longitude-events"></a>
### Interval longitude events

`longitude_events()`, `longitude_stations()` and `longitude_difference_events()` expose the
[C++ interval searches](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/EPHEMERIS_EVENTS.md)
for supplied longitudes, stationary instants and directed two-body longitude differences. Angles carry no
names or interpretations — map exact target angles to your own terminology.

```python
import celestial_calendar as celestial

crossings = celestial.longitude_events(celestial.Target.MERCURY, 0.0, 2460735.5, 2460785.5)
for event in crossings:
  print(event.jde_tt, event.longitude_deg, event.direction)

stations = celestial.longitude_stations(celestial.Target.MERCURY, 2460735.5, 2460785.5)
for station in stations:
  print(station.jde_tt, station.motion_before.value, "->", station.motion_after.value)

differences = celestial.longitude_difference_events(
  celestial.Target.MOON, celestial.Target.SUN, -60.0, 2460676.5, 2460707.5
)
```

Times are JDE(TT) on the published 2^-30-day reporting lattice; rates are degrees per TT day from the published
seven-point operator. The interval `[start, end)` must be nonempty, at most 3660 TT days, and inside the
target's longitude-rate domain; an empty tuple is a legal no-event outcome, while numerical ambiguity or
work-limit exhaustion raises `CelestialError` rather than returning a partial list.

<a id="platforms-and-local-builds"></a>
## Platforms and local builds

Official releases use wheels for the four platforms below; PyPI and the matching
[GitHub Release](https://github.com/0xf3cd/celestial-calendar/releases) carry the same four wheels, byte for byte:

| System | Architecture | Required tag in the wheel filename |
|---|---|---|
| Linux (manylinux 2.28) | x86_64 | `manylinux_2_28_x86_64` |
| Linux (manylinux 2.28) | aarch64 | `manylinux_2_28_aarch64` |
| macOS 15 or newer | arm64 | `macosx_15_0_arm64` |
| Windows | AMD64 | `win_amd64` |

Linux wheel filenames may include other compatible manylinux tags. Each wheel is a `py3` wheel for Python 3.11 or
newer on its platform. No wheels are published for other platforms, such as Intel macOS, Windows on ARM, or
musl-based Linux.

<a id="build-a-local-wheel"></a>
### Build a local wheel

Use a full source checkout; copying only `bindings/python` does not work. The Linux shell example below needs
Python 3.11 or newer, system CMake 3.22 or newer, and an installed C/C++23 toolchain. It selects `clang-22` and
`clang++-22`; if your compilers have other names, set `CC` and `CXX` accordingly. The existing hash-locked
requirements supply the build frontend, the backend, and Ninja. Run it from the repository root, with build,
virtual-environment, and output directories that are not already in use:

```sh
python3 -m venv build/local-wheel-env
. build/local-wheel-env/bin/activate
python -m pip install --require-hashes -r bindings/python/requirements-build.txt
CC=clang-22 CXX=clang++-22 python -m build \
  --wheel --no-isolation \
  --outdir build/local-wheelhouse \
  --config-setting=build-dir="$PWD/build/local-wheel-build" \
  bindings/python
deactivate
```

Install the `.whl` produced in `build/local-wheelhouse` into your application environment. This builds only a
wheel, not an sdist. That wheel uses the host's native library; it is not an official repaired wheel or a portable
release wheel, and it does not extend the supported platforms in the table above.

<a id="api-contract"></a>
## API contract

The flat public API uses frozen dataclasses, enums, plain scalars, and tuples; it does not expose the ctypes
protocol. `CivilDateTime` has four fields: `year`, `month`, `day`, and `fraction`, a finite fraction of the day in
`[0, 1)`; there are no separate hour, minute, or second fields. A function name, a field name, or `CivilScale`
states the time scale; civil moments are never implicitly converted to Python `datetime` values. The docstrings in
[`__init__.py`](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/python/src/celestial_calendar/__init__.py)
give the per-function contracts.

Wrong input types, including members of the wrong enum, raise `TypeError`. Values that fail finiteness, range, or
domain checks raise `ValueError`, and so does an object of the expected enum class that is not one of its declared
members. Native invalid-input errors also raise `ValueError`. Computation, resource and internal failures raise
`CelestialError`, as do corrupt successful payloads. Its `operation` names the public call; `recorded` is true when
native reported a recognized error kind, even if it could not allocate a message. A legitimate absence remains `None` or `()`.
Errors are not automatically printed.

`local_apparent_sidereal_time(jd_ut1, longitude_deg)` accepts a finite JD on UT1 whose Gregorian year is in
`[401, 32766]`, and a finite, east-positive geographic longitude in `[-180, 180]` degrees. The result is in
`[0, 360)` degrees. A JD outside the native year range raises `ValueError`. `apparent_solar_time()` also accepts an
east-positive geographic longitude in `[-180, 180]` degrees, but its civil input is UTC.

`jieqi_moment(year, jieqi)` returns `JieqiMoment(jieqi, moment_ut1)`. Its `CivilDateTime` is UT1, not UTC or a
UTC+8 wall clock; shown at UTC+8, the same instant can fall on a different date. Establish the time-scale
conversion before displaying UTC or a fixed-offset time. The returned UT1 year can differ from the requested year.
The input year domain is `[401, 32766]`, but even an in-range call raises `CelestialError` when the native
calculation cannot produce a unique moment.

`moon_phase_moments(year, phase)`, `sun_longitude_crossings(year, longitude_deg)`, and `new_moons_in_year(year)`
accept Gregorian years in `[1, 32766]`.

`sun_longitude_crossings()` returns a tuple of JDEs on TT at which the Sun reaches the given apparent geocentric
longitude. `longitude_deg` must be finite and in `[0, 360)` degrees. Native failures use
`CelestialError.operation == "sun_longitude_crossings"`.

`new_moons_after(jde, count)` accepts `count` in `[0, 4096]`; zero returns `()`.
The cap keeps a single native output buffer within 32 KiB.
Variable-length result and text buffers have a separate binding resource cap of 64 MiB per buffer; exceeding it
raises `CelestialError` before allocation. Inconsistent native results also raise `CelestialError`.

<a id="chart-inputs-and-failure-boundaries"></a>
### Chart inputs and failure boundaries

`chart_snapshot(civil_dt, scale, location, system, model=DeltaTModel.DEFAULT)` accepts a `CivilDateTime`,
`CivilScale.UTC` / `UT1`, a `GeoLocation`, and `HouseSystem.EQUAL` / `WHOLE_SIGN` / `PLACIDUS`.
Native resolves `DEFAULT` (currently `ALGO5`); select models explicitly with `DeltaTModel` or a matching `ModelKey`. It does not accept
a standard-library `datetime`: the caller must first convert local wall-clock time to the declared input scale,
not drop the timezone and pass the rest.

Python checks record/enum/scalar types, finiteness and ABI integer width before native. Wrong types raise `TypeError`, including a plain string
or another enum's member for `scale`, `system`, or `model`, and an enum member in a numeric field. `ValueError`
covers an object of the expected enum class that is not one of its declared members, invalid Gregorian dates,
non-finite numbers, and raw values out of range: civil years `[1885, 2100)`, day fraction `[0, 1)`, UTC from
1972-01-01 (select `CivilScale.UT1` for earlier input), latitude `(-90, 90)`, and longitude `[-180, 180]`.
The native core owns the shared converted-TT domain JDE `[2409543.875, 2488069.125)`, Algo4's additional
JDE `< 2464328.5` limit, Ascendant directions unresolved at round-off in every house system, and date-dependent
Placidus polar limits. Native domain/geometry rejection raises `ValueError`; computation failures raise
`CelestialError`, with no partial result or substitute house system. Civil-house queries share the same exclusive
derived-TT Algo4 cutoff; this also rejects UT1 inputs near the end of 2034 whose derived TT crosses the cutoff.
A valid raw civil year does not guarantee that the converted snapshot can be calculated.

For UTC input, the reported UT1 is modelled UT1, not observed DUT1;
[Time scales](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md#time-scales) explains how UTC
reaches TT and UT1. An inserted leap second is outside the civil day-fraction input. The Python result is copied
from the C protocol and keeps no native buffer; incompatible extensions require an explicitly different shape or major.

<a id="date-bridges-and-lunar-round-trip"></a>
### Date-only bridges and Lunar round trip

`GregorianDate(year, month, day)` is a proleptic Gregorian date, separate from `CivilDateTime` and `LunarDate`.
Calculation calls check that Gregorian date and civil-moment inputs have a year in `[1, 32767]` and a valid month
and day. The project's date types still have a wider range than the standard library's: the constructors do not
cap the year at 9999.

```python
from datetime import date

import celestial_calendar as celestial

day = date(2026, 8, 15)
gregorian = celestial.GregorianDate.from_date(day)
print(gregorian.to_date())

algorithm = celestial.LunarAlgorithm.ALGO3
lunar = celestial.gregorian_to_lunar(algorithm, day)
print(celestial.lunar_to_gregorian(algorithm, lunar).to_date())
```

`GregorianDate.from_date(value)` is a static factory: it always returns exactly `GregorianDate`, even when called
through a subclass. It accepts `datetime.date` instances and subclasses in the standard library's year range
`[1, 9999]`, but rejects every `datetime.datetime` instance or subclass, with or without a timezone, by raising
`TypeError`. `gregorian_to_lunar(algorithm, date)` accepts a `GregorianDate` or the same standard-library date
input; it rejects `datetime` rather than dropping its time or offset.

`GregorianDate.to_date()` checks the fields and returns a standard-library `datetime.date`. A field of the wrong
type raises `TypeError`; a year outside `[1, 9999]` or an invalid Gregorian month or day raises `ValueError`.
Both bridges preserve the year/month/day label. In Lunar conversion, that label belongs to the date basis of the
algorithm selected below; neither bridge converts timezones or UT1/UTC/TT.

<a id="lunar-algorithms"></a>
### Lunar algorithms

Select `LunarAlgorithm.ALGO1`, `ALGO2` or `ALGO3`. Gregorian inputs and outputs are calendar-date labels, not instants.
For sources, date bases, Lunar-year domains and selection roles, see
[Lunar algorithms](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/ALGORITHMS.md#lunar-algorithms).

`supported_lunar_year_range(algorithm)` queries the native algorithm's inclusive **Lunar-year** range and returns
`LunarYearRange(start, end)`. Gregorian coverage runs from the first day of Lunar year `start` to the last day of
Lunar year `end`, not from January to December of those Gregorian years. `lunar_year_info(algorithm, year)` returns
the Gregorian `first_day`, the traditional `leap_month` number or `None`, and `month_lengths` in calendar order,
with a leap month directly after the regular month of the same name. `lunar_to_gregorian()` accepts a `LunarDate`
with a traditional month number in `[1, 12]` and an explicit `is_leap` boolean, not a positional month index.

`lunar_year_info()` and `lunar_to_gregorian()` leave date/year admission to their native calculation.
The Python package keeps no separate range table or cache and makes no range query at import or before those calls.

A valid Gregorian label outside the selected algorithm's coverage, or a Lunar leap-month/day combination that does
not exist, raises `ValueError`, as does a Lunar year outside the selected algorithm's domain.

<a id="delta-t-models"></a>
### Delta T models

`delta_t(year, model=DeltaTModel.DEFAULT)` returns TT minus UT1 in seconds for a finite decimal Gregorian year.
Select `DeltaTModel.ALGO1` through `ALGO5`, or a matching `ModelKey`; the default resolves to Algo5.
Sources, maintenance status, hard domains and selection roles are in
[Delta T models](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/ALGORITHMS.md#delta-t-models).

<a id="license"></a>
## License

Project-authored package material is licensed under MIT. Third-party components retain their own terms
in the included `THIRD_PARTY_NOTICES.txt`; each section states where it applies. Source and issue
tracking are at <https://github.com/0xf3cd/celestial-calendar>.
