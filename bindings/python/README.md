# CelestialCalendar Python Package

`celestial-calendar` provides Python bindings for CelestialCalendar: astronomical calculations, Gregorian / Chinese
Lunar date conversion, and Jieqi (节气, the 24 solar terms) moments. Each wheel contains its target platform's native
library, so using it requires no compiler or separate CelestialCalendar installation. Python 3.11 or newer is supported.

[中文](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/python/README_ZH.md)

This guide describes version `0.8.0`. The install command below gets the latest version on PyPI; an older installed
version may lack some APIs shown here, so check its version before running an example. To use the current source,
build a local wheel from a full checkout as described below.

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

In these conversions `CivilDateTime` is explicitly UT1, and its last field is the day fraction: `0.5` means half a
day. `ut1_to_jde()` converts the same UT1 input to a TT-based JDE. `moment_ut1` is not UTC or a UTC+8 wall clock.
Establish the time-scale conversion before displaying either.

<a id="chart-snapshot-example"></a>
### Complete chart snapshot of one instant

`chart_snapshot` is new in 0.8.0.
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

<a id="platforms-and-local-builds"></a>
## Platforms and local builds

Official releases use wheels for the four platforms below; PyPI and the matching
[GitHub Release](https://github.com/0xf3cd/celestial-calendar/releases) carry the same four wheels, byte for byte:

| System | Architecture | Required tag in the wheel filename |
|---|---|---|
| Linux (manylinux 2.28) | x86_64 | `manylinux_2_28_x86_64` |
| Linux (manylinux 2.28) | aarch64 | `manylinux_2_28_aarch64` |
| macOS 14 or newer | arm64 | `macosx_14_0_arm64` |
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
domain checks raise `ValueError`. Failures reported by the native boundary raise `CelestialError`; its `operation`
names the public call, and `recorded` says whether the message came from the native error channel. A legitimate
absence remains `None` or `()`.

`local_apparent_sidereal_time(jd_ut1, longitude_deg)` accepts a finite JD on UT1 whose Gregorian year is in
`[401, 32766]`, and a finite, east-positive geographic longitude in `[-180, 180]` degrees. The result is in
`[0, 360)` degrees. The native boundary checks the year range, so a JD outside it raises `CelestialError`, not the
`ValueError` used by the Python finiteness and longitude checks. `apparent_solar_time()` also accepts an
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

<a id="chart-inputs-and-failure-boundaries"></a>
### Chart inputs and failure boundaries

`chart_snapshot(civil_dt, scale, location, system, model=DeltaTModel.DEFAULT)` accepts a `CivilDateTime`,
`CivilScale.UTC` / `UT1`, a `GeoLocation`, and `HouseSystem.EQUAL` / `WHOLE_SIGN` / `PLACIDUS`.
`DEFAULT` is the same as `ALGO5`; select other models explicitly with the existing `DeltaTModel`. It does not accept
a standard-library `datetime`: the caller must first convert local wall-clock time to the declared input scale,
not drop the timezone and pass the rest.

Python checks every input before the native calculation. Wrong types raise `TypeError`, including a plain string
or another enum's member for `scale`, `system`, or `model`, and an enum member in a numeric field. `ValueError`
covers an object of the expected enum class that is not one of its declared members, invalid Gregorian dates,
non-finite numbers, and raw values out of range: civil years `[1885, 2100)`, day fraction `[0, 1)`, UTC from
1972-01-01 (select `CivilScale.UT1` for earlier input), latitude `(-90, 90)`, and longitude `[-180, 180]`.
The native core owns the shared converted-TT domain JDE `[2409543.875, 2488069.125)`, Algo4's additional
JDE `< 2464328.5` limit, Ascendant directions unresolved at round-off in every house system, and date-dependent
Placidus polar limits. Those failures raise `CelestialError`, with no partial result or substitute house system.
A valid raw civil year does not guarantee that the converted snapshot can be calculated.

For UTC input, the reported UT1 is modelled UT1, not observed DUT1;
[Time scales](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md#time-scales) explains how UTC
reaches TT and UT1. An inserted leap second is outside the civil day-fraction input. The Python result is copied
from the fixed C V1 protocol and keeps no native buffer; incompatible extensions of the fields or body set use a new
protocol version.

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

Gregorian inputs and outputs are calendar-date labels on the selected algorithm's basis, not instants.
Choose the source and compatibility role you need, not an assumed accuracy ranking:

| Algorithm | Source and status | Lunar-year domain | Selection role |
|---|---|---|---|
| `LunarAlgorithm.ALGO1` | Published Hong Kong Observatory date labels, stored as a table | `[1901, 2099]` | HKO almanac compatibility |
| `LunarAlgorithm.ALGO2` | Computed from VSOP87D and truncated ELP2000-82B | `[410, 2500]` | Astronomical construction over the wider computed window |
| `LunarAlgorithm.ALGO3` | Baked hybrid: algo1/HKO in 1901-2099, algo2-generated dates elsewhere | `[1600, 2199]` | Table lookup with HKO compatibility in the overlapping years |

Algo2 assigns dates by rendering TT moments in the library's modelled UTC and then applying a fixed eight-hour
eastward offset. That modelled UTC is UT1 before 1972 and adds no leap seconds after the table's last entry (2017);
see [Time scales](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/DESIGN.md#time-scales). Algo1
preserves HKO's published labels rather than imposing this rule on their history; algo3 inherits the basis of each
source slice. The 2500 ceiling is the computed algorithm's enforced civil-date domain, not a physical limit of
VSOP87D or ELP2000-82B.

`supported_lunar_year_range(algorithm)` queries the native algorithm's inclusive **Lunar-year** range and returns
`LunarYearRange(start, end)`. Gregorian coverage runs from the first day of Lunar year `start` to the last day of
Lunar year `end`, not from January to December of those Gregorian years. `lunar_year_info(algorithm, year)` returns
the Gregorian `first_day`, the traditional `leap_month` number or `None`, and `month_lengths` in calendar order,
with a leap month directly after the regular month of the same name. `lunar_to_gregorian()` accepts a `LunarDate`
with a traditional month number in `[1, 12]` and an explicit `is_leap` boolean, not a positional month index.

`lunar_year_info()` and `lunar_to_gregorian()` query the native range once per call before checking the Lunar year.
The Python package keeps no separate range table or cache and does not query ranges at import. A Lunar year out
of range raises `ValueError`; a failed native range query raises `CelestialError` whose public operation is the
caller, `"lunar_year_info"` or `"lunar_to_gregorian"`, not the internal query.

A valid Gregorian label outside the selected algorithm's coverage, or a Lunar leap-month/day combination that does
not exist, raises `CelestialError`. For example, `gregorian_to_lunar(LunarAlgorithm.ALGO1, GregorianDate(1900, 1, 1))`
passes Gregorian validation but raises `CelestialError`, whereas `lunar_year_info(LunarAlgorithm.ALGO1, 1900)`
raises `ValueError`.

<a id="delta-t-models"></a>
### Delta T models

`delta_t(year, model)` returns TT minus UT1 in seconds for a finite decimal Gregorian year.
The default is the current project model; the named older models allow historical comparison:

| Model | Source and status | Hard year domain | Selection role |
|---|---|---|---|
| `DeltaTModel.DEFAULT`, `DeltaTModel.ALGO5` | Current project model: algo2 before 2005, IERS Bulletin A fit through about 2026.41, then anchored Morrison et al. (2021) long-term extrapolation | No model-specific bound | Current project calculations |
| `DeltaTModel.ALGO1` | Xu Jianwei's 2008 model, frozen | `year >= -4000` | Comparison with that historical model |
| `DeltaTModel.ALGO2` | Espenak and Meeus, NASA/TP-2006-214141, frozen | No model-specific bound | NASA polynomial comparison; live pre-2005 branch of algo3, algo4, and algo5 |
| `DeltaTModel.ALGO3` | Fred Espenak's 2014 eclipse canon, frozen | `year < 3000` | Comparison with that later polynomial model |
| `DeltaTModel.ALGO4` | IERS Bulletin A observations and USNO predictions, frozen | `year < 2035` | Comparison with the previous project fit/prediction model |

"Frozen" describes model maintenance, not dead code: algo2 still supplies the pre-2005 values used by the current
default. A hard domain is an input contract, not an accuracy claim. Fitted residuals and test tolerances are not
statistical error guarantees, and an unbounded model does not promise useful accuracy at arbitrary years.

<a id="license"></a>
## License

Project-authored package material is licensed under MIT. Third-party components retain their own terms
in the included `THIRD_PARTY_NOTICES.txt`; each section states where it applies. Source and issue
tracking are at <https://github.com/0xf3cd/celestial-calendar>.
