# CelestialCalendar for Python

`celestial-calendar` provides Python bindings for CelestialCalendar's astronomical calculations, Gregorian and
Chinese Lunar conversions, and Jieqi (节气) moments. The wheel contains the native library for its target platform;
no compiler or separate CelestialCalendar installation is required at runtime. Python 3.11 or newer is supported.

## Install

Download the wheel for your platform from the matching
[GitHub release](https://github.com/0xf3cd/celestial-calendar/releases), then install the local file:

```sh
python -m pip install "./<downloaded-wheel>.whl"
```

The project publishes four wheels directly to GitHub Releases rather than PyPI:

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
print(winter_solstice)
```

The public API uses immutable dataclasses and enums. Civil moments retain a day fraction and identify their time scale
in the function or field name; they are not silently converted to Python's narrower `datetime` domain.

Wrong input types, including members of the wrong enum, raise `TypeError`. Values rejected by finiteness, range, or
domain checks raise `ValueError`. A failure reported by the native boundary raises `CelestialError`. Its `operation`
attribute names the public function, and its `recorded` attribute says whether the message came from the native error
channel. A legitimate absence remains `None` or `()`.

`jieqi_moment(year, jieqi)` accepts Gregorian years in `[401, 32766]`.
Lunar conversions use algorithm-specific year windows; query them with `supported_lunar_year_range(algorithm)`.
`moon_phase_moments(year, phase)`, `solar_longitude_roots(year, longitude_deg)`, and `new_moons_in_year(year)` accept
Gregorian years in `[1, 32766]`.

`delta_t(year, model)` accepts a finite decimal Gregorian year. Three models have additional bounds:

| Model | Year domain |
|---|---|
| `DeltaTModel.ALGO1` | `year >= -4000` |
| `DeltaTModel.ALGO3` | `year < 3000` |
| `DeltaTModel.ALGO4` | `year < 2035` |

`DeltaTModel.DEFAULT`, `DeltaTModel.ALGO2`, and `DeltaTModel.ALGO5` have no model-specific year bound.

`new_moons_after(jde, count)` accepts `count` in `[0, 4096]`; zero returns `()`. The upper bound keeps one native
output buffer at or below 32 KiB.

The project is licensed under GPL-3.0-or-later. Source and issue tracking are at
<https://github.com/0xf3cd/celestial-calendar>.
