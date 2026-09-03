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

"""Capture and verify Windows final-link evidence without changing the linked DLL."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import zipfile

from pathlib import Path
from typing import Final, Sequence


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
CONTRACT_PATH: Final[Path] = REPO_ROOT / "automation" / "windows_toolchain_contract.json"
REQUIRED_STATIC_ROLES: Final[dict[str, str]] = {
  "c_runtime": "libcmt.lib",
  "cxx_runtime": "libcpmt.lib",
  "ucrt": "libucrt.lib",
  "vcruntime": "libvcruntime.lib",
}
FORBIDDEN_DYNAMIC_IMPORTS: Final[tuple[str, ...]] = (
  "API-MS-WIN-CRT-*.DLL",
  "MSVCP*.DLL",
  "MSVCR*.DLL",
  "UCRTBASE*.DLL",
  "VCRUNTIME*.DLL",
)
WINDOWS_NATIVE_MEMBER: Final[str] = "celestial_calendar/_native/_celestial_calendar.dll"
TERMS_NAME_TOKENS: Final[tuple[str, ...]] = ("eula", "licence", "license")
TERMS_SUFFIXES: Final[frozenset[str]] = frozenset({"", ".htm", ".html", ".rtf", ".txt", ".xml"})
RUNNER_IDENTITY_FIELDS: Final[tuple[str, ...]] = (
  "ImageOS",
  "ImageVersion",
  "RUNNER_ARCH",
  "RUNNER_OS",
  "WindowsSdkDir",
  "WindowsSDKVersion",
  "VCToolsInstallDir",
  "VCToolsVersion",
)


def _require(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def _sha256(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def _write_json(path: Path, payload: object) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(f"{json.dumps(payload, indent=2, sort_keys=True)}\n", encoding="utf-8", newline="\n")


def _read_json(path: Path, label: str) -> dict:
  try:
    payload = json.loads(path.read_text(encoding="utf-8"))
  except (json.JSONDecodeError, UnicodeDecodeError) as error:
    raise RuntimeError(f"Invalid {label} {path}: {error}") from error
  _require(isinstance(payload, dict), f"{label} must be a JSON object: {path}")
  return payload


def normalized_pe_fingerprint(data: bytes) -> dict[str, int | str]:
  """Hash PE bytes after zeroing only the four-byte COFF TimeDateStamp field."""
  _require(len(data) >= 0x40 and data[:2] == b"MZ", "PE DOS header is missing or truncated")
  pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
  _require(pe_offset + 24 <= len(data), "PE header is truncated")
  _require(data[pe_offset : pe_offset + 4] == b"PE\0\0", "PE signature differs")
  machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
  _require(machine == 0x8664, "PE machine is not AMD64")
  optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
  _require(pe_offset + 24 + optional_size <= len(data), "PE optional header is truncated")
  optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
  _require(optional_magic == 0x20B, "PE optional header is not PE32+")

  timestamp_offset = pe_offset + 8
  timestamp = struct.unpack_from("<I", data, timestamp_offset)[0]
  normalized = bytearray(data)
  normalized[timestamp_offset : timestamp_offset + 4] = b"\0\0\0\0"
  return {
    "algorithm": "coff_timestamp_zero_v1",
    "coff_timestamp": timestamp,
    "coff_timestamp_offset": timestamp_offset,
    "sha256": _sha256(normalized),
  }


def parse_windows_imports(output: str) -> list[str]:
  """Parse the complete ordered DLL import inventory from llvm-readobj output."""
  imports = re.findall(r"\bName:\s*([^\s]+\.dll)\b", output, flags=re.IGNORECASE)
  _require(imports, "Windows import table is empty or malformed")
  canonical = [name.upper() for name in imports]
  _require(len(canonical) == len(set(canonical)), "Windows import table contains duplicate DLL descriptors")
  return canonical


def _forbidden_import(import_name: str) -> bool:
  upper = import_name.upper()
  return any(re.fullmatch(pattern.replace("*", ".*"), upper) is not None for pattern in FORBIDDEN_DYNAMIC_IMPORTS)


def _windows_command_line(command_line: str) -> list[str]:
  if os.name != "nt":
    # Test-only fallback. Production capture always uses CommandLineToArgvW on Windows.
    import shlex

    return shlex.split(command_line)
  command_line_to_argv = ctypes.windll.shell32.CommandLineToArgvW
  command_line_to_argv.argtypes = (ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int))
  command_line_to_argv.restype = ctypes.POINTER(ctypes.c_wchar_p)
  local_free = ctypes.windll.kernel32.LocalFree
  local_free.argtypes = (ctypes.c_void_p,)
  local_free.restype = ctypes.c_void_p

  argc = ctypes.c_int()
  argv = command_line_to_argv(command_line, ctypes.byref(argc))
  if not argv:
    raise ctypes.WinError()
  try:
    return [argv[index] for index in range(argc.value)]
  finally:
    local_free(ctypes.cast(argv, ctypes.c_void_p))


def _response_arguments(path: Path) -> list[str]:
  raw = path.read_bytes()
  for encoding in ("utf-8-sig", "utf-16"):
    try:
      command_line = raw.decode(encoding).strip()
    except UnicodeDecodeError:
      continue
    _require(command_line, f"Linker response file is empty: {path}")
    _require("\0" not in command_line, f"Linker response file contains NUL: {path}")
    command_line = re.sub(r"[\r\n]+", " ", command_line)
    arguments = _windows_command_line(f"response-file {command_line}")
    _require(arguments and arguments[0] == "response-file", f"Cannot parse linker response file: {path}")
    return arguments[1:]
  raise RuntimeError(f"Cannot decode linker response file: {path}")


def _expand_response_files(arguments: Sequence[str], evidence_dir: Path) -> tuple[list[str], list[dict]]:
  expanded: list[str] = []
  records: list[dict] = []
  pending = list(arguments)
  seen: set[Path] = set()
  while pending:
    argument = pending.pop(0)
    if not argument.startswith("@"):
      expanded.append(argument)
      continue
    source = Path(argument[1:].strip('"')).resolve()
    _require(source.is_file(), f"Linker response file does not exist: {source}")
    _require(source not in seen, f"Linker response-file cycle or duplicate: {source}")
    seen.add(source)
    raw = source.read_bytes()
    retained = evidence_dir / "link" / "response-files" / f"{len(records):02d}-{source.name}"
    retained.parent.mkdir(parents=True, exist_ok=True)
    retained.write_bytes(raw)
    arguments_in_file = _response_arguments(source)
    records.append(
      {
        "arguments": arguments_in_file,
        "path": str(source),
        "retained_path": retained.relative_to(evidence_dir).as_posix(),
        "sha256": _sha256(raw),
      }
    )
    pending = arguments_in_file + pending
  _require(not any(argument.startswith("@") for argument in expanded), "Unresolved linker response file remains")
  return expanded, records


def _output_path(arguments: Sequence[str]) -> Path:
  for index, argument in enumerate(arguments):
    if argument == "-o" and index + 1 < len(arguments):
      return Path(arguments[index + 1]).resolve()
    if argument.lower().startswith("/out:"):
      return Path(argument.split(":", maxsplit=1)[1].strip('"')).resolve()
  raise RuntimeError("Final-link output path was not found")


def _run_bytes(command: Sequence[str], check: bool = True) -> subprocess.CompletedProcess[bytes]:
  return subprocess.run(command, check=check, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def _run_text(command: Sequence[str], check: bool = True) -> str:
  result = subprocess.run(
    command,
    check=check,
    text=True,
    encoding="utf-8",
    errors="replace",
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
  )
  return result.stdout


def capture_link(evidence_dir: Path, producer: str, command: Sequence[str]) -> None:
  """Run the CMake final-link command exactly once and retain its diagnostics."""
  _require(producer in {"native", "wheel"}, f"Unknown Windows producer: {producer}")
  _require(command, "Final-link command is empty")
  evidence_dir.mkdir(parents=True, exist_ok=True)
  (evidence_dir / "link").mkdir(exist_ok=True)
  expanded, responses = _expand_response_files(command[1:], evidence_dir)

  dry_run = _run_bytes([command[0], "-###", *command[1:]], check=False)
  (evidence_dir / "link" / "driver-dry-run.stdout.bin").write_bytes(dry_run.stdout)
  (evidence_dir / "link" / "driver-dry-run.stderr.bin").write_bytes(dry_run.stderr)

  linked = _run_bytes(command, check=False)
  (evidence_dir / "link" / "final-link.stdout.bin").write_bytes(linked.stdout)
  (evidence_dir / "link" / "final-link.stderr.bin").write_bytes(linked.stderr)
  sys.stdout.buffer.write(linked.stdout)
  sys.stderr.buffer.write(linked.stderr)
  if linked.returncode != 0:
    raise subprocess.CalledProcessError(linked.returncode, command)

  output = _output_path(expanded)
  _require(output.is_file(), f"Final-link DLL does not exist: {output}")
  data = output.read_bytes()
  linked_copy = evidence_dir / "linked" / "original.dll"
  linked_copy.parent.mkdir(parents=True, exist_ok=True)
  linked_copy.write_bytes(data)
  _write_json(
    evidence_dir / "link" / "invocation.json",
    {
      "command": list(command),
      "compiler": str(Path(command[0]).resolve()),
      "expanded_arguments": expanded,
      "output": str(output),
      "producer": producer,
      "raw_dll_sha256": _sha256(data),
      "response_files": responses,
    },
  )


def _linker_arguments(dry_run: str) -> list[str]:
  candidates = []
  for line in dry_run.splitlines():
    try:
      arguments = _windows_command_line(line.strip())
    except (OSError, ValueError):
      continue
    if arguments and any(argument.lower().startswith(("/out:", "-out:")) for argument in arguments):
      candidates.append(arguments)
  _require(len(candidates) == 1, f"Actual linker invocation count is {len(candidates)}, expected 1")
  return candidates[0]


def _library_search_paths(arguments: Sequence[str]) -> list[Path]:
  paths = []
  for argument in arguments:
    lower = argument.lower()
    if lower.startswith(("/libpath:", "-libpath:")):
      paths.append(Path(argument.split(":", maxsplit=1)[1].strip('"')))
  paths.extend(Path(path) for path in os.environ.get("LIB", "").split(";") if path)
  return [path.resolve() for path in paths if path.is_dir()]


def _library_names(arguments: Sequence[str], verbose_link: str) -> list[str]:
  explicit = []
  defaults = []
  excluded_defaults: set[str] = set()
  exclude_all_defaults = False
  for argument in arguments:
    lower = argument.lower()
    if lower in {"/nodefaultlib", "-nodefaultlib"}:
      exclude_all_defaults = True
      continue
    if lower.startswith(("/nodefaultlib:", "-nodefaultlib:")):
      value = argument.split(":", maxsplit=1)[1].strip('"')
      if not value.lower().endswith(".lib"):
        value += ".lib"
      excluded_defaults.add(Path(value).name.lower())
      continue
    if lower.startswith(("/defaultlib:", "-defaultlib:")):
      value = argument.split(":", maxsplit=1)[1].strip('"')
      if not value.lower().endswith(".lib"):
        value += ".lib"
      defaults.append(Path(value).name.lower())
      continue
    if lower.startswith(("/", "-")):
      continue
    value = argument.strip('"')
    if value.lower().endswith(".lib"):
      explicit.append(Path(value).name.lower())

  names = explicit
  if not exclude_all_defaults:
    names.extend(name for name in defaults if name not in excluded_defaults)
  names.extend(_loaded_library_names(verbose_link))
  return list(dict.fromkeys(names))


def _loaded_library_names(verbose_link: str) -> list[str]:
  names = []
  for line in verbose_link.splitlines():
    if re.search(r"(?i)\b(?:loaded|reading)\b", line):
      names.extend(name.lower() for name in re.findall(r"(?i)\b([A-Za-z0-9_.+-]+\.lib)\b", line))
  return list(dict.fromkeys(names))


def _resolve_library(name: str, search_paths: Sequence[Path], arguments: Sequence[str]) -> Path | None:
  for argument in arguments:
    candidate = Path(argument.strip('"'))
    if candidate.name.lower() == name and candidate.is_file():
      return candidate.resolve()
  for directory in search_paths:
    candidate = directory / name
    if candidate.is_file():
      return candidate.resolve()
  return None


def _archive_members(path: Path) -> list[str]:
  output = _run_text(["llvm-ar", "t", str(path)])
  members = output.splitlines()
  _require(members, f"Static archive has no listed members: {path}")
  return members


def _visual_studio_identity() -> dict:
  installer = Path(os.environ.get("ProgramFiles(x86)", "")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
  if not installer.is_file():
    return {"identity_unrecovered": True}
  output = _run_text([str(installer), "-latest", "-format", "json"])
  try:
    instances = json.loads(output)
  except json.JSONDecodeError as error:
    raise RuntimeError(f"Invalid vswhere output: {error}") from error
  _require(isinstance(instances, list) and len(instances) == 1, "Visual Studio instance inventory differs")
  instance = instances[0]
  return {
    "installation_path": instance.get("installationPath"),
    "installation_version": instance.get("installationVersion"),
    "product_id": instance.get("productId"),
  }


def _decode_terms(data: bytes) -> str:
  for encoding in ("utf-8-sig", "utf-16", "latin-1"):
    try:
      return data.decode(encoding)
    except UnicodeDecodeError:
      continue
  return ""


def _terms_evidence(evidence_dir: Path, visual_studio: dict) -> dict:
  roots = []
  installation_path = visual_studio.get("installation_path")
  if isinstance(installation_path, str) and installation_path:
    roots.append(Path(installation_path))
  installer = Path(os.environ.get("ProgramFiles(x86)", "")) / "Microsoft Visual Studio" / "Installer"
  roots.append(installer)
  searched_roots = list(dict.fromkeys(str(root.resolve()) for root in roots if root.is_dir()))

  documents = []
  for root_text in searched_roots:
    for directory, _subdirectories, filenames in os.walk(root_text):
      for filename in filenames:
        folded_name = filename.casefold()
        if Path(folded_name).suffix not in TERMS_SUFFIXES or not any(
          token in folded_name for token in TERMS_NAME_TOKENS
        ):
          continue
        source = Path(directory) / filename
        try:
          data = source.read_bytes()
        except OSError:
          continue
        text = _decode_terms(data).casefold()
        if not (
          "microsoft" in text
          and ("visual studio" in text or "visual c++" in text)
          and ("license" in text or "licence" in text)
        ):
          continue
        retained = evidence_dir / "terms" / f"{len(documents):02d}-{filename}"
        retained.parent.mkdir(parents=True, exist_ok=True)
        retained.write_bytes(data)
        documents.append(
          {
            "path": str(source.resolve()),
            "retained_path": retained.relative_to(evidence_dir).as_posix(),
            "sha256": _sha256(data),
          }
        )

  return {
    "documents": documents,
    "identity_unrecovered": not documents,
    "searched_roots": searched_roots,
    "terms_text_captured": bool(documents),
  }


def _mt_request(producer: str) -> dict:
  relative = Path("src/CMakeLists.txt") if producer == "native" else Path("bindings/python/CMakeLists.txt")
  text = (REPO_ROOT / relative).read_text(encoding="utf-8")
  request = 'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")'
  target = "add_subdirectory(shared_lib)" if producer == "native" else 'add_subdirectory("${REPO_ROOT}/src/shared_lib"'
  _require(text.count(request) == 1, f"{producer} /MT request differs")
  _require(text.index(request) < text.index(target), f"{producer} /MT request must precede target creation")
  return {"cmake_value": "MultiThreaded$<$<CONFIG:Debug>:Debug>", "path": relative.as_posix()}


def _runner_identity(search_paths: Sequence[Path]) -> dict:
  identity = {name: os.environ.get(name) for name in RUNNER_IDENTITY_FIELDS}
  msvc_identities: set[tuple[str, str]] = set()
  sdk_identities: set[tuple[str, str]] = set()
  for path in search_paths:
    parts = path.parts
    folded = tuple(part.casefold() for part in parts)
    for index in range(len(parts) - 4):
      if folded[index : index + 3] == ("vc", "tools", "msvc") and folded[index + 4] == "lib":
        msvc_identities.add((str(Path(*parts[: index + 4])), parts[index + 3]))
      if folded[index] == "windows kits" and folded[index + 2] == "lib":
        sdk_identities.add((str(Path(*parts[: index + 2])), parts[index + 3]))

  _require(len(msvc_identities) <= 1, "MSVC toolset identity differs across linker search paths")
  _require(len(sdk_identities) <= 1, "Windows SDK identity differs across linker search paths")
  if msvc_identities:
    identity["VCToolsInstallDir"], identity["VCToolsVersion"] = next(iter(msvc_identities))
  if sdk_identities:
    identity["WindowsSdkDir"], identity["WindowsSDKVersion"] = next(iter(sdk_identities))
  return identity


def _tool_identity(command: str, version_argument: str = "--version") -> dict:
  path = Path(command)
  resolved = path.resolve() if path.is_file() else Path(shutil.which(command) or command)
  _require(resolved.is_file(), f"Tool path was not resolved: {command}")
  return {
    "path": str(resolved),
    "sha256": _sha256(resolved.read_bytes()),
    "version": _run_text([str(resolved), version_argument], check=False).strip(),
  }


def _packaged_dll(binary: Path | None, wheel: Path | None, evidence_dir: Path) -> tuple[Path, dict]:
  _require((binary is None) != (wheel is None), "Specify exactly one packaged DLL or wheel")
  if binary is not None:
    _require(binary.is_file(), f"Packaged native DLL does not exist: {binary}")
    return binary, {"kind": "native", "path": str(binary.resolve())}
  assert wheel is not None
  _require(wheel.is_file(), f"Windows wheel does not exist: {wheel}")
  with zipfile.ZipFile(wheel) as archive:
    _require(WINDOWS_NATIVE_MEMBER in archive.namelist(), "Windows wheel native member is missing")
    data = archive.read(WINDOWS_NATIVE_MEMBER)
  extracted = evidence_dir / "packaged" / "wheel-member.dll"
  extracted.parent.mkdir(parents=True, exist_ok=True)
  extracted.write_bytes(data)
  return extracted, {
    "kind": "wheel",
    "member": WINDOWS_NATIVE_MEMBER,
    "path": str(wheel.resolve()),
    "wheel_sha256": _sha256(wheel.read_bytes()),
  }


def approved_view(report: dict) -> dict:
  """Return the exact capture profile retained for deliberate approval."""
  keys = (
    "compiler",
    "imports",
    "linker",
    "normalized_pe",
    "raw_dll_sha256",
    "response_files",
    "runner",
    "selected_default_libraries",
    "static_library_roles",
    "terms",
    "visual_studio",
  )
  return {key: report[key] for key in keys}


def standing_view(report: dict) -> dict:
  """Return only stable fields suitable for ordinary PR and push gates."""
  return {
    "artifact_binding_required": True,
    "forbidden_dynamic_imports": list(FORBIDDEN_DYNAMIC_IMPORTS),
    "imports": report["imports"],
    "mt_request": report["mt_request"],
    "producer": report["producer"],
    "required_static_roles": sorted(report["static_library_roles"]),
    "response_expansion_required": True,
  }


def _approved_comparison(profile: dict) -> dict:
  comparable = json.loads(json.dumps(profile))
  comparable.pop("raw_dll_sha256", None)
  comparable.get("normalized_pe", {}).pop("coff_timestamp", None)
  return comparable


def _validate_intrinsic(report: dict, packaged_dll: Path, producer: str) -> None:
  _require(report.get("schema") == 1, "Windows evidence schema differs")
  _require(report.get("producer") == producer, "Windows evidence producer differs")
  data = packaged_dll.read_bytes()
  _require(report.get("raw_dll_sha256") == _sha256(data), "Windows evidence does not bind the packaged DLL")
  _require(
    report.get("link_trace_raw_dll_sha256") == report.get("raw_dll_sha256"),
    "Windows final-link trace does not bind the packaged DLL",
  )
  _require(
    report.get("linked_copy_raw_dll_sha256") == report.get("raw_dll_sha256"),
    "Windows raw evidence copy does not bind the packaged DLL",
  )
  _require(report.get("artifact_binding") is True, "Windows producer sidecar does not bind the packaged artifact")
  expected_normalized = normalized_pe_fingerprint(data)
  _require(report.get("normalized_pe") == expected_normalized, "Windows normalized PE fingerprint differs")
  imports = report.get("imports")
  _require(isinstance(imports, list) and imports, "Windows evidence import inventory is missing")
  _require(
    all(isinstance(name, str) and name == name.upper() for name in imports) and imports == sorted(set(imports)),
    "Windows evidence import inventory is malformed or duplicated",
  )
  _require(not any(_forbidden_import(name) for name in imports), "Windows DLL imports an unapproved dynamic runtime")
  runner = report.get("runner")
  _require(
    isinstance(runner, dict)
    and set(runner) == set(RUNNER_IDENTITY_FIELDS)
    and all(isinstance(runner[field], str) and runner[field] for field in RUNNER_IDENTITY_FIELDS),
    "Windows runner/toolset/SDK identity is incomplete",
  )
  selected = report.get("selected_default_libraries")
  _require(
    isinstance(selected, list)
    and all(isinstance(name, str) and name for name in selected)
    and len(selected) == len(set(selected)),
    "Windows selected-library inventory is malformed or duplicated",
  )
  roles = report.get("static_library_roles")
  _require(
    isinstance(roles, dict) and set(roles) == set(REQUIRED_STATIC_ROLES),
    "Windows positive static CRT/UCRT role inventory differs",
  )
  for role, basename in REQUIRED_STATIC_ROLES.items():
    archive = roles[role]
    _require(
      isinstance(archive, dict) and set(archive) == {"basename", "members", "path", "sha256"},
      f"Windows static library role evidence differs: {role}",
    )
    _require(archive["basename"] == basename, f"Windows static library role differs: {role}")
    _require(
      isinstance(archive["path"], str)
      and archive["path"]
      and isinstance(archive["sha256"], str)
      and re.fullmatch(r"[0-9a-f]{64}", archive["sha256"]) is not None,
      f"Windows static library identity differs: {role}",
    )
    _require(
      isinstance(archive["members"], list)
      and archive["members"]
      and all(isinstance(member, str) and member for member in archive["members"])
      and len(archive["members"]) == len(set(archive["members"])),
      f"Windows static library member inventory differs: {role}",
    )
    _require(basename in selected, f"Windows selected library is missing its static role: {role}")
  _require(report.get("response_expanded") is True, "Windows linker response expansion is incomplete")
  _require(report.get("mt_request") == _mt_request(producer), "Windows /MT request evidence differs")
  terms = report.get("terms")
  _require(
    isinstance(terms, dict)
    and set(terms) == {"documents", "identity_unrecovered", "searched_roots", "terms_text_captured"},
    "Windows terms-capture evidence differs",
  )
  documents = terms["documents"]
  _require(isinstance(documents, list), "Windows terms document inventory differs")
  _require(
    terms["terms_text_captured"] is bool(documents) and terms["identity_unrecovered"] is (not documents),
    "Windows terms-capture disposition differs",
  )
  _require(
    isinstance(terms["searched_roots"], list)
    and terms["searched_roots"]
    and all(isinstance(root, str) and root for root in terms["searched_roots"]),
    "Windows terms search-root inventory differs",
  )
  for document in documents:
    _require(
      isinstance(document, dict) and set(document) == {"path", "retained_path", "sha256"},
      "Windows terms document identity differs",
    )
    _require(
      all(isinstance(document[field], str) and document[field] for field in document),
      "Windows terms document identity differs",
    )
    _require(re.fullmatch(r"[0-9a-f]{64}", document["sha256"]) is not None, "Windows terms digest differs")


def _contract_profile(contract: dict, lifetime: str, producer: str) -> dict | None:
  profiles = contract.get(lifetime)
  _require(isinstance(profiles, dict), f"Windows {lifetime} contract inventory differs")
  _require(set(profiles) == {"native", "wheel"}, f"Windows {lifetime} producer inventory differs")
  profile = profiles[producer]
  _require(profile is None or isinstance(profile, dict), f"Windows {lifetime} profile differs for {producer}")
  return profile


def evaluate_contract(report: dict, contract: dict, mode: str, packaged_dll: Path, producer: str) -> list[str]:
  """Return deferred contract errors so workflows can upload raw evidence before failing."""
  errors = []
  try:
    _validate_intrinsic(report, packaged_dll, producer)
    _require(contract.get("schema") == 1, "Windows toolchain contract schema differs")
    _require(mode in {"capture", "standing", "verify-approved"}, f"Unknown Windows evidence mode: {mode}")
    if mode in {"standing", "verify-approved"}:
      expected_standing = _contract_profile(contract, "standing_contract", producer)
      _require(isinstance(expected_standing, dict), f"Standing Windows contract is not approved for {producer}")
      _require(standing_view(report) == expected_standing, f"Standing Windows contract differs for {producer}")
    if mode == "verify-approved":
      approved = _contract_profile(contract, "approved_evidence", producer)
      _require(isinstance(approved, dict), f"Windows evidence profile is not approved for {producer}")
      _require(
        _approved_comparison(approved_view(report)) == _approved_comparison(approved),
        f"Approved Windows evidence profile differs for {producer}",
      )
  except (KeyError, RuntimeError, TypeError) as error:
    errors.append(str(error))
  return errors


def validate_windows_evidence(report_path: Path, packaged_dll: Path, producer: str) -> dict[str, str]:
  """Require positive static-runtime evidence and return the public runtime-floor shape."""
  report = _read_json(report_path, "Windows evidence report")
  _validate_intrinsic(report, packaged_dll, producer)
  return {"msvc_runtime": "static"}


def collect_evidence(
  evidence_dir: Path,
  producer: str,
  mode: str,
  binary: Path | None,
  wheel: Path | None,
  build_info_path: Path | None,
  sidecar_path: Path | None,
  contract_path: Path,
  github_output: Path | None,
) -> None:
  """Collect a run-bound report and defer policy failure to the enforce command."""
  invocation = _read_json(evidence_dir / "link" / "invocation.json", "Windows link invocation")
  linked_dll = evidence_dir / "linked" / "original.dll"
  _require(linked_dll.is_file(), "Original linked DLL evidence is missing")
  packaged_dll, packaged_identity = _packaged_dll(binary, wheel, evidence_dir)
  packaged_data = packaged_dll.read_bytes()
  artifact_binding = False
  if producer == "native":
    _require(build_info_path is not None, "Native Windows evidence requires build_info.json")
    build_info = _read_json(build_info_path, "native build info")
    artifact_binding = build_info.get("sha256", {}).get("celestial_calendar.dll") == _sha256(packaged_data)
    packaged_identity["build_info_path"] = str(build_info_path.resolve())
  else:
    _require(wheel is not None and sidecar_path is not None, "Wheel evidence requires its SHA-256 sidecar")
    _require(sidecar_path.is_file(), f"Wheel SHA-256 sidecar does not exist: {sidecar_path}")
    artifact_binding = sidecar_path.read_text(encoding="ascii") == f"{_sha256(wheel.read_bytes())}  {wheel.name}\n"
    packaged_identity["sidecar_path"] = str(sidecar_path.resolve())

  imports_output = _run_text(["llvm-readobj", "--coff-imports", str(packaged_dll)])
  inspection = evidence_dir / "inspection"
  inspection.mkdir(parents=True, exist_ok=True)
  (inspection / "imports.txt").write_text(imports_output, encoding="utf-8", newline="\n")

  dry_run = (evidence_dir / "link" / "driver-dry-run.stderr.bin").read_bytes().decode("utf-8", errors="replace")
  linker_arguments = _linker_arguments(dry_run)
  verbose_link = b"\n".join(
    (
      (evidence_dir / "link" / "final-link.stdout.bin").read_bytes(),
      (evidence_dir / "link" / "final-link.stderr.bin").read_bytes(),
    )
  ).decode("utf-8", errors="replace")
  search_paths = _library_search_paths(linker_arguments)
  library_names = _library_names(linker_arguments, verbose_link)
  loaded_library_names = set(_loaded_library_names(verbose_link))
  resolved = {
    name: path for name in library_names if (path := _resolve_library(name, search_paths, linker_arguments)) is not None
  }
  roles = {}
  for role, basename in REQUIRED_STATIC_ROLES.items():
    path = resolved.get(basename)
    if path is None or basename not in loaded_library_names:
      continue
    members = _archive_members(path)
    archive = {
      "basename": basename,
      "members": members,
      "path": str(path),
      "sha256": _sha256(path.read_bytes()),
    }
    roles[role] = archive

  linker_path = linker_arguments[0]
  visual_studio = _visual_studio_identity()
  report = {
    "artifact_binding": artifact_binding,
    "compiler": _tool_identity(invocation["command"][0]),
    "imports": parse_windows_imports(imports_output),
    "link_trace_raw_dll_sha256": invocation["raw_dll_sha256"],
    "linked_copy_raw_dll_sha256": _sha256(linked_dll.read_bytes()),
    "linker": _tool_identity(linker_path),
    "mt_request": _mt_request(producer),
    "normalized_pe": normalized_pe_fingerprint(packaged_data),
    "packaged_artifact": packaged_identity,
    "producer": producer,
    "raw_dll_sha256": _sha256(packaged_data),
    "response_expanded": not any(argument.startswith("@") for argument in invocation["expanded_arguments"]),
    "response_files": invocation["response_files"],
    "runner": _runner_identity(search_paths),
    "schema": 1,
    "selected_default_libraries": library_names,
    "static_library_roles": roles,
    "terms": _terms_evidence(evidence_dir, visual_studio),
    "visual_studio": visual_studio,
  }
  report_path = evidence_dir / "report.json"
  _write_json(report_path, report)
  contract = _read_json(contract_path, "Windows toolchain contract")
  errors = evaluate_contract(report, contract, mode, packaged_dll, producer)
  verdict = {"errors": errors, "mode": mode, "ok": not errors, "producer": producer, "schema": 1}
  _write_json(evidence_dir / "verdict.json", verdict)

  manifest = {}
  for path in sorted(item for item in evidence_dir.rglob("*") if item.is_file()):
    if path.name == "manifest.json":
      continue
    manifest[path.relative_to(evidence_dir).as_posix()] = _sha256(path.read_bytes())
  _write_json(evidence_dir / "manifest.json", {"files": manifest, "schema": 1})
  if github_output is not None:
    with github_output.open("a", encoding="utf-8", newline="\n") as output:
      output.write(f"report={report_path}\n")
      output.write(f"upload_raw={'false' if mode == 'standing' and not errors else 'true'}\n")


def enforce(evidence_dir: Path) -> None:
  verdict = _read_json(evidence_dir / "verdict.json", "Windows evidence verdict")
  _require(verdict.get("ok") is True, "; ".join(verdict.get("errors", [])) or "Windows evidence failed")


def main() -> None:
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest="command_name", required=True)

  link = subparsers.add_parser("link")
  link.add_argument("--evidence-dir", type=Path, required=True)
  link.add_argument("--producer", choices=("native", "wheel"), required=True)
  link.add_argument("command", nargs=argparse.REMAINDER)

  collect = subparsers.add_parser("collect")
  collect.add_argument("--evidence-dir", type=Path, required=True)
  collect.add_argument("--producer", choices=("native", "wheel"), required=True)
  collect.add_argument("--mode", choices=("capture", "standing", "verify-approved"), required=True)
  collect.add_argument("--binary", type=Path)
  collect.add_argument("--wheel", type=Path)
  collect.add_argument("--build-info", type=Path)
  collect.add_argument("--sidecar", type=Path)
  collect.add_argument("--contract", type=Path, default=CONTRACT_PATH)
  collect.add_argument("--github-output", type=Path)

  enforce_parser = subparsers.add_parser("enforce")
  enforce_parser.add_argument("--evidence-dir", type=Path, required=True)

  args = parser.parse_args()
  if args.command_name == "link":
    command = args.command[1:] if args.command and args.command[0] == "--" else args.command
    capture_link(args.evidence_dir, args.producer, command)
  elif args.command_name == "collect":
    collect_evidence(
      args.evidence_dir,
      args.producer,
      args.mode,
      args.binary,
      args.wheel,
      args.build_info,
      args.sidecar,
      args.contract,
      args.github_output,
    )
  else:
    enforce(args.evidence_dir)


if __name__ == "__main__":
  main()
