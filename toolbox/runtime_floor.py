#!/usr/bin/env python3
#
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

"""Measure a packaged native library's runtime requirements."""

import argparse
import json
import re
import subprocess
from pathlib import Path

if __package__:
  from .windows_toolchain_evidence import validate_windows_evidence
else:
  from windows_toolchain_evidence import validate_windows_evidence


SUPPORTED_RUNTIME = {
  "linux_amd64": {"glibc": "2.28", "glibcxx": "3.4.21"},
  "linux_arm64": {"glibc": "2.28", "glibcxx": "3.4.21"},
  "macos_arm64": {"macos": "14.0"},
  "windows_x86_64": {"windows": "not_declared"},
}


def version_key(version: str) -> tuple[int, ...]:
  """Convert a dotted version to a naturally ordered tuple."""
  return tuple(int(part) for part in version.split("."))


def validate_runtime_floor(runtime_floor, context: str) -> None:
  """Validate recorded runtime metadata and its support relationship."""
  if not isinstance(runtime_floor, dict) or set(runtime_floor) != {"supported", "measured"}:
    raise RuntimeError(f"Invalid runtime floor in {context}")
  supported = runtime_floor["supported"]
  measured = runtime_floor["measured"]
  if not all(
    isinstance(values, dict)
    and values
    and all(isinstance(key, str) and isinstance(value, str) and key and value for key, value in values.items())
    for values in (supported, measured)
  ):
    raise RuntimeError(f"Invalid runtime floor in {context}")
  for key in supported.keys() & measured.keys():
    try:
      exceeds_support = version_key(measured[key]) > version_key(supported[key])
    except ValueError:
      if measured[key] != supported[key]:
        raise RuntimeError(
          f"Measured {key} property {measured[key]} does not match supported {supported[key]} in {context}"
        ) from None
      continue
    if exceeds_support:
      raise RuntimeError(f"Measured {key} requirement {measured[key]} exceeds supported {supported[key]} in {context}")


def parse_elf_versions(output: str) -> dict[str, str]:
  """Return the greatest required GLIBC and GLIBCXX symbol versions."""
  glibc = set(re.findall(r"\bGLIBC_([0-9]+(?:\.[0-9]+)+)\b", output))
  glibcxx = set(re.findall(r"\bGLIBCXX_([0-9]+(?:\.[0-9]+)+)\b", output))
  if not glibc or not glibcxx:
    raise RuntimeError("ELF version information must contain GLIBC and GLIBCXX requirements")
  return {"glibc": max(glibc, key=version_key), "glibcxx": max(glibcxx, key=version_key)}


def parse_macos_versions(output: str) -> dict[str, str]:
  """Return the greatest minimum OS version among Mach-O load commands."""
  versions = re.findall(r"\bminos\s+([0-9]+(?:\.[0-9]+)+)", output)
  if not versions:
    raise RuntimeError("Mach-O load commands do not declare a minimum macOS version")
  return {"macos": max(versions, key=version_key)}


def inspect(
  artifact: str,
  binary: Path,
  windows_evidence: Path | None = None,
  producer: str | None = None,
) -> dict[str, str]:
  """Inspect one packaged binary using the platform's native object tool."""
  if artifact.startswith("linux_"):
    command = ["readelf", "--version-info", str(binary)]
    parser = parse_elf_versions
  elif artifact.startswith("macos_"):
    command = ["otool", "-l", str(binary)]
    parser = parse_macos_versions
  elif artifact.startswith("windows_"):
    if windows_evidence is None or producer is None:
      raise RuntimeError("Windows runtime-floor measurement requires positive link evidence")
    return validate_windows_evidence(windows_evidence, binary, producer)
  else:
    raise RuntimeError(f"Unknown native artifact: {artifact}")
  output = subprocess.run(
    command,
    check=True,
    text=True,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
  ).stdout
  return parser(output)


def record_runtime_floor(
  artifact: str,
  binary: Path,
  build_info_path: Path,
  windows_evidence: Path | None = None,
  producer: str | None = None,
) -> None:
  """Add declared support and measured requirements to build_info.json."""
  if artifact not in SUPPORTED_RUNTIME:
    raise RuntimeError(f"Unknown native artifact: {artifact}")
  if not binary.is_file():
    raise RuntimeError(f"Native runtime library is not a file: {binary}")
  try:
    build_info = json.loads(build_info_path.read_text(encoding="utf-8"))
  except (json.JSONDecodeError, UnicodeDecodeError) as error:
    raise RuntimeError(f"Invalid build info {build_info_path}: {error}") from error
  if not isinstance(build_info, dict):
    raise RuntimeError(f"Build info must be a JSON object: {build_info_path}")
  runtime_floor = {
    "supported": SUPPORTED_RUNTIME[artifact],
    "measured": inspect(artifact, binary, windows_evidence, producer),
  }
  validate_runtime_floor(runtime_floor, str(build_info_path))
  build_info["runtime_floor"] = runtime_floor
  build_info_path.write_text(f"{json.dumps(build_info, indent=2)}\n", encoding="utf-8")


def main() -> None:
  """Parse CLI arguments and record one artifact's runtime floor."""
  parser = argparse.ArgumentParser()
  parser.add_argument("--artifact", choices=SUPPORTED_RUNTIME, required=True)
  parser.add_argument("--binary", type=Path, required=True)
  parser.add_argument("--build-info", type=Path, required=True)
  parser.add_argument("--windows-evidence", type=Path)
  parser.add_argument("--producer", choices=("native", "wheel"))
  args = parser.parse_args()
  record_runtime_floor(args.artifact, args.binary, args.build_info, args.windows_evidence, args.producer)


if __name__ == "__main__":
  main()
