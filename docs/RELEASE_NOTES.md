<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.7.0]

### Calendar APIs

- Python now bridges standard-library dates with `GregorianDate.from_date()` and `.to_date()`.
  `gregorian_to_lunar()` also accepts date-only values; it rejects `datetime` rather than discarding its time or zone.
- JavaScript's pure `/date` entry converts `Date` values to and from civil records in UTC or at a fixed offset.
  It works without initialization or WASM, preserves milliseconds on a same-offset round trip, and does not
  convert UTC to UT1/TT or implement IANA zones and daylight-saving rules.
- JavaScript exports the 24 named `Jieqi` constants. Jieqi results identify their UT1 time scale explicitly;
  lunar documentation describes each algorithm's date basis and supported lunar-year window.

### Breaking Changes

- JavaScript `CivilDate` becomes `GregorianDate`; Gregorian, civil-moment and lunar records are distinct kinds.
  `JieqiMoment` changes from a flat record to `{ jieqi, momentUt1 }`. Derived clock fields accompany civil results,
  while inputs still use `fraction`. Use the `Jieqi` constants for typed queries.
- JavaScript Sun/Moon `apparentGeocentricCoordinates()` methods become singular `apparentGeocentricCoordinate()`.
  Python `solar_longitude_roots()` becomes `sun_longitude_crossings()`. Old names have no compatibility aliases.
- JavaScript calls before initialization now throw `CelestialError` with the public operation and `recorded = false`.
  `init()` still shares its promise, resolves to `undefined`, and preserves loader rejections and explicit retry.
- C++ rise/set parameters and result members carry UT1/TT suffixes; use `rise_jde_tt`, `transit_jde_tt` and `set_jde_tt`
  rather than the old unsuffixed members. The C ABI is unchanged.

### Packages and Documentation

- The npm alias `celestial-calendar` forwards the root and `/date` entries to the exact same-version
  `@0xf3cd/celestial`, without another WASM copy. Both names share state when they resolve to the same primary
  installation. The primary package keeps zero runtime dependencies.
- README and package guides are Chinese-first, with an English root guide. Examples cover today's lunar date,
  the next Jieqi after an explicit UT1 instant, and date round trips. Native C/C++ guidance distinguishes source
  headers from prebuilt C ABI archives; the Python guide includes a full-checkout local-wheel recipe.
- Installed consumers check enum names, default-model routing, numerical references and TypeScript declarations.
  Browser checks now cover Chrome and Firefox.

### License

- Project-authored material is now licensed under MIT. Retained third-party material keeps its source terms and remains
  outside the project MIT grant; `THIRD_PARTY_NOTICES.txt` and the source-tree attribution boundaries identify the
  applicable exceptions.
