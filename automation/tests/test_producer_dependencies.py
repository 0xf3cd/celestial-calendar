# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

import re
import tomllib
from pathlib import Path

import pytest
import yaml

from toolbox.build_npm import PACKAGE_FILES, PACK_ALLOWLIST, WASM_ARTIFACT_FILES
from toolbox.release_validation import SOURCE_WORKFLOWS


REPO = Path(__file__).parents[2]
BUILD_WORKFLOW = REPO / ".github" / "workflows" / "build_and_test.yml"
WHEEL_WORKFLOW = REPO / ".github" / "workflows" / "python-wheel.yml"
RELEASE_WORKFLOW = REPO / ".github" / "workflows" / "release.yml"
DOCKERFILE = REPO / "Dockerfile"
NATIVE_CMAKE = REPO / "src" / "shared_lib" / "CMakeLists.txt"
PYTHON_CMAKE = REPO / "bindings" / "python" / "CMakeLists.txt"
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
PIP_INSTALL_RE = re.compile(r"(?:^|[/\\\s])pip(?:3(?:\.\d+)?)?(?:\.exe)?(?:\s+--\S+)*\s+install(?:\s|$)")
LOCAL_WHEEL_INSTALL_RE = re.compile(
  r'^\S+ -m pip install --no-deps "(?:/wheels/\$WHEEL_FILENAME|\$WHEEL|\$env:WHEEL)"$'
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
  return [
    line for line in path.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")
  ]


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


def workflow_install_lines(path):
  workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
  lines = [
    line
    for job in workflow["jobs"].values()
    for step in job["steps"]
    for line in pip_install_lines(str(step.get("run", "")))
  ]
  lines += [
    line
    for env in [workflow.get("env", {})]
    + [job.get("env", {}) for job in workflow["jobs"].values()]
    + [step.get("env", {}) for job in workflow["jobs"].values() for step in job["steps"]]
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
  notice = REPO / "THIRD_PARTY_NOTICES.txt"
  python_cmake = PYTHON_CMAKE.read_text(encoding="utf-8")

  assert '"${REPO_ROOT}/THIRD_PARTY_NOTICES.txt"' in python_cmake
  assert PACKAGE_FILES[notice] == "THIRD_PARTY_NOTICES.txt"
  assert {"package.json", *PACKAGE_FILES.values()} == PACK_ALLOWLIST
  assert WASM_ARTIFACT_FILES[notice] == "THIRD_PARTY_NOTICES.txt"


def test_readme_describes_the_current_npm_and_wasm_members():
  readme = (REPO / "README.md").read_text(encoding="utf-8")

  assert "exact nine-file npm tarball" in readme
  assert "raw `.mjs/.wasm` pair, `LICENSE`, `THIRD_PARTY_NOTICES.txt`" in readme


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
