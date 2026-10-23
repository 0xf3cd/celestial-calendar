# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

"""Source and artifact receipts for the local shared-library build and jieqi exporter."""

import hashlib
import json
import re
import subprocess
import sys

from pathlib import Path

from . import paths


SCHEMA = "celestial-calendar/library-build@1"


def file_sha256(path: Path) -> str:
  """Hash the actual file bytes, following the library's version symlink."""
  with path.open("rb") as stream:
    return hashlib.file_digest(stream, "sha256").hexdigest()


def find_shared_lib(folder: Path | None = None) -> Path:
  """Find the native library, preferring its unversioned link to accumulated old versions."""
  extensions = {"win32": ".dll", "darwin": ".dylib", "linux": ".so"}
  if sys.platform not in extensions:
    raise OSError(f"Unsupported platform: {sys.platform}")
  extension = re.escape(extensions[sys.platform])
  pattern = re.compile(rf"^(?:lib)?celestial_calendar(?:\.\d+)*{extension}(?:\.\d+)*$")
  unversioned = re.compile(rf"^(?:lib)?celestial_calendar{extension}$")
  directory = paths.shared_lib_dir() if folder is None else folder
  candidates = [path for path in directory.iterdir() if path.is_file() and pattern.fullmatch(path.name)]
  candidates.sort(key=lambda path: (not unversioned.fullmatch(path.name), path.name))
  if not candidates:
    raise FileNotFoundError(f"Shared library not found under {directory}; run project.py --build first")
  return candidates[0]


def source_snapshot(root: Path) -> dict:
  """Capture all non-ignored production source files; dirty trees do not name a source commit."""
  tracked = subprocess.check_output(
    ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--", "src"], cwd=root
  )
  names = sorted({name.decode("utf-8") for name in tracked.split(b"\0") if name})
  files = {name: file_sha256(root / name) for name in names if not name.startswith(("src/test/", "src/bench/"))}
  if "src/shared_lib/CMakeLists.txt" not in files or "src/calendar/jieqi.hpp" not in files:
    raise RuntimeError("Cannot identify a complete shared-library source snapshot")
  serialized = json.dumps(files, sort_keys=True, separators=(",", ":")).encode("utf-8")
  dirty = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=normal"], cwd=root)
  commit = None if dirty else subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
  return {"commit": commit, "snapshot_sha256": hashlib.sha256(serialized).hexdigest(), "files": files}


def configuration_sha256(root: Path, build_dir: Path) -> str:
  """Require this build directory to belong to the source root and identify its CMake configuration."""
  cache = build_dir / "CMakeCache.txt"
  text = cache.read_text(encoding="utf-8")
  home = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", text, re.MULTILINE)
  if home is None or Path(home[1]).resolve() != (root / "src").resolve():
    raise RuntimeError("CMake build directory does not belong to this source tree; run project.py --cmake")
  return file_sha256(cache)


def receipt_path(library: Path) -> Path:
  """The receipt follows the resolved artifact, not an unversioned name that may move."""
  artifact = library.resolve(strict=True)
  return artifact.with_name(artifact.name + ".provenance.json")


def verify_library(library: Path, root: Path) -> dict:
  """Reject absent, damaged, stale-source or mismatched-artifact receipts before loading a library."""
  try:
    receipt = json.loads(receipt_path(library).read_text(encoding="utf-8"))
    if receipt["schema"] != SCHEMA:
      raise ValueError("unsupported receipt schema")
    source = receipt["source"]
    current = source_snapshot(root)
    if source["files"] != current["files"] or source["snapshot_sha256"] != current["snapshot_sha256"]:
      raise ValueError("source snapshot does not match this checkout")
    commit = source["commit"]
    if commit is not None:
      if not isinstance(commit, str) or re.fullmatch(r"[0-9a-f]{40}", commit) is None:
        raise ValueError("invalid source commit")
      for name, digest in source["files"].items():
        data = subprocess.check_output(["git", "show", f"{commit}:{name}"], cwd=root)
        if hashlib.sha256(data).hexdigest() != digest:
          raise ValueError("source commit does not contain the recorded snapshot")
    artifact = receipt["artifact"]
    if artifact["name"] != library.resolve(strict=True).name or artifact["sha256"] != file_sha256(library):
      raise ValueError("loaded artifact does not match its receipt")
    if (
      not isinstance(receipt["configuration_sha256"], str)
      or re.fullmatch(r"[0-9a-f]{64}", receipt["configuration_sha256"]) is None
    ):
      raise ValueError("missing CMake configuration identity")
    return receipt
  except (OSError, KeyError, TypeError, ValueError, subprocess.CalledProcessError) as error:
    raise RuntimeError(f"Cannot verify shared-library provenance: {error}; rebuild with project.py --build") from error


def build_library_project(root: Path, build_dir: Path, cpu_cores: int, run) -> int:
  """Build from a stable snapshot; only a fresh build may establish a new provenance receipt."""
  snapshot = source_snapshot(root)
  configuration = configuration_sha256(root, build_dir)
  reusable = None
  try:
    library = find_shared_lib(build_dir / "shared_lib")
    prior = verify_library(library, root)
    if prior["configuration_sha256"] == configuration:
      reusable = prior
    receipt_path(library).unlink(missing_ok=True)
  except (OSError, RuntimeError):
    # A stale receipt cannot survive a failed attempt to rebuild its artifact.
    directory = build_dir / "shared_lib"
    if directory.is_dir():
      for stale in directory.glob("*celestial_calendar*.provenance.json"):
        stale.unlink()

  command = ["cmake", "--build", ".", "--parallel", str(cpu_cores)]
  if reusable is None:
    command.append("--clean-first")
  result = run(command, cwd=build_dir)
  if result.retcode != 0:
    return result.retcode
  after = source_snapshot(root)
  if after != snapshot or configuration_sha256(root, build_dir) != configuration:
    raise RuntimeError("Sources or CMake configuration changed during the build; provenance was not recorded")
  library = find_shared_lib(build_dir / "shared_lib")
  artifact = {"name": library.resolve(strict=True).name, "sha256": file_sha256(library)}
  receipt = {
    "schema": SCHEMA,
    "source": reusable["source"] if reusable is not None and reusable["artifact"] == artifact else snapshot,
    "configuration_sha256": configuration,
    "artifact": artifact,
  }
  destination = receipt_path(library)
  temporary = destination.with_suffix(destination.suffix + ".pending")
  temporary.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
  temporary.replace(destination)
  return 0
