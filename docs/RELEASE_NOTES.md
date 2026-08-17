<!-- This file is the release body: release.yml passes it verbatim as `bodyFile` when a
     v*.*.* tag is pushed. Keep ONLY the notes of the release being cut; history lives in
     CHANGELOG.md (attached to every release). -->

## [v0.6.0] - 2026-08-15

### JavaScript / TypeScript

- `@0xf3cd/celestial` packages all 29 stable C exports behind six domain namespaces: `config`, `time`, `sun`,
  `moon`, `jieqi`, and `lunar`. Import performs no I/O; one explicit `await init()` loads the package-owned WASM,
  and calculations remain synchronous afterwards.
- Hand-written TypeScript declarations keep JD/JDE/UT1, angle and distance units, and algorithm choices visible.
  Enum-like inputs are string unions; the package does not add runtime enum objects, aliases, `Date` conversion,
  or implicit number coercion.
- Input shape, finiteness, integer width, and operation domains are checked before WASM. Bad input throws
  `TypeError` / `RangeError`; native failures become `CelestialError`. Legitimate absence remains `null` or `[]`.

### Packaging and Verification

- The WASM module now exposes the complete 29-entry ABI and allows memory growth for the singleton's bounded
  calendar caches. Its test oracle reconciles 29 signatures, 16 struct layouts, and seven recording functions.
- The npm tarball contains exactly eight files and takes its `0.6.0` version from the C++ release SSOT. CI checks
  22 public methods, 31 edge/error cases, all 389 existing WASM goldens, Node 22 and current-Node consumers,
  TypeScript declarations, and an Astro/Vite production build in Chrome.
- `celestial-wasm.zip` keeps the raw `.mjs/.wasm` pair and adds the exact npm tarball, `npm-pack.json`, and its
  SHA-256 sidecar. The four native platform archives are unchanged.
