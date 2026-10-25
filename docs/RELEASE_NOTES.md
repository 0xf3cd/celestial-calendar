<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v1.0.0]

Unreleased draft. The 1.0 API and release notes are not yet frozen.

### API changes

- The C ABI uses `celestial_` exports, `Celestial` records and one `CELESTIAL_TARGET_*` code family.
  Existing 0.x binaries and source callers must migrate; no compatibility forwarders are provided.
- Independent ecliptic/equatorial positions and civil houses are available from C++ and C, with immutable Python
  result values. Positions retain caller order and provider windows without computing rates or houses.
- Model keys distinguish Delta T, nutation and position sources. Selected-model results record their resolved
  identities; capability queries describe TT target domains and the separate civil-house policy.
- Native error kinds distinguish invalid input from computation, resource and internal failures, including when
  no error message could be allocated. Automatic logging and logging configuration APIs are removed.
- Sized strings count the terminating NUL. JavaScript Sun distance is named `distanceAu`; JavaScript gains no
  public position, equatorial, house, event or capability query API.

### Calendar and civil time

- `calendar::Datetime` accepts raw durations and checks original date/time values before narrowing.
- Jieqi table schema `celestial-calendar/jieqi-table@2` stores model UT1 as `jd_ut1` / `iso_ut1`, with verified
  build provenance, instead of presenting it as UTC.
- Charts and civil houses share civil-year and UTC admission. Algo4 requires derived TT before 2035-01-01,
  excluding roughly the last 71.5 UT1 seconds of 2034. Houses do not inherit the chart's target/rate window.

See [Migrating to 1.0](MIGRATING_1_0.md) for replacement names, ownership protocols and changed failure behavior.
The [changelog](CHANGELOG.md) retains the full development history.
