<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions among various calendars,
    including Gregorian, Lunar, and Chinese Ganzhi calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  This project is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This project is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this project. If not, see <https://www.gnu.org/licenses/>.
-->

# Releasing CelestialCalendar

The release workflow freezes one candidate for GitHub Release, PyPI, and npm. It accepts three explicit producer
run IDs and rejects a tag, commit, run, artifact, or registry version that does not match exactly. npm v0.6.0 has
the one-time bootstrap exception described below; the workflow proves those existing bytes before continuing.

## One-Time Setup

Complete these settings before creating the release tag:

1. Add an active tag ruleset for `v*`. Test `github.ref_protected` with a disposable matching tag, then remove it.
2. Create `pypi` and `npm` environments. The maintainer is the required reviewer and may self-approve after reading
   the candidate manifest; disable self-review prevention and admin bypass. Restrict deployment tags to `v*.*.*`.
3. Register the PyPI pending publisher with owner `0xf3cd`, repository `celestial-calendar`, workflow
   `release.yml`, and environment `pypi`.
4. After npm v0.6.0 exists, register its Trusted Publisher for the same repository and workflow with environment
   `npm`.

Environment approval is a deliberate manifest check, not independent review. Confirm the tag, commit, producer
run IDs, filenames, sizes, and hashes shown in the workflow summary before approving.

## Release

1. Confirm `project.py`, `docs/RELEASE_NOTES.md`, and the first `docs/CHANGELOG.md` entry all name the same version.
2. Create and push the protected `vMAJOR.MINOR.PATCH` tag from a commit already contained in `main`.
3. Dispatch `build_and_test.yml`, `wasm.yml`, and `python-wheel.yml` on that tag. Wait for all three to succeed and
   record their run IDs. Each must be a `workflow_dispatch` run at the tagged commit.
4. Finish the remaining steps within the producer artifacts' 30-day retention window.

### npm v0.6.0 Bootstrap

npm cannot register a Trusted Publisher for a package that does not exist. Bootstrap v0.6.0 once, before running
`release.yml`:

1. Download `celestial-wasm` from the selected WASM run and use the single tarball named by `npm-pack.json`.
2. Verify that tarball with `npm-pack.sha256`.
3. Authenticate npm with a short-lived token without printing or storing it in the repository, then publish the
   exact tarball with `--access public --ignore-scripts`.
4. Revoke the token immediately and register the npm Trusted Publisher.
5. Use this same WASM run ID when dispatching `release.yml`.

From an empty `npm-bootstrap` directory, the byte-selection steps are:

```sh
gh run download <wasm-run-id> --name celestial-wasm --dir npm-bootstrap
tarball=$(python3 - <<'PY'
import json
from pathlib import Path

pack = json.loads(Path("npm-bootstrap/npm-pack.json").read_text(encoding="utf-8"))
if len(pack) != 1 or pack[0].get("name") != "@0xf3cd/celestial" or pack[0].get("version") != "0.6.0":
  raise SystemExit("unexpected npm package identity")
filename = pack[0].get("filename", "")
if Path(filename).name != filename or not (Path("npm-bootstrap") / filename).is_file():
  raise SystemExit("invalid npm tarball filename")
print(filename)
PY
)
(cd npm-bootstrap && sha256sum --check npm-pack.sha256)
npm publish "npm-bootstrap/$tarball" --access public --ignore-scripts
```

Authenticate the npm client with the short-lived token before the last command; never add the token to the command,
shell history, output, or repository.

Do not repack the module or select a tarball with a glob. npm v0.6.0 has no OIDC-generated provenance because of
this bootstrap. Its later byte-verified no-op proves package identity, not the npm OIDC publication path; the first
live OIDC publication is v0.6.1.

### Frozen Candidate

Dispatch the release workflow on the tag with the three recorded IDs:

```sh
gh workflow run release.yml --ref vMAJOR.MINOR.PATCH \
  -f native_run_id=<native-run-id> \
  -f wasm_run_id=<wasm-run-id> \
  -f python_run_id=<python-run-id>
```

Preparation validates the protected tag, main ancestry, producer runs, artifact API digests, archive contents,
and documentation. It then stages one candidate and classifies npm:

- an absent version requires OIDC publication;
- a byte-identical version is a verified no-op;
- any metadata, integrity, or byte mismatch stops before GitHub Release creation.

GitHub Release publishes first. Approve the `pypi` and `npm` jobs only after checking the candidate summary. The
final unprivileged job requires exactly four PyPI wheels and one npm tarball, compares registry hashes and bytes,
and clean-installs both packages from their public registries.

After the workflow succeeds, confirm the immutable GitHub Release and its asset inventory, then install
`celestial-calendar==VERSION` and `@0xf3cd/celestial@VERSION` from unrelated temporary directories.

## Recovery

- If immutable GitHub Release creation fails, inspect the release first. Delete it only if it is still a draft,
  then rerun failed jobs against the same workflow artifact.
- An unambiguous failure before registry acceptance may use `gh run rerun RUN_ID --failed` after reviewing the
  evidence.
- If a publish command fails ambiguously but registry queries prove the exact candidate is present, leave that
  publication job red. Do not rerun it or use `skip-existing`; record the recovery and complete consumer validation
  manually.
- After any irreversible job succeeds, never use "Re-run all jobs". The original run is the identity of the frozen
  candidate.

Rehearse environment self-approval and same-run failed-job artifact recovery after workflow changes and before the
final release tag. The v0.6.0 npm no-op cannot rehearse a live OIDC publish.
