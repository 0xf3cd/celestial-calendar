<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when it is
     dispatched on a v*.*.* tag. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.6.1] - 2026-08-24

### Fixed

- Python wheels now include the PEP 561 `py.typed` marker, so mypy and other type checkers consume the annotations
  already present in the public `celestial_calendar` package instead of reporting it as an untyped dependency.

### Packaging and Verification

- Wheel verification pins `py.typed` in the exact member allowlist, and the clean-installed consumer suite checks that
  the marker is discoverable through `importlib.resources`.
