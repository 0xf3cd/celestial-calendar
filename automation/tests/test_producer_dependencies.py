# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import json
import io
import tarfile
from types import SimpleNamespace
import re
import tomllib
from pathlib import Path

import pytest
import yaml

from toolbox.build_npm import (
  ALIAS_ALLOWLIST,
  ALIAS_FILES,
  ALIAS_NAME,
  ALIAS_SOURCE,
  NPM_METADATA,
  PACKAGE_NAME,
  PACKAGE_FILES,
  PACKAGE_SOURCE,
  PACK_ALLOWLIST,
  WASM_ARTIFACT_ALLOWLIST,
  WASM_ARTIFACT_FILES,
  staging_manifest,
  verify_manifest,
)
from toolbox.release_validation import SOURCE_WORKFLOWS
from toolbox import build_npm


REPO = Path(__file__).parents[2]
BUILD_WORKFLOW = REPO / ".github" / "workflows" / "build_and_test.yml"
WHEEL_WORKFLOW = REPO / ".github" / "workflows" / "python-wheel.yml"
RELEASE_WORKFLOW = REPO / ".github" / "workflows" / "release.yml"
DOCKERFILE = REPO / "Dockerfile"
NATIVE_CMAKE = REPO / "src" / "shared_lib" / "CMakeLists.txt"
PYTHON_CMAKE = REPO / "bindings" / "python" / "CMakeLists.txt"
WHEEL_VERIFY = REPO / "bindings" / "python" / "test" / "wheel" / "verify.py"
CIBW_CONSTRAINTS = REPO / "bindings" / "python" / "constraints-cibuildwheel.txt"
CIBW_LOCK = REPO / "bindings" / "python" / "requirements-cibuildwheel.txt"
CIBW_LOCK_INPUT = REPO / "bindings" / "python" / "requirements-cibuildwheel.in"
BUILD_LOCK_INPUT = REPO / "bindings" / "python" / "requirements-build.in"
PYPROJECT = REPO / "bindings" / "python" / "pyproject.toml"
LOCK_INPUTS = {
  REPO / "Requirements-producer.txt": (REPO / "Requirements-producer.in", "3.12"),
  REPO / "bindings" / "python" / "requirements-host.txt": (
    REPO / "bindings" / "python" / "requirements-host.in",
    "3.14",
  ),
  REPO / "bindings" / "python" / "requirements-build.txt": (BUILD_LOCK_INPUT, "3.11"),
  CIBW_LOCK: (CIBW_LOCK_INPUT, "3.11"),
  REPO / "bindings" / "python" / "requirements-mypy.txt": (
    REPO / "bindings" / "python" / "requirements-mypy.in",
    "3.14",
  ),
}
LOCK_NAMES = {path.relative_to(REPO).as_posix() for path in LOCK_INPUTS}
LOCK_REFERENCES = LOCK_NAMES | {path.name for path in LOCK_INPUTS}
REQUIREMENT_RE = re.compile(r"^([A-Za-z0-9][A-Za-z0-9_.-]*)==([^\s;\\]+)")
HASH_RE = re.compile(r"--hash=sha256:([0-9a-f]{64})(?:\s|$)")
PIP_WORD_PATTERN = r"""(?:[^\s"'\\]|\\.|"(?:\\.|[^"\\])*"|'[^']*')+"""
PIP_INSTALL_RE = re.compile(
  r"(?:^|[/\\\s])pip(?:3(?:\.\d+)?)?(?:\.exe)?"
  rf"(?:\s+-{PIP_WORD_PATTERN}(?:\s+(?!install(?:\s|$)|-){PIP_WORD_PATTERN})?)*\s+install(?:\s|$)"
)
LOCAL_WHEEL_INSTALL_RE = re.compile(
  r'^\S+ -m pip install --no-deps "(?:/wheels/\$WHEEL_FILENAME|\$WHEEL|\$env:WHEEL)"$'
)
REGISTRY_DEPENDENCY_INSTALL = "python3 -m pip install --require-hashes --only-binary=:all: -r Requirements-producer.txt"
PYPI_CONSUMER_INSTALL = (
  "registry-venv/bin/python -m pip --isolated install "
  "--index-url https://pypi.org/simple --only-binary=:all: --no-cache-dir --no-deps "
  '"celestial-calendar==${TAG_NAME#v}"'
)
QUOTED_PIP_INSTALLS = (
  'pip --log "path with spaces.log" install requests==2.34.2',
  "pip --log 'path with spaces.log' install requests==2.34.2",
  'pip --log="path with spaces.log" install requests==2.34.2',
  "pip --log='path with spaces.log' install requests==2.34.2",
  'python3 -m pip --python "path with spaces/bin/python" install requests==2.34.2',
  "python3 -m pip --python 'path with spaces/bin/python' install requests==2.34.2",
  'python3 -m pip --python="path with spaces/bin/python" install requests==2.34.2',
  "python3 -m pip --python='path with spaces/bin/python' install requests==2.34.2",
)
TOP_LEVEL_CIBW_KEYS = {
  "CIBW_BEFORE_BUILD",
  "CIBW_BUILD_FRONTEND",
  "CIBW_BUILD_VERBOSITY",
  "CIBW_DEPENDENCY_VERSIONS",
  "CIBW_ENVIRONMENT_MACOS",
  "CIBW_ENVIRONMENT_WINDOWS",
}
JOB_CIBW_KEYS = {
  "manylinux": {"CIBW_ENVIRONMENT_LINUX"},
  "macos-arm64": {"CIBW_ARCHS_MACOS"},
  "windows-amd64": {"CIBW_ARCHS_WINDOWS"},
}


def requirement_pins(path):
  return {
    match.group(1).lower().replace("_", "-"): match.group(2)
    for line in source_requirements(path)
    if (match := REQUIREMENT_RE.match(line))
  }


def source_requirements(path):
  return [line for line in path.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")]


def assert_complete_hash_lock(path, python_version):
  text = path.read_text(encoding="utf-8")
  requirement_lines = [
    line for line in text.splitlines() if line and not line.startswith("#") and not line[0].isspace()
  ]
  starts = list(re.finditer(r"(?m)^[A-Za-z0-9][A-Za-z0-9_.-]*==", text))
  assert starts
  assert all(REQUIREMENT_RE.match(line) for line in requirement_lines)
  assert "uvx --from uv==0.12.5 uv pip compile" in text
  assert "--generate-hashes" in text
  assert "--universal" in text
  assert f"--python-version {python_version}" in text
  assert "--index-url" not in text and "https://pypi.org" not in text

  for index, start in enumerate(starts):
    end = starts[index + 1].start() if index + 1 < len(starts) else len(text)
    block = text[start.start() : end]
    lines = block.splitlines()
    hash_lines = [line for line in lines if "--hash=" in line]
    assert REQUIREMENT_RE.match(block)
    assert hash_lines and HASH_RE.search(block)
    assert "# via" in block
    assert lines[0].endswith("\\")
    assert all(line.endswith("\\") for line in hash_lines[:-1])
    assert not hash_lines[-1].endswith("\\")


def pip_install_lines(command):
  command = re.sub(r"\\\r?\n", "", command)
  return [
    line.strip() for line in command.splitlines() if not line.lstrip().startswith("#") and PIP_INSTALL_RE.search(line)
  ]


def assert_hash_locked_install(line):
  if "--no-deps" in line:
    assert LOCAL_WHEEL_INSTALL_RE.fullmatch(line)
    return

  assert "--require-hashes" in line
  assert re.search(r"--only-binary(?:=|\s+):all:", line)
  assert any(name in line for name in LOCK_REFERENCES)


def workflow_install_lines(path, job_name=None):
  workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
  jobs = list(workflow["jobs"].values()) if job_name is None else [workflow["jobs"][job_name]]
  lines = [line for job in jobs for step in job["steps"] for line in pip_install_lines(str(step.get("run", "")))]
  lines += [
    line
    for env in [workflow.get("env", {})]
    + [job.get("env", {}) for job in jobs]
    + [step.get("env", {}) for job in jobs for step in job["steps"]]
    for value in env.values()
    for line in pip_install_lines(str(value))
  ]
  return lines


def release_candidate_install_lines(path):
  workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
  job = workflow["jobs"]["prepare_release"]
  steps = job["steps"]
  upload_index = next(
    index
    for index, step in enumerate(steps)
    if str(step.get("uses", "")).startswith("actions/upload-artifact@")
    and step.get("with", {}).get("name") == "celestial-release-candidate"
  )
  producer_steps = steps[:upload_index]
  lines = [line for step in producer_steps for line in pip_install_lines(str(step.get("run", "")))]
  lines += [
    line
    for env in [workflow.get("env", {}), job.get("env", {})] + [step.get("env", {}) for step in producer_steps]
    for value in env.values()
    for line in pip_install_lines(str(value))
  ]
  return lines


def test_producer_lock_files_pin_every_requirement_with_hashes():
  for lock, (source, python_version) in LOCK_INPUTS.items():
    assert all(REQUIREMENT_RE.match(line) for line in source_requirements(source))
    assert_complete_hash_lock(lock, python_version)
    assert requirement_pins(source).items() <= requirement_pins(lock).items()


def test_release_staging_lock_contains_only_requests_closure():
  assert source_requirements(REPO / "Requirements-producer.in") == ["requests==2.34.2"]
  assert set(requirement_pins(REPO / "Requirements-producer.txt")) == {
    "certifi",
    "charset-normalizer",
    "idna",
    "requests",
    "urllib3",
  }


def test_build_lock_input_pins_every_pyproject_backend_requirement():
  requirements = tomllib.loads(PYPROJECT.read_text(encoding="utf-8"))["build-system"]["requires"]
  backend_pins = {
    match.group(1).lower().replace("_", "-"): match.group(2)
    for requirement in requirements
    if (match := REQUIREMENT_RE.match(requirement))
  }

  assert len(backend_pins) == len(requirements)
  assert backend_pins.items() <= requirement_pins(BUILD_LOCK_INPUT).items()


def test_cibuildwheel_constraints_and_lock_pin_bootstrap_pip():
  lines = [
    line for line in CIBW_CONSTRAINTS.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")
  ]
  assert lines == [f"-c {CIBW_LOCK.name}", "pip==26.2"]
  assert requirement_pins(CIBW_LOCK_INPUT)["pip"] == "26.2"


@pytest.mark.parametrize(
  ("old", "new"),
  [
    pytest.param(
      "".join(
        [
          "example==1.0 \\\n",
          f"    --hash=sha256:{'0' * 64} \\\n",
          f"    --hash=sha256:{'1' * 64}\n",
          "    # via source.in\n",
        ]
      ),
      "",
      id="no-requirement-block",
    ),
    pytest.param("uvx --from uv==0.12.5 uv pip compile", "uv pip compile", id="wrong-command"),
    pytest.param(" --generate-hashes", "", id="no-generate-hashes"),
    pytest.param(" --universal", "", id="no-universal"),
    pytest.param("--python-version 3.12", "--python-version 3.11", id="wrong-python-version"),
    pytest.param(
      "uv pip compile",
      "uv pip compile --index-url https://pypi.org/simple",
      id="index-url",
    ),
    pytest.param("example==1.0 \\", "example== \\", id="malformed-requirement"),
    pytest.param(
      "    # via source.in\n",
      "    # via source.in\nextra>=1.0\n",
      id="non-exact-requirement",
    ),
    pytest.param(
      f"    --hash=sha256:{'0' * 64} \\\n    --hash=sha256:{'1' * 64}\n",
      "",
      id="no-hash",
    ),
    pytest.param("    # via source.in\n", "", id="no-via"),
    pytest.param("example==1.0 \\", "example==1.0", id="no-requirement-continuation"),
    pytest.param(
      f"    --hash=sha256:{'0' * 64} \\",
      f"    --hash=sha256:{'0' * 64}",
      id="no-intermediate-hash-continuation",
    ),
    pytest.param(
      f"    --hash=sha256:{'1' * 64}\n",
      f"    --hash=sha256:{'1' * 64} \\\n",
      id="final-hash-continuation",
    ),
  ],
)
def test_complete_hash_lock_gate_rejects_invalid_fixtures(tmp_path, old, new):
  lock = tmp_path / "requirements.txt"
  complete = (
    "# This file was autogenerated by uv via the following command:\n"
    "# uvx --from uv==0.12.5 uv pip compile source.in --generate-hashes --universal "
    "--python-version 3.12 --output-file requirements.txt\n"
    "example==1.0 \\\n"
    f"    --hash=sha256:{'0' * 64} \\\n"
    f"    --hash=sha256:{'1' * 64}\n"
    "    # via source.in\n"
  )
  lock.write_text(complete, encoding="utf-8")
  assert_complete_hash_lock(lock, "3.12")
  lock.write_text(complete.replace(old, new), encoding="utf-8")

  with pytest.raises(AssertionError):
    assert_complete_hash_lock(lock, "3.12")


def test_producer_install_gate_reads_every_workflow_env_scope(tmp_path):
  workflow = tmp_path / "workflow.yml"
  workflow.write_text(
    yaml.safe_dump(
      {
        "env": {"TOP_INSTALL": "python -m pip install top==1"},
        "jobs": {
          "fixture": {
            "env": {"JOB_INSTALL": "python -m pip install job==1"},
            "steps": [{"env": {"STEP_INSTALL": "python -m pip install step==1"}}],
          }
        },
      }
    ),
    encoding="utf-8",
  )

  assert set(workflow_install_lines(workflow)) == {
    "python -m pip install top==1",
    "python -m pip install job==1",
    "python -m pip install step==1",
  }


def test_release_candidate_install_gate_honors_upload_boundary_and_env_scopes(tmp_path):
  workflow = tmp_path / "release.yml"
  workflow.write_text(
    yaml.safe_dump(
      {
        "env": {"WORKFLOW_INSTALL": "python -m pip install workflow==1"},
        "jobs": {
          "prepare_release": {
            "env": {"JOB_INSTALL": "python -m pip install job==1"},
            "steps": [
              {"run": "python -m pip install run==1"},
              {
                "uses": "actions/upload-artifact@digest",
                "with": {"name": "diagnostics"},
              },
              {"env": {"STEP_INSTALL": "python -m pip install step==1"}},
              {
                "uses": "actions/upload-artifact@digest",
                "with": {"name": "celestial-release-candidate"},
              },
              {
                "run": "python -m pip install after==1",
                "env": {"AFTER_INSTALL": "python -m pip install after-env==1"},
              },
            ],
          }
        },
      }
    ),
    encoding="utf-8",
  )

  assert set(release_candidate_install_lines(workflow)) == {
    "python -m pip install workflow==1",
    "python -m pip install job==1",
    "python -m pip install run==1",
    "python -m pip install step==1",
  }


def test_every_explicit_producer_pip_install_is_hash_locked():
  producer_workflows = []
  remaining = set(SOURCE_WORKFLOWS)
  for path in (REPO / ".github" / "workflows").glob("*.yml"):
    name = yaml.safe_load(path.read_text(encoding="utf-8")).get("name")
    if name in remaining:
      producer_workflows.append(path)
      remaining.remove(name)

  assert not remaining
  lines = [line for path in producer_workflows for line in workflow_install_lines(path)]
  lines += release_candidate_install_lines(RELEASE_WORKFLOW)
  lines += pip_install_lines(DOCKERFILE.read_text(encoding="utf-8"))

  assert lines
  assert all("--upgrade pip" not in line for line in lines)
  for line in lines:
    assert_hash_locked_install(line)


def test_wheel_build_configuration_uses_only_hash_locked_dependency_paths():
  workflow = yaml.safe_load(WHEEL_WORKFLOW.read_text(encoding="utf-8"))
  env = workflow["env"]
  assert {key for key in env if key.startswith("CIBW_")} == TOP_LEVEL_CIBW_KEYS
  assert env["CIBW_BUILD_FRONTEND"] == "build; args: --no-isolation"
  before_build = env["CIBW_BEFORE_BUILD"]
  assert "--require-hashes" in before_build
  assert "--only-binary :all:" in before_build
  assert "{package}/requirements-build.txt" in before_build
  assert env["CIBW_DEPENDENCY_VERSIONS"] == "bindings/python/constraints-cibuildwheel.txt"
  for platform in ("MACOS", "WINDOWS"):
    target_env = env[f"CIBW_ENVIRONMENT_{platform}"]
    assert "PIP_REQUIRE_HASHES=1" in target_env
    assert "PIP_ONLY_BINARY=:all:" in target_env
  assert "CIBW_TEST_COMMAND" not in env

  for job_name, job in workflow["jobs"].items():
    job_keys = {key for key in job.get("env", {}) if key.startswith("CIBW_")}
    assert job_keys == JOB_CIBW_KEYS.get(job_name, set())
    for step in job["steps"]:
      assert not {key for key in step.get("env", {}) if key.startswith("CIBW_")}


def test_native_producers_do_not_run_unlocked_project_setup():
  commands = BUILD_WORKFLOW.read_text(encoding="utf-8") + DOCKERFILE.read_text(encoding="utf-8")
  project_lines = [line for line in commands.splitlines() if "project.py" in line and not line.lstrip().startswith("#")]
  build_commands = [line for line in project_lines if "--build" in line]

  assert project_lines and build_commands
  assert all("--setup" not in line and "--all" not in line for line in project_lines)
  assert all({"--clean", "--cmake", "--build", "--test"} <= set(line.split()) for line in build_commands)


def test_native_producers_install_no_python_dependencies():
  assert workflow_install_lines(BUILD_WORKFLOW) == []
  assert pip_install_lines(DOCKERFILE.read_text(encoding="utf-8")) == []


def test_native_producers_install_and_guard_canonical_notices():
  cmake = NATIVE_CMAKE.read_text(encoding="utf-8")
  cmake = re.sub(r"#\[(=*)\[.*?\]\1\]", "", cmake, flags=re.DOTALL)
  cmake_lines = {line.strip() for line in cmake.splitlines() if not line.lstrip().startswith("#")}
  workflow = BUILD_WORKFLOW.read_text(encoding="utf-8")

  assert 'install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE" DESTINATION .)' in cmake_lines
  assert 'install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../THIRD_PARTY_NOTICES.txt" DESTINATION .)' in cmake_lines
  assert '[ -f "$DEST_DIR/LICENSE" ] || { echo "missing LICENSE"; ok=0; }' in workflow
  assert '[ -f "$DEST_DIR/THIRD_PARTY_NOTICES.txt" ] || { echo "missing THIRD_PARTY_NOTICES.txt"; ok=0; }' in workflow
  assert '[ -f "./macos_arm64/LICENSE" ] || { echo "missing LICENSE"; ok=0; }' in workflow
  assert (
    '[ -f "./macos_arm64/THIRD_PARTY_NOTICES.txt" ] || { echo "missing THIRD_PARTY_NOTICES.txt"; ok=0; }' in workflow
  )
  assert 'if (!(Test-Path "$destDir/LICENSE")) { Write-Output "missing LICENSE"; $ok = $false }' in workflow
  assert (
    'if (!(Test-Path "$destDir/THIRD_PARTY_NOTICES.txt")) '
    '{ Write-Output "missing THIRD_PARTY_NOTICES.txt"; $ok = $false }' in workflow
  )


def test_package_producers_include_the_canonical_notice():
  license_file = REPO / "LICENSE"
  notice = REPO / "THIRD_PARTY_NOTICES.txt"
  python_cmake = PYTHON_CMAKE.read_text(encoding="utf-8")

  assert '"${REPO_ROOT}/THIRD_PARTY_NOTICES.txt"' in python_cmake
  assert PACKAGE_FILES[notice] == "THIRD_PARTY_NOTICES.txt"
  assert len(PACKAGE_FILES) == len(set(PACKAGE_FILES.values()))
  assert {"package.json", *PACKAGE_FILES.values()} == PACK_ALLOWLIST
  assert WASM_ARTIFACT_FILES[license_file] == "LICENSE"
  assert WASM_ARTIFACT_FILES[notice] == "THIRD_PARTY_NOTICES.txt"
  assert len(WASM_ARTIFACT_FILES) == len(set(WASM_ARTIFACT_FILES.values()))
  assert set(WASM_ARTIFACT_FILES.values()) == WASM_ARTIFACT_ALLOWLIST


def test_npm_date_subpath_inventory_and_exports():
  source = json.loads((PACKAGE_SOURCE / "package.json").read_text(encoding="utf-8"))
  assert source["exports"] == {
    ".": {
      "types": "./types/index.d.ts",
      "import": "./src/index.mjs",
      "default": "./src/index.mjs",
    },
    "./date": {
      "types": "./types/date.d.ts",
      "import": "./src/date.mjs",
      "default": "./src/date.mjs",
    },
  }
  assert set(source["files"]) == {
    path.relative_to(PACKAGE_SOURCE).as_posix() for path in PACKAGE_FILES if path.is_relative_to(PACKAGE_SOURCE)
  }
  assert PACK_ALLOWLIST == {
    "package.json",
    "README.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.txt",
    "index.mjs",
    "bindings.mjs",
    "validation.mjs",
    "date.mjs",
    "index.d.ts",
    "date.d.ts",
    "celestial-jieqi.mjs",
    "celestial-jieqi.wasm",
  }
  assert len(WASM_ARTIFACT_ALLOWLIST) + 3 * len(NPM_METADATA) == 10
  verify_manifest(staging_manifest("0.7.0"), "0.7.0")


@pytest.mark.parametrize("mutation", ["date-export", "date-file"])
def test_npm_manifest_rejects_date_surface_mutations(mutation):
  manifest = staging_manifest("0.7.0")
  verify_manifest(manifest, "0.7.0")
  if mutation == "date-export":
    manifest["exports"]["./date"]["import"] = "./index.mjs"
  else:
    manifest["files"].remove("date.mjs")

  with pytest.raises(RuntimeError, match=r"staging package (exports|files) mismatch"):
    verify_manifest(manifest, "0.7.0")


@pytest.mark.parametrize("key", ["dependencies", "optionalDependencies", "peerDependencies"])
@pytest.mark.parametrize("dependencies", [None, {}, {"unexpected-runtime-package": "1.0.0"}])
def test_npm_manifest_runtime_dependency_fields(tmp_path, monkeypatch, key, dependencies):
  manifest = staging_manifest("0.7.0")
  manifest[key] = dependencies
  source = json.loads((PACKAGE_SOURCE / "package.json").read_text(encoding="utf-8"))
  source[key] = dependencies
  (tmp_path / "package.json").write_text(json.dumps(source), encoding="utf-8")
  monkeypatch.setattr("toolbox.build_npm.PACKAGE_SOURCE", tmp_path)

  if dependencies:
    with pytest.raises(RuntimeError, match="zero runtime dependencies"):
      staging_manifest("0.7.0")
    with pytest.raises(RuntimeError, match="zero runtime dependencies"):
      verify_manifest(manifest, "0.7.0")
  else:
    verify_manifest(staging_manifest("0.7.0"), "0.7.0")
    verify_manifest(manifest, "0.7.0")


def test_producers_verify_notice_bytes():
  workflow = BUILD_WORKFLOW.read_text(encoding="utf-8")
  wheel = WHEEL_VERIFY.read_text(encoding="utf-8")
  npm = (REPO / "toolbox" / "build_npm.py").read_text(encoding="utf-8")

  assert 'cmp -s "$DEST_DIR/THIRD_PARTY_NOTICES.txt" "$ROOT_DIR/THIRD_PARTY_NOTICES.txt"' in workflow
  assert 'cmp -s "./macos_arm64/THIRD_PARTY_NOTICES.txt" "./THIRD_PARTY_NOTICES.txt"' in workflow
  assert (
    '(Get-FileHash "$destDir/THIRD_PARTY_NOTICES.txt" -Algorithm SHA256).Hash -ne '
    '(Get-FileHash "$ROOT_DIR/THIRD_PARTY_NOTICES.txt" -Algorithm SHA256).Hash'
  ) in workflow
  assert (
    'archive.read(f"{dist_info}/licenses/THIRD_PARTY_NOTICES.txt") == (REPO / "THIRD_PARTY_NOTICES.txt").read_bytes()'
  ) in " ".join(wheel.split())
  assert 'member.name == "package/THIRD_PARTY_NOTICES.txt"' in npm
  assert 'notice.read() != (PROJ_ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes()' in npm
  assert (
    '(artifact_dir / "THIRD_PARTY_NOTICES.txt").read_bytes() != (PROJ_ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes()'
  ) in " ".join(npm.split())


def test_readme_describes_the_current_npm_and_wasm_members():
  readme = (REPO / "README.md").read_text(encoding="utf-8")
  wasm_section = readme.split("## 6.", maxsplit=1)[1].split("## 7.", maxsplit=1)[0]

  assert f"exact {len(PACK_ALLOWLIST)}-file npm tarball" in wasm_section
  assert f"contains exactly {len(WASM_ARTIFACT_ALLOWLIST) + 3 * len(NPM_METADATA)} top-level files" in wasm_section
  assert f"{len(ALIAS_ALLOWLIST)}-file alias tarball" in wasm_section
  assert all(f"`{member}`" in wasm_section for member in WASM_ARTIFACT_ALLOWLIST)
  assert "the exact npm tarball, `npm-pack.json`, and `npm-pack.sha256`" in wasm_section
  assert "`npm-alias-pack.json`" in wasm_section and "`npm-alias-pack.sha256`" in wasm_section


def test_npm_alias_source_and_staging_contract():
  source = json.loads((ALIAS_SOURCE / "package.json").read_text(encoding="utf-8"))
  assert source["private"] is True and source["version"] == "0.0.0-development"
  assert "devDependencies" not in source
  assert source["dependencies"] == {PACKAGE_NAME: "0.0.0-development"}
  assert ALIAS_ALLOWLIST == {"package.json", "LICENSE", "README.md", "index.mjs", "index.d.ts", "date.mjs", "date.d.ts"}
  assert {"package.json", *ALIAS_FILES.values()} == ALIAS_ALLOWLIST
  assert ALIAS_FILES[REPO / "LICENSE"] == "LICENSE"
  for entry in ("index", "date"):
    target = PACKAGE_NAME + ("/date" if entry == "date" else "")
    for extension in ("mjs", "d.ts"):
      text = (ALIAS_SOURCE / f"{entry}.{extension}").read_text(encoding="utf-8")
      assert text.split("*/", 1)[1].strip() == f'export * from "{target}";'
  manifest = staging_manifest("0.7.0", ALIAS_NAME)
  verify_manifest(manifest, "0.7.0", ALIAS_NAME)
  assert manifest["dependencies"] == {PACKAGE_NAME: "0.7.0"}


@pytest.mark.parametrize(
  "key,value",
  [
    ("dependencies", None),
    ("dependencies", {}),
    ("dependencies", {PACKAGE_NAME: "^0.7.0"}),
    ("dependencies", {PACKAGE_NAME: "0.7.1"}),
    ("dependencies", {PACKAGE_NAME: "0.7.0", "extra": "1.0.0"}),
    ("optionalDependencies", {PACKAGE_NAME: "0.7.0"}),
    ("peerDependencies", {PACKAGE_NAME: "0.7.0"}),
  ],
)
def test_alias_rejects_wrong_dependency_graph(tmp_path, monkeypatch, key, value):
  source = json.loads((ALIAS_SOURCE / "package.json").read_text(encoding="utf-8"))
  manifest = staging_manifest("0.7.0", ALIAS_NAME)
  source[key] = value
  manifest[key] = value
  (tmp_path / "package.json").write_text(json.dumps(source), encoding="utf-8")
  monkeypatch.setattr("toolbox.build_npm.ALIAS_SOURCE", tmp_path)
  with pytest.raises(RuntimeError, match="exact same-version"):
    staging_manifest("0.7.0", ALIAS_NAME)
  with pytest.raises(RuntimeError, match="exact same-version"):
    verify_manifest(manifest, "0.7.0", ALIAS_NAME)


@pytest.mark.parametrize("job_name", ["publish_npm", "verify_registries"])
def test_registry_classifier_and_verifier_installs_are_hash_locked(job_name):
  lines = [" ".join(line.split()) for line in workflow_install_lines(RELEASE_WORKFLOW, job_name)]
  expected = [REGISTRY_DEPENDENCY_INSTALL]
  if job_name == "verify_registries":
    expected.append(PYPI_CONSUMER_INSTALL)
  assert sorted(lines) == sorted(expected)
  for line in lines:
    if job_name == "verify_registries" and line == PYPI_CONSUMER_INSTALL:
      continue
    assert_hash_locked_install(line)


@pytest.fixture
def registry_install_workflow(tmp_path, monkeypatch, job_name):
  steps = [{"run": REGISTRY_DEPENDENCY_INSTALL}]
  if job_name == "verify_registries":
    steps.append({"run": PYPI_CONSUMER_INSTALL})
  workflow = {"jobs": {job_name: {"steps": steps}}}
  path = tmp_path / "release.yml"
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  monkeypatch.setattr(f"{__name__}.RELEASE_WORKFLOW", path)
  test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)
  return path, workflow


@pytest.mark.parametrize("job_name", ["publish_npm", "verify_registries"])
@pytest.mark.parametrize("scope", ["run", "workflow-env", "job-env", "step-env", "same-run"])
@pytest.mark.parametrize(
  "command",
  [
    "python3 -m pip install requests==2.34.2",
    "python3 -m pip --isolated install requests==2.34.2",
    "pip3 --timeout 30 --no-input install requests==2.34.2",
    *QUOTED_PIP_INSTALLS,
  ],
)
def test_registry_install_gate_rejects_unlocked_siblings(registry_install_workflow, job_name, scope, command):
  path, workflow = registry_install_workflow
  job = workflow["jobs"][job_name]
  if scope == "run":
    job["steps"].append({"run": command})
  elif scope == "workflow-env":
    workflow["env"] = {"EXTRA_INSTALL": command}
  elif scope == "job-env":
    job["env"] = {"EXTRA_INSTALL": command}
  elif scope == "same-run":
    job["steps"][0]["run"] += "\n" + command
  else:
    job["steps"].append({"env": {"EXTRA_INSTALL": command}})
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  with pytest.raises(AssertionError):
    test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)


@pytest.mark.parametrize("job_name", ["publish_npm", "verify_registries"])
@pytest.mark.parametrize(
  "old,new",
  [
    (" --require-hashes", ""),
    (" --only-binary=:all:", ""),
    (" --only-binary=:all:", " --only-binary=requests"),
    (" -r Requirements-producer.txt", " requests==2.34.2"),
    ("Requirements-producer.txt", "Requirements.txt"),
    ("Requirements-producer.txt", "bindings/python/requirements-host.txt"),
  ],
)
def test_registry_install_gate_rejects_changed_bootstrap(registry_install_workflow, job_name, old, new):
  path, workflow = registry_install_workflow
  workflow["jobs"][job_name]["steps"][0]["run"] = REGISTRY_DEPENDENCY_INSTALL.replace(old, new)
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  with pytest.raises(AssertionError):
    test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)


@pytest.mark.parametrize("job_name", ["verify_registries"])
@pytest.mark.parametrize(
  "old,new",
  [
    ('"celestial-calendar==${TAG_NAME#v}"', '"celestial-calendar==${TAG_NAME#v}" requests==2.34.2'),
    ("celestial-calendar==${TAG_NAME#v}", "requests==2.34.2"),
    ("celestial-calendar==${TAG_NAME#v}", "celestial-calendar>=0.7.0"),
    ("celestial-calendar==${TAG_NAME#v}", "celestial-calendar"),
    (" --isolated", ""),
    (" --no-deps", ""),
    (" --only-binary=:all:", ""),
    (" --no-cache-dir", ""),
    ("https://pypi.org/simple", "https://example.invalid/simple"),
  ],
)
def test_registry_install_gate_rejects_changed_consumer(registry_install_workflow, job_name, old, new):
  path, workflow = registry_install_workflow
  workflow["jobs"][job_name]["steps"][1]["run"] = PYPI_CONSUMER_INSTALL.replace(old, new)
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  with pytest.raises(AssertionError):
    test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)


@pytest.mark.parametrize("job_name", ["verify_registries"])
@pytest.mark.parametrize("scope", ["run", "env"])
@pytest.mark.parametrize(
  "command",
  [
    "python3 -m pip install --no-deps requests==2.34.2",
    'python -m pip install --no-deps "$WHEEL"',
  ],
)
def test_registry_consumer_exception_does_not_exempt_its_step(registry_install_workflow, job_name, scope, command):
  path, workflow = registry_install_workflow
  step = workflow["jobs"][job_name]["steps"][1]
  if scope == "run":
    step["run"] += "\n" + command
  else:
    step["env"] = {"EXTRA_INSTALL": command}
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  with pytest.raises(AssertionError):
    test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)


@pytest.mark.parametrize("job_name", ["publish_npm"])
def test_pypi_consumer_exception_is_not_allowed_in_publisher(registry_install_workflow, job_name):
  path, workflow = registry_install_workflow
  workflow["jobs"][job_name]["steps"].append({"run": PYPI_CONSUMER_INSTALL})
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  with pytest.raises(AssertionError):
    test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)


@pytest.mark.parametrize("job_name", ["publish_npm", "verify_registries"])
def test_registry_install_gate_accepts_private_multiline_yaml(registry_install_workflow, job_name):
  path, workflow = registry_install_workflow
  for step in workflow["jobs"][job_name]["steps"]:
    step["run"] = step["run"].replace(" install ", " install \\\n    ")
  path.write_text(yaml.safe_dump(workflow), encoding="utf-8")
  test_registry_classifier_and_verifier_installs_are_hash_locked(job_name)


@pytest.mark.parametrize(
  "command",
  [
    "python3 -m pip --isolated install requests==2.34.2",
    "pip3 --disable-pip-version-check install requests==2.34.2",
    "/tools/pip3.12 --timeout 30 --no-input install requests==2.34.2",
    "pip.exe -q --retries=2 install requests==2.34.2",
    "python3 -m pip --isolated \\\n  install \\\n  requests==2.34.2",
    *QUOTED_PIP_INSTALLS,
  ],
)
def test_install_scanner_recognizes_pip_options_and_continuations(command):
  lines = pip_install_lines(command)
  assert len(lines) == 1
  assert " ".join(lines[0].split()) == " ".join(command.replace("\\\n", "").split())
  with pytest.raises(AssertionError):
    assert_hash_locked_install(lines[0])


@pytest.mark.parametrize(
  "mutation",
  [
    None,
    "filename",
    "missing-tarball",
    "extra-tarball",
    "wrong-name",
    "wrong-version",
    "missing-files",
    "duplicate-files",
    "extra-member",
    "wrong-bytes",
    "tarball-budget",
    "outer-extra",
  ],
)
def test_builder_packs_each_identity_once_and_checks_fresh_inventories(tmp_path, monkeypatch, mutation):
  inputs = tmp_path / "inputs"
  inputs.mkdir()
  for filename in ("celestial-jieqi.mjs", "celestial-jieqi.wasm"):
    (inputs / filename).write_bytes(b"wasm fixture")
  for attribute in ("PACKAGE_FILES", "WASM_ARTIFACT_FILES"):
    files = getattr(build_npm, attribute)
    monkeypatch.setattr(
      build_npm,
      attribute,
      {(inputs / name if name.startswith("celestial-jieqi.") else path): name for path, name in files.items()},
    )
  monkeypatch.setattr(build_npm, "project_version", lambda: "0.7.0")
  monkeypatch.setattr(build_npm.shutil, "which", lambda _name: "fake-npm")
  out = tmp_path / "out"
  out.mkdir()
  (out / "stale.tgz").write_bytes(b"stale")
  calls = []

  def pack(command, **_kwargs):
    assert command[:4] == ["fake-npm", "pack", "--json", "--pack-destination"]
    assert not (out / "stale.tgz").exists()
    stage = Path(command[-1])
    manifest = json.loads((stage / "package.json").read_bytes())
    name = manifest["name"]
    calls.append(name)
    if name == ALIAS_NAME:
      assert (out / "primary-custom.tgz").is_file()
    filename = "alias-custom.tgz" if name == ALIAS_NAME else "primary-custom.tgz"
    content = io.BytesIO()
    with tarfile.open(fileobj=content, mode="w:gz") as archive:
      for path in stage.iterdir():
        data = path.read_bytes()
        if name == ALIAS_NAME and mutation == "wrong-bytes" and path.name == "date.mjs":
          data = b"changed"
        info = tarfile.TarInfo(f"package/{path.name}")
        info.size = len(data)
        archive.addfile(info, io.BytesIO(data))
      if name == ALIAS_NAME and mutation == "extra-member":
        archive.addfile(tarfile.TarInfo("package/extra.txt"), io.BytesIO())
    (out / filename).write_bytes(content.getvalue())
    record = {
      "name": name,
      "version": "0.7.0",
      "filename": filename,
      "files": [{"path": path.name} for path in stage.iterdir()],
    }
    if name == ALIAS_NAME:
      if mutation == "filename":
        record["filename"] = "../escape.tgz"
      elif mutation == "missing-tarball":
        (out / filename).unlink()
      elif mutation == "extra-tarball":
        (out / "extra.tgz").write_bytes(b"extra")
      elif mutation == "wrong-name":
        record["name"] = PACKAGE_NAME
      elif mutation == "wrong-version":
        record["version"] = "0.7.1"
      elif mutation == "missing-files":
        record["files"].pop()
      elif mutation == "duplicate-files":
        record["files"].append(record["files"][0])
      elif mutation == "tarball-budget":
        (out / filename).write_bytes(b"0" * (build_npm.MAX_TARBALL_BYTES + 1))
    return SimpleNamespace(stdout=json.dumps([record]))

  monkeypatch.setattr(build_npm.subprocess, "run", pack)
  if mutation == "outer-extra":
    original_copy = build_npm.shutil.copy2

    def copy(source, destination):
      result = original_copy(source, destination)
      if destination.parent.name == "artifact":
        (destination.parent / "extra").mkdir(exist_ok=True)
      return result

    monkeypatch.setattr(build_npm.shutil, "copy2", copy)
  if mutation is not None:
    with pytest.raises(RuntimeError):
      build_npm.build(out)
  else:
    primary, alias = build_npm.build(out)
    assert [primary.name, alias.name] == ["primary-custom.tgz", "alias-custom.tgz"]
    assert len(list((out / "artifact").iterdir())) == 10
    assert len(list((out / "package").iterdir())) == 12
    assert len(list((out / "alias-package").iterdir())) == 7
    assert (out / "alias-package/LICENSE").read_bytes() == (REPO / "LICENSE").read_bytes()
    for package_name, stem in NPM_METADATA.items():
      metadata = json.loads((out / f"{stem}.json").read_bytes())
      assert metadata[0]["name"] == package_name
      assert (out / "artifact" / metadata[0]["filename"]).read_bytes() == (out / metadata[0]["filename"]).read_bytes()
  assert calls == [PACKAGE_NAME, ALIAS_NAME]


@pytest.mark.parametrize(
  "mutation",
  [
    "python -m pip install --only-binary :all: -r bindings/python/requirements-host.txt",
    "python -m pip install --require-hashes -r bindings/python/requirements-host.txt",
    "python -m pip install cibuildwheel==4.2.0",
    'python -m pip install --no-deps "$WHEEL" pytest-xdist',
  ],
)
def test_producer_install_gate_rejects_unlocked_mutations(mutation):
  with pytest.raises(AssertionError):
    assert_hash_locked_install(mutation)


def test_producer_install_gate_accepts_equivalent_only_binary_spelling():
  assert_hash_locked_install(
    "python -m pip install --require-hashes --only-binary=:all: -r bindings/python/requirements-host.txt"
  )
