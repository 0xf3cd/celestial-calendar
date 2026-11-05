# UTC and JDE(TT) conversions

The C, Python and JavaScript interfaces expose the existing C++ conversions from 1.1.0 onward.
They convert time scales; adding a timezone offset is a separate display operation.

| Interface | UTC civil fields to JDE(TT) | JDE(TT) to UTC civil fields |
|---|---|---|
| C++ | `astro::julian_day::utc_to_jde(utc_dt)` | `astro::julian_day::jde_to_utc(jde_tt)` |
| C | `celestial_utc_to_jde(y, m, d, fraction)` | `celestial_jde_to_utc(jde_tt)` |
| Python | `utc_to_jde(CivilDateTime(...))` | `jde_to_utc(jde)` |
| JavaScript | `time.utcToJde(civil)` | `time.jdeToUtc(jde)` |

The C forward result is `CelestialJulianDay`; the inverse returns a `CelestialUTCTime`
record by value. Check `valid`, then read `celestial_last_error_kind()` and `celestial_last_error()` on failure;
neither reader clears the recorded error.
Python returns a float or a frozen `CivilDateTime`.

JavaScript returns a number or an owned
`CivilDateTimeResult`, including derived hour, minute and fractional second fields; call `await init()` first.
Civil records do not carry a time scale themselves: the conversion name specifies it.

## Domains and inherited limits

- Forward input uses a valid Gregorian date in years `[1, 32767]` and a finite day fraction in `[0, 1)`.
  The shifted TT date must also be representable. At the upper end TT is 69.184 seconds ahead of UTC,
  so the final 69.184 seconds of year 32767 are rejected.
- The inverse accepts finite JDE(TT) in `[1867522.5, 13689325.5)`. Its lower endpoint returns
  **400-12-31**, after subtracting fixed Algo5's modelled TT minus UT1 (ΔT). Forward dates before this inverse window
  can succeed without having an admitted inverse.
- Before 1972-01-01, UTC-labelled civil fields use the **UT1 substitute with fixed Algo5**. This does
  not model historical UTC, including the variable-length seconds of 1960–1971. Chart and civil-house
  queries retain their stricter policy and reject pre-1972 UTC input.
  For coordinated broadcasts in 1961–1971, the clock-label substitution is on the order of a tenth of a second,
  separate from ΔT-model error. This is not a bound for all pre-1972 dates or for conversion accuracy;
  the historical quantities and source are in [Time scales](DESIGN.md#time-scales).
- From 1972 onward the existing leap-second table applies. Its final row is 2017-01-01; later dates
  retain TT minus UTC = 69.184 seconds. This is a library policy, not a forecast of future UTC.
- `23:59:60` cannot be represented. For example, a JDE inside the 2016-12-31 inserted second converts
  into the first civil second of 2017-01-01, labelled 00:00:00 with a possible fractional part.
  Converting that civil result back to JDE does not recover the original input.
  Separately, Julian-day rounding at a leap-step midnight can make a UTC → JDE → UTC result one second
  later than its input.
- The switch at 1972-01-01 from the UT1 substitute to the leap-second table has a gap of about 68 ms
  and is non-monotone: stepping forward across the last
  millisecond of 1971 can move JDE backward. Do not assume exact roundtrips at the seam or leap steps.

These functions do not supply a whole-domain accuracy or reversibility guarantee.
See [Time scales](DESIGN.md#time-scales) for the models and the historical reference.

Invalid civil values and non-finite/out-of-domain JDEs produce `CELESTIAL_ERROR_INVALID_INPUT` in C,
`ValueError` in Python and `RangeError` in JavaScript. Wrong binding types produce `TypeError`.
Computation/resource/internal failures and malformed native success records produce the binding's
`CelestialError`; the normal [failure contract](DESIGN.md#failures) is unchanged.

<a id="next-jieqi-from-a-utc-input-displayed-at-utc8"></a>
## Next Jieqi from a UTC input, displayed at UTC+8

Copy [the Python program](examples/utc_jieqi/example.py) or
[the JavaScript program](examples/utc_jieqi/example.mjs) into an application with the matching package installed.
Run it as `python example.py` or `node example.mjs`. For an unreleased checkout, build the
[local wheel](../bindings/python/README.md#build-a-local-wheel) or
[npm packages](BUILDING.md#wasm) first; a published 1.0 package does not have these methods.

The C++ program below uses only the source headers. Save it as `build/next_jieqi.cpp` and compile from the
source root with the [three include roots](../README.md#cpp):

```cpp
#include <format>
#include <iostream>
#include <string_view>

#include "datetime.hpp"
#include "jieqi.hpp"
#include "julian_day.hpp"
#include "ymd.hpp"

int main() {
  const calendar::Datetime cutoff_utc {
    util::to_ymd(2026, 10, 28), (19 * 3600 + 35 * 60 + 37) / 86400.0
  };
  const double cutoff_jde_tt = astro::julian_day::utc_to_jde(cutoff_utc);
  calendar::jieqi::JieqiGenerator events { cutoff_jde_tt };
  const auto [jieqi, jde_tt] = events.next();
  const auto utc = astro::julian_day::jde_to_utc(jde_tt);
  const auto east_eight = calendar::add_seconds(utc, 8 * 3600.0);

  const auto print_civil = [](const std::string_view label, const calendar::Datetime& civil) {
    std::cout << std::format(
      "{}: {:04}-{:02}-{:02} {:02}:{:02}\n",
      label,
      civil.year(),
      civil.month(),
      civil.day(),
      civil.time_of_day.hours().count(),
      civil.time_of_day.minutes().count()
    );
  };
  std::cout << calendar::jieqi::name_of(jieqi) << '\n';
  print_civil("UTC", utc);
  print_civil("UTC+8", east_eight);
}
```

```sh
clang++ -std=c++23 -I src/astro -I src/calendar -I src/util build/next_jieqi.cpp -o build/next_jieqi
./build/next_jieqi
```

The same [C++ source](examples/utc_jieqi/example.cpp) is available to copy. All three programs use the fixed
input `2026-10-28T19:35:37Z` and print with seconds omitted:

```text
立冬
UTC: 2026-11-07 09:52
UTC+8: 2026-11-07 17:52
```

The Python and JavaScript programs compare moments in JDE(TT):

1. Convert the UTC cutoff with `utc_to_jde()` or `time.utcToJde()`.
2. Convert each Jieqi's UT1 moment with `ut1_to_jde()` or `time.ut1ToJde()`.
3. Select the first JDE strictly later than the cutoff's JDE.
4. Convert that selected JDE with `jde_to_utc()` or `time.jdeToUtc()`, then add eight hours for display.

`ut1_to_jd()` / `time.ut1ToJd()` return JD(UT1), not JDE(TT); do not pass those values to JDE-to-UTC.
The programs use the library's leap table, without a chart, location or house query.

The Python and JavaScript examples enumerate both the input year and the following year and select by moment, since the Jieqi
enum starts at Lichun, not January. This is a contemporary example. The public Jieqi result is derived
from a TT instant via UT1; converting it back to TT is not guaranteed to reproduce that instant in remote eras.
Jieqi queries accept years `[401, 32766]`, so a two-year scan's first query year must be in `[401, 32765]`.
The C++ `JieqiGenerator` instead yields the first strictly later JDE(TT) directly, with no UT1 intermediate.
The year of its starting JDE must be in `[1, 32766]`; it searches by successive years. `add_seconds()` applies the display
offset to the selected UTC date, carrying across midnight if needed; it does not change the physical time scale.
