<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.6.1] - 2026-08-24

### Fixed

- Python wheels now include the PEP 561 `py.typed` marker, so mypy and other type checkers consume the annotations
  already present in the public `celestial_calendar` package instead of reporting it as an untyped dependency.

### Packaging and Verification

- Wheel verification pins the empty marker, its `RECORD` entry, and the exact member allowlist. A clean-installed mypy
  consumer checks the typed success path, a bad assignment, and the `import-untyped` failure when the marker is hidden.
