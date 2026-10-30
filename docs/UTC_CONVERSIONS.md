# UTC and JDE(TT) conversions

The C, Python and JavaScript interfaces expose the existing C++ conversions from 1.1.0 onward.
They convert time scales; adding a timezone offset is a separate display operation.

| Interface | UTC civil fields to JDE(TT) | JDE(TT) to UTC civil fields |
|---|---|---|
| C++ | `astro::julian_day::utc_to_jde(utc_dt)` | `astro::julian_day::jde_to_utc(jde_tt)` |
| C | `celestial_utc_to_jde(y, m, d, fraction)` | `celestial_jde_to_utc(jde_tt)` |
| Python | `utc_to_jde(CivilDateTime(...))` | `jde_to_utc(jde)` |
| JavaScript | `time.utcToJde(civil)` | `time.jdeToUtc(jde)` |

The C forward result is `CelestialJulianDay`; the inverse returns the independent `CelestialUTCTime`
record by value. Check `valid`, then use the preserving error-kind/message readers on failure.
Python returns a float or a frozen `CivilDateTime`. JavaScript returns a number or an owned
`CivilDateTimeResult`, including derived hour, minute and fractional second fields; call `await init()` first.
Civil records do not carry a time scale themselves: the conversion name specifies it.

## Domains and inherited limits

- Forward input uses a valid Gregorian date in years `[1, 32767]` and a finite day fraction in `[0, 1)`.
  The shifted TT date must also be representable, so the final 69.184 seconds of year 32767 are rejected.
- The inverse accepts finite JDE(TT) in `[1867522.5, 13689325.5)`. Its lower endpoint returns
  **400-12-31**, after subtracting the historical offset. Forward dates before this inverse window
  can succeed without having an admitted inverse.
- Before 1972-01-01, UTC-labelled civil fields use the **UT1 substitute with fixed Algo5**. This does
  not model historical UTC, including the variable-length seconds of 1960–1971. Chart and civil-house
  queries retain their stricter policy and reject pre-1972 UTC input.
- From 1972 onward the existing leap-second table applies. Its final row is 2017-01-01; later dates
  retain TT minus UTC = 69.184 seconds. This is a library policy, not a forecast of future UTC.
- `23:59:60` cannot be represented. The inserted-second TT interval maps into the next civil second
  and is not invertible. Julian-day rounding at leap-step midnight can also produce a one-second
  roundtrip difference.
- The historical/modern seam is about 68 ms and is non-monotone: stepping forward across the last
  millisecond of 1971 can move JDE backward. Do not assume exact roundtrips at the seam or leap steps.

Ordinary modern roundtrip tests away from these boundaries use 100 microseconds; tests near year 32767
use 200 microseconds for the coarser double-precision day count. These are fixture margins, not a
whole-domain accuracy or reversibility guarantee. See [Time scales](DESIGN.md#time-scales) for the models.

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

Both programs use the fixed input `2026-10-28T19:35:37Z` and print, to the minute:

```text
立冬
UTC: 2026-11-07 09:52
UTC+8: 2026-11-07 17:52
```

The input goes through UTC-to-JDE. Existing Jieqi queries return **UT1**, so each event goes through
UT1-to-JDE before comparison. Only the selected JDE goes through JDE-to-UTC; then the application
formats it with a fixed +8-hour offset. No chart, location, house computation or caller-owned leap table runs.

The examples enumerate both the input year and the following year and sort by moment, since the Jieqi
enum starts at Lichun, not January. This is a contemporary example, not an all-era exact TT-root API:
converting the public UT1 event back to TT can differ from the internal TT root in remote eras.
Jieqi queries accept years `[401, 32766]`; a two-year scan also needs its following year admitted.
