<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when a
     v*.*.* tag is pushed. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.6.0] - 2026-08-17

### JavaScript / TypeScript

- `@0xf3cd/celestial` packages all 29 stable C exports behind six domain namespaces: `config`, `time`, `sun`,
  `moon`, `jieqi`, and `lunar`. Import performs no I/O; one explicit `await init()` loads the package-owned WASM,
  and calculations remain synchronous afterwards.
- Hand-written TypeScript declarations keep JD/JDE/UT1, angle and distance units, and algorithm choices visible.
  Enum-like inputs are string unions; the package does not add runtime enum objects, aliases, `Date` conversion,
  or implicit number coercion.
- Input shape, finiteness, integer width, and operation domains are checked before WASM. Bad input throws
  `TypeError` / `RangeError`; native failures become `CelestialError`. Legitimate absence remains `null` or `[]`.
- The npm registry package is not yet published. Install the exact npm tarball from `celestial-wasm.zip` on the
  matching GitHub Release.

### Python

- Install a `celestial-calendar` wheel from the matching GitHub Release; wheels are not uploaded to PyPI. The
  supported targets are `manylinux_2_28` x86_64/aarch64, macOS 14 arm64, and Windows AMD64, with Python 3.11 or
  newer.
- The flat `celestial_calendar` API wraps the complete C ABI with enums, frozen dataclasses, scalars, and tuples;
  the raw ctypes layer remains private. Wrong types raise `TypeError`, invalid values raise
  `ValueError`, and native failures raise `CelestialError` with `operation` and `recorded` attributes.

### Changed

- Jieqi moment APIs now use Gregorian years in `[401, 32766]` across C++, C, Python, and JavaScript. The C++
  `jieqi_ut1_moment` rejects earlier years; the lower-level `jieqi_jde` remains available for years in
  `[1, 32766]`.

### Packaging and Verification

- The WASM module now exposes the complete 29-entry ABI and allows memory growth for the singleton's bounded
  calendar caches. Its test oracle reconciles 29 signatures, 16 struct layouts, and seven recording functions.
- The npm tarball contains exactly eight files and takes its `0.6.0` version from the C++ release SSOT. CI checks
  22 public methods, 31 edge/error cases, all 389 existing WASM goldens, Node 22 and current-Node consumers,
  TypeScript declarations, and an Astro/Vite production build in Chrome.
- `celestial-wasm.zip` keeps the raw `.mjs/.wasm` pair and adds the exact npm tarball, `npm-pack.json`, and its
  SHA-256 sidecar.
- emsdk is pinned to a commit included in the cache key. Before publishing, the release collector verifies the WASM
  archive's exact members, npm package identity, and tarball sidecar; it separately verifies each native archive's
  members, build version, and runtime-library hashes. `release_downloader.py` repeats the archive-internal checks after
  download without comparing against the caller's checkout documentation.
- Each platform clean-installs its wheel and replays the same native golden dataset as the WASM leg.
- The JavaScript acceptance suite pins the inclusive endpoints of the civil, calendar, Jieqi, longitude, ΔT, and
  lunar-algorithm domains. Directed mutations prove that the ABI oracle detects export, signature, layout, and
  recording drift; CI also rejects ABI, Node, browser, or TypeScript test entries that are not wired into their runner.
