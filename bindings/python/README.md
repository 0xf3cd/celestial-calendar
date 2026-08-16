# CelestialCalendar for Python

`celestial-calendar` provides Python bindings for CelestialCalendar's astronomical calculations, Gregorian and
Chinese Lunar conversions, and Jieqi (节气) moments. The wheel contains the native library for its target platform;
no compiler or separate CelestialCalendar installation is required at runtime. Python 3.11 or newer is supported.

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

`new_moons_after(jde, count)` accepts `count` in `[0, 4096]`; zero returns `()`. The upper bound keeps one native
output buffer at or below 32 KiB.

The project is licensed under GPL-3.0-or-later. Source and issue tracking are at
<https://github.com/0xf3cd/celestial-calendar>.
