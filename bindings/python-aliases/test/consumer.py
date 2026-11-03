# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""Clean-install the three Python names separately and in every combination."""

import argparse
import hashlib
import itertools
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
sys.path.append(str(REPO))

from toolbox.build_python_aliases import ALIAS_NAMES, PRIMARY_PACKAGE, PYPI_PACKAGES, validate_alias_wheel  # noqa: E402


def run(command: list[str], cwd: Path, success: bool = True) -> str:
  result = subprocess.run(
    command, cwd=cwd, env={**os.environ, "PYTHONPATH": ""}, text=True, capture_output=True, check=False
  )
  output = result.stdout + result.stderr
  if (result.returncode == 0) != success:
    raise RuntimeError(f"Consumer command exited {result.returncode}: {command}\n{output}")
  return output


def environment(root: Path, name: str) -> Path:
  directory = root / f"venv-{name}"
  run([sys.executable, "-m", "venv", str(directory)], root)
  return directory / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def installed_contract(python: Path, names: tuple[str, ...], version: str, cwd: Path) -> dict:
  source = f"""
import importlib, importlib.metadata, json
from pathlib import Path
import celestial_calendar as primary
assert primary.__version__ == {version!r}
assert importlib.metadata.version('celestial-calendar') == {version!r}
for name in {ALIAS_NAMES!r}:
    if name in {names!r}:
        alias = importlib.import_module(name)
        assert importlib.metadata.version(name) == {version!r}
        assert alias.__all__ == primary.__all__
        assert all(getattr(alias, key) is getattr(primary, key) for key in primary.__all__)
        civil = alias.CivilDateTime(2026, 1, 1, 0.5)
        assert alias.utc_to_jde(civil) == primary.utc_to_jde(civil)
        try:
            importlib.import_module(name + '.chart')
        except ModuleNotFoundError:
            pass
        else:
            raise AssertionError('alias must not invent a submodule API')
    else:
        try:
            importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            pass
        else:
            raise AssertionError('unrequested alias was installed')
print(json.dumps({{'package': str(Path(primary.__file__).resolve().parent), 'exports': primary.__all__}}))
"""
  return json.loads(run([str(python), "-I", "-c", source], cwd))


def local_wheels(directory: Path, version: str) -> dict[str, Path]:
  wheels = list(directory.glob("*.whl"))
  expected_aliases = {f"{name}-{version}-py3-none-any.whl": name for name in ALIAS_NAMES}
  primary = [wheel for wheel in wheels if wheel.name.startswith(f"celestial_calendar-{version}-py3-none-")]
  if len(wheels) != 3 or len(primary) != 1 or {w.name for w in wheels} != {*expected_aliases, primary[0].name}:
    raise RuntimeError("Consumer wheelhouse requires exactly one primary wheel and both exact aliases")
  expected_files = {filename for wheel in wheels for filename in (wheel.name, f"{wheel.name}.sha256")}
  if {p.name for p in directory.iterdir()} != expected_files:
    raise RuntimeError("Consumer wheelhouse must contain only three wheels and their SHA-256 sidecars")
  for wheel in wheels:
    digest = hashlib.sha256(wheel.read_bytes()).hexdigest()
    if wheel.with_name(f"{wheel.name}.sha256").read_bytes() != f"{digest}  {wheel.name}\n".encode():
      raise RuntimeError(f"Consumer wheel SHA-256 mismatch: {wheel.name}")
  result = {PRIMARY_PACKAGE: primary[0]}
  for filename, name in expected_aliases.items():
    validate_alias_wheel(directory / filename, name, version)
    result[name] = directory / filename
  return result


def typing_contract(python: Path, package: Path, exports: list[str], root: Path) -> None:
  """Check every named re-export and reject wrong types and missing typing markers."""
  for name in ALIAS_NAMES:
    positive = root / f"{name}_positive.py"
    positive.write_text(
      f"from {name} import ({', '.join(exports)})\n"
      f"import {name} as astro\nfrom typing import assert_type\n"
      "assert_type(astro.utc_to_jde(astro.CivilDateTime(2026, 1, 1, 0.5)), float)\n",
      encoding="utf-8",
    )
    command = [
      sys.executable,
      "-m",
      "mypy",
      "--strict",
      "--no-incremental",
      "--show-error-codes",
      "--python-executable",
      str(python),
      str(positive),
    ]
    run(command, root)
    negative = root / f"{name}_negative.py"
    negative.write_text(f"from {name} import utc_to_jde\nutc_to_jde('bad')\n", encoding="utf-8")
    output = run([*command[:-1], str(negative)], root, success=False)
    if "[arg-type]" not in output or 'incompatible type "str"' not in output:
      raise RuntimeError(f"Alias type rejection did not discriminate: {output}")
    marker = package.parent / name / "py.typed"
    hidden = marker.with_name("py.typed.hidden")
    marker.rename(hidden)
    try:
      output = run(command, root, success=False)
      if "[import-untyped]" not in output:
        raise RuntimeError(f"Missing-marker control did not discriminate: {output}")
    finally:
      hidden.rename(marker)
    print(f"PASS installed alias typing and wrong-type/missing-marker controls: {name}")


def mixed_version_control(python: Path, wheels: dict[str, Path], version: str, root: Path) -> None:
  """Offer a valid stale primary to the resolver, without executing its placeholder wheel."""
  wrong = root / "wrong-version"
  wrong.mkdir()
  stale = wrong / "celestial_calendar-0.0.0-py3-none-any.whl"
  info = "celestial_calendar-0.0.0.dist-info"
  with zipfile.ZipFile(stale, "w") as archive:
    archive.writestr(f"{info}/METADATA", "Metadata-Version: 2.1\nName: celestial-calendar\nVersion: 0.0.0\n")
    archive.writestr(f"{info}/WHEEL", "Wheel-Version: 1.0\nRoot-Is-Purelib: true\nTag: py3-none-any\n")
    archive.writestr(f"{info}/RECORD", "")
  prefix = [str(python), "-m", "pip", "--isolated", "install", "--dry-run", "--ignore-installed", "--no-index"]
  run([*prefix, str(stale)], root)
  for name in ALIAS_NAMES:
    shutil.copyfile(wheels[name], wrong / wheels[name].name)
    output = run([*prefix, "--find-links", str(wrong), f"{name}=={version}"], root, success=False)
    if f"celestial-calendar=={version}" not in output:
      raise RuntimeError(f"Mixed-version rejection did not identify the pinned dependency: {output}")


def missing_native_contract(python: Path, package: Path, root: Path) -> None:
  """Reject each alias import when the installed native implementation is missing."""
  native = list((package / "_native").iterdir())
  if len(native) != 1:
    raise RuntimeError("Expected exactly one installed native implementation")
  hidden = native[0].with_name(native[0].name + ".hidden")
  native[0].rename(hidden)
  try:
    for name in ALIAS_NAMES:
      output = run([str(python), "-I", "-c", f"import {name}"], root, success=False)
      if "OSError" not in output:
        raise RuntimeError(f"Missing-native control failed for {name} for another reason: {output}")
  finally:
    hidden.rename(native[0])


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("version")
  mode = parser.add_mutually_exclusive_group(required=True)
  mode.add_argument("--wheelhouse", type=Path)
  mode.add_argument("--registry", action="store_true")
  parser.add_argument("--typing", action="store_true")
  args = parser.parse_args()
  wheels = local_wheels(args.wheelhouse.resolve(), args.version) if args.wheelhouse else None
  with tempfile.TemporaryDirectory(prefix="celestial-alias-consumers-") as temporary:
    root = Path(temporary)
    if wheels is not None:
      python = environment(root, "exact-bytes")
      run([str(python), "-m", "pip", "--isolated", "install", "--no-deps", *(str(p) for p in wheels.values())], root)
      installed_contract(python, PYPI_PACKAGES, args.version, root)
      mixed_version_control(python, wheels, args.version, root)
      print("PASS exact wheel bytes and mixed-version resolver rejection")
    options = (
      ["--no-index", "--find-links", str(args.wheelhouse.resolve())]
      if wheels is not None
      else ["--index-url", "https://pypi.org/simple"]
    )
    for size in range(1, len(PYPI_PACKAGES) + 1):
      for names in itertools.combinations(PYPI_PACKAGES, size):
        python = environment(root, "-".join(names))
        run(
          [
            str(python),
            "-m",
            "pip",
            "--isolated",
            "install",
            "--only-binary=:all:",
            "--no-cache-dir",
            *options,
            *(f"{name}=={args.version}" for name in names),
          ],
          root,
        )
        installed = installed_contract(python, names, args.version, root)
        if names == PYPI_PACKAGES:
          print(run([str(python), str(REPO / "bindings/python/test/run_all.py")], root), end="")
          package = Path(installed["package"])
          if args.typing:
            typing_contract(python, package, installed["exports"], root)
          missing_native_contract(python, package, root)
          print("PASS missing-native rejection for both aliases")
        print(f"PASS clean installed Python names: {', '.join(names)}; {len(installed['exports'])} original exports")


if __name__ == "__main__":
  main()
