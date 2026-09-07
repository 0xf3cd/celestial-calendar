# CelestialCalendar for Python

`celestial-calendar` provides Python bindings for CelestialCalendar's astronomical calculations, Gregorian and
Chinese Lunar conversions, and Jieqi (节气) moments. The wheel contains the native library for its target platform;
no compiler or separate CelestialCalendar installation is required at runtime. Python 3.11 or newer is supported.

## Install

Install the wheel for your platform from PyPI:

```sh
python -m pip install celestial-calendar
```

The same four wheel bytes are published to PyPI and the matching
[GitHub release](https://github.com/0xf3cd/celestial-calendar/releases):

| System | Architecture | Required tag in the wheel filename |
|---|---|---|
| Linux (manylinux 2.28) | x86_64 | `manylinux_2_28_x86_64` |
| Linux (manylinux 2.28) | aarch64 | `manylinux_2_28_aarch64` |
| macOS 14 or newer | arm64 | `macosx_14_0_arm64` |
| Windows | AMD64 | `win_amd64` |

Linux wheel filenames may contain additional compatible manylinux tags. Each is a `py3` wheel for Python 3.11 or
newer on that platform.
No wheel is published for other platforms, such as Intel macOS, Windows on ARM, or musl-based Linux.

## API

```python
import celestial_calendar as celestial

ut1 = celestial.CivilDateTime(2026, 8, 16, 0.5)
jde = celestial.ut1_to_jde(ut1)
winter_solstice = celestial.jieqi_moment(2026, celestial.Jieqi.DONGZHI)

print(jde)
print(winter_solstice.moment_ut1)
```

The flat public API uses immutable dataclasses and enums. `CivilDateTime` has four fields: `year`, `month`,
`day`, and `fraction`, a finite day fraction in `[0, 1)`. It has no separate hour, minute, or second fields. The function
or field name identifies the time scale; civil moments are not silently converted to Python's `datetime` types.

Wrong input types, including members of the wrong enum, raise `TypeError`. Values rejected by finiteness, range, or
domain checks raise `ValueError`. A failure reported by the native boundary raises `CelestialError`. Its `operation`
attribute names the public function, and its `recorded` attribute says whether the message came from the native error
channel. A legitimate absence remains `None` or `()`.

`jieqi_moment(year, jieqi)` returns `JieqiMoment(jieqi, moment_ut1)`. The nested `CivilDateTime` is UT1, not UTC or an
east-eight wall clock; rendering the same instant at UTC+8 can change its calendar date. Establish the time-scale
conversion before using a UTC or fixed-offset display. The returned UT1 year can differ from the requested year.
The input window is `[401, 32766]`, but an in-range query can still raise `CelestialError` if the native calculation
cannot produce a unique moment.

`moon_phase_moments(year, phase)`, `sun_longitude_crossings(year, longitude_deg)`, and `new_moons_in_year(year)` accept
Gregorian years in `[1, 32766]`.

`sun_longitude_crossings()` returns a tuple of TT-based JDEs when the Sun reaches the requested apparent geocentric
longitude. `longitude_deg` must be finite and in `[0, 360)`. Native failures use
`CelestialError.operation == "sun_longitude_crossings"`. The previous name has no compatibility alias.

`new_moons_after(jde, count)` accepts `count` in `[0, 4096]`; zero returns `()`. The upper bound keeps one native
output buffer at or below 32 KiB.

### Date-only bridge

`GregorianDate(year, month, day)` is a proleptic Gregorian date, separate from `CivilDateTime` and `LunarDate`.
Calculation calls validate Gregorian and civil inputs for years in `[1, 32767]` and valid month/day fields.
The project value remains wider than the standard library's date type; its constructor is not capped at year 9999.

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
through a `GregorianDate` subclass. It accepts standard-library `datetime.date` instances and their subclasses in
the standard library's year range `[1, 9999]`, but rejects all `datetime.datetime` instances and subclasses with
`TypeError`, whether naive or timezone-aware.
`gregorian_to_lunar(algorithm, date)` accepts either a `GregorianDate` or the same date-only standard-library inputs;
it does not discard the time or offset of a datetime.

`GregorianDate.to_date()` validates the fields and returns a standard-library `datetime.date`. Wrong field types
raise `TypeError`; a year outside `[1, 9999]` or an invalid Gregorian month/day raises `ValueError`.
Both bridge methods preserve the year/month/day label. For lunar conversion, that label belongs to the selected
algorithm's date basis below; neither method performs a timezone or UT1/UTC/TT conversion.

### Lunar algorithms

Gregorian inputs and outputs are calendar-date labels on the selected algorithm's basis, not instants.
Choose the source and compatibility role you need, not an assumed accuracy ranking:

| Algorithm | Source and status | Lunar-year domain | Selection role |
|---|---|---|---|
| `LunarAlgorithm.ALGO1` | Published Hong Kong Observatory date labels, stored as a table | `[1901, 2099]` | HKO almanac compatibility |
| `LunarAlgorithm.ALGO2` | Computed from VSOP87D and truncated ELP2000-82B | `[410, 2500]` | Astronomical construction over the wider computed window |
| `LunarAlgorithm.ALGO3` | Baked hybrid: algo1/HKO in 1901-2099, algo2-generated dates elsewhere | `[1600, 2199]` | Table lookup with HKO compatibility in the overlapping years |

Algo2 assigns dates by rendering TT moments through the library's UTC model and then applying a fixed eight-hour
eastward offset. Before 1972, that model deliberately uses UT1 as a proxy because historical UTC is not modelled.
From 1972 it uses the leap-second table; after the final entry (2017) it holds Delta AT (TAI minus UTC) at 37 seconds.
Algo1 preserves HKO's published labels rather than imposing this rule on their history; algo3 inherits the basis of
each source slice. The 2500 ceiling is the computed algorithm's enforced civil-date domain, not a physical limit of
VSOP87D or ELP2000-82B.

`supported_lunar_year_range(algorithm)` queries the native inclusive **lunar-year** range, returned as
`LunarYearRange(start, end)`. Gregorian date coverage runs from the first day of lunar year `start` to the last day
of lunar year `end`, not from January through December of those Gregorian years.
`lunar_year_info(algorithm, year)` returns the Gregorian `first_day`, a traditional `leap_month` number or `None`,
and `month_lengths` in calendar order, with the leap month after its ordinary namesake. `lunar_to_gregorian()` takes
a `LunarDate` with traditional month numbers `[1, 12]` and an explicit `is_leap` boolean, not a positional month index.

`lunar_year_info()` and `lunar_to_gregorian()` query the native range once per call before validating the lunar year.
The Python package keeps no duplicate range table or cache and performs no import-time range query. An out-of-range
lunar year raises `ValueError`; a native range-query failure raises `CelestialError` with the caller's public
operation, `"lunar_year_info"` or `"lunar_to_gregorian"`, not the internal query.

A valid Gregorian label outside the selected algorithm's coverage, or a lunar leap-month/day combination that does
not exist, raises `CelestialError`. For example,
`gregorian_to_lunar(LunarAlgorithm.ALGO1, GregorianDate(1900, 1, 1))` passes Gregorian validation but raises
`CelestialError`, whereas `lunar_year_info(LunarAlgorithm.ALGO1, 1900)` raises `ValueError`.

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

## 中文

`celestial-calendar` 把 CelestialCalendar 的天文计算与公历/阴历转换包装为自带原生库的 Python 包，
运行时不需编译器或另行安装 CelestialCalendar。需要 Python 3.11 或更新版本；支持平台见上面的 wheel 表。

```python
import celestial_calendar as celestial

result = celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO3, celestial.GregorianDate(2026, 8, 15))
```

公开 API 使用不可变 dataclass 与枚举；日期桥接接受 `datetime.date`，不接受 `datetime.datetime`。
节气结果的 `moment_ut1` 是 UT1，不是东八区日期；阴历转换的日期基准取决于所选算法。
时间尺度、年域与错误契约见上面的 API 节。

## License

Project-authored package material is licensed under MIT. Third-party components retain their own terms
in the included `THIRD_PARTY_NOTICES.txt`; each section states where it applies. Source and issue
tracking are at <https://github.com/0xf3cd/celestial-calendar>.
