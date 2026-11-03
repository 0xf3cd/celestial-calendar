<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v1.1.0]

### Compatibility

- Prebuilt macOS arm64 libraries and Python wheels require macOS 15 or newer, up from macOS 14.
  Other platform minimums are unchanged.
- Existing public API/ABI layouts and signatures remain compatible. No astronomical model, numerical tolerance,
  calculation domain or historical time policy changes in this release.

### API and documentation

- UTC civil time ↔ JDE(TT) conversions are available in C (`celestial_utc_to_jde`, `celestial_jde_to_utc`),
  Python (`utc_to_jde`, `jde_to_utc`) and JavaScript (`time.utcToJde`, `time.jdeToUtc`). They expose the existing
  C++ conversions, including their historical UT1 substitute and leap-second policy.
- Quickstarts and task guides include runnable Python and JavaScript examples. Model and domain explanations
  have one shared guide; hosted documentation identifies its source revision and release/development state.
- An optional [consumer skill](https://github.com/0xf3cd/celestial-calendar/tree/main/skills/using-celestial-calendar)
  provides public-guide links and a checklist for interface availability, time scales, units and program execution.

### Maintenance

- Share chart-replay tolerances and native-library discovery, remove unused maintenance paths, and keep
  non-Linux export-surface checks on their static path.

The [changelog](CHANGELOG.md) retains the full development history.
