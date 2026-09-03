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

import hashlib
import struct

from pathlib import Path
from types import SimpleNamespace

import pytest
import yaml

from toolbox import windows_toolchain_evidence as evidence


REPO = Path(__file__).resolve().parents[2]


def pe_bytes(timestamp: int = 1, payload: int = 0) -> bytes:
  data = bytearray(0x108)
  data[:2] = b"MZ"
  struct.pack_into("<I", data, 0x3C, 0x80)
  data[0x80:0x84] = b"PE\0\0"
  struct.pack_into("<H", data, 0x84, 0x8664)
  struct.pack_into("<I", data, 0x88, timestamp)
  struct.pack_into("<H", data, 0x94, 0x70)
  struct.pack_into("<H", data, 0x98, 0x20B)
  data[-1] = payload
  return bytes(data)


def sha256(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def report_for(data: bytes, producer: str = "native") -> dict:
  roles = {
    role: {"basename": basename, "members": ["member.obj"], "path": basename, "sha256": "a" * 64}
    for role, basename in evidence.REQUIRED_STATIC_ROLES.items()
  }
  return {
    "artifact_binding": True,
    "compiler": {"path": "clang++.exe", "sha256": "b" * 64, "version": "22.1.7"},
    "imports": ["KERNEL32.DLL"],
    "link_trace_raw_dll_sha256": sha256(data),
    "linked_copy_raw_dll_sha256": sha256(data),
    "linker": {"path": "lld-link.exe", "sha256": "c" * 64, "version": "22.1.7"},
    "mt_request": evidence._mt_request(producer),
    "normalized_pe": evidence.normalized_pe_fingerprint(data),
    "producer": producer,
    "raw_dll_sha256": sha256(data),
    "response_expanded": True,
    "response_files": [{"path": "link.rsp", "sha256": "d" * 64}],
    "runner": {"ImageOS": "win25", "ImageVersion": "1"},
    "schema": 1,
    "selected_default_libraries": sorted(evidence.REQUIRED_STATIC_ROLES.values()),
    "static_library_roles": roles,
    "terms": {
      "documents": [],
      "identity_unrecovered": True,
      "searched_roots": ["C:/Program Files/Microsoft Visual Studio"],
      "terms_text_captured": False,
    },
    "visual_studio": {"installation_version": "18"},
  }


def contract_for(report: dict) -> dict:
  producer = report["producer"]
  return {
    "approved_evidence": {"native": None, "wheel": None} | {producer: evidence.approved_view(report)},
    "schema": 1,
    "standing_contract": {"native": None, "wheel": None} | {producer: evidence.standing_view(report)},
  }


def test_pe_normalization_zeros_only_coff_timestamp():
  first = pe_bytes(timestamp=1)
  second = pe_bytes(timestamp=2)
  changed = pe_bytes(timestamp=2, payload=1)

  assert sha256(first) != sha256(second)
  assert evidence.normalized_pe_fingerprint(first)["sha256"] == evidence.normalized_pe_fingerprint(second)["sha256"]
  assert evidence.normalized_pe_fingerprint(first)["sha256"] != evidence.normalized_pe_fingerprint(changed)["sha256"]


@pytest.mark.parametrize(
  ("mutation", "message"),
  [
    (lambda data: b"", "DOS header"),
    (lambda data: bytes(bytearray(data[:0x80]) + b"NOPE" + data[0x84:]), "signature"),
    (
      lambda data: bytes(bytearray(data[:0x84]) + struct.pack("<H", 0x14C) + data[0x86:]),
      "not AMD64",
    ),
    (
      lambda data: bytes(bytearray(data[:0x98]) + struct.pack("<H", 0x10B) + data[0x9A:]),
      "not PE32",
    ),
  ],
  ids=["truncated", "signature", "machine", "optional-magic"],
)
def test_pe_normalization_rejects_malformed_inputs(mutation, message):
  with pytest.raises(RuntimeError, match=message):
    evidence.normalized_pe_fingerprint(mutation(pe_bytes()))


def test_windows_import_inventory_is_exact_and_nonempty():
  output = "Import {\n Name: KERNEL32.dll\n}\nImport {\n Name: USER32.dll\n}\n"
  assert evidence.parse_windows_imports(output) == ["KERNEL32.DLL", "USER32.DLL"]
  with pytest.raises(RuntimeError, match="empty or malformed"):
    evidence.parse_windows_imports("")
  with pytest.raises(RuntimeError, match="duplicate"):
    evidence.parse_windows_imports("Name: KERNEL32.dll\nName: kernel32.DLL")


def test_windows_command_line_declares_pointer_return_types(monkeypatch):
  storage = (evidence.ctypes.c_wchar_p * 2)("clang++.exe", "two words.obj")
  freed = []

  def command_line_to_argv(_command_line, argc):
    evidence.ctypes.cast(argc, evidence.ctypes.POINTER(evidence.ctypes.c_int))[0] = 2
    return evidence.ctypes.cast(storage, evidence.ctypes.POINTER(evidence.ctypes.c_wchar_p))

  def local_free(pointer):
    freed.append(pointer)
    return None

  command_line_to_argv.argtypes = None
  command_line_to_argv.restype = None
  local_free.argtypes = None
  local_free.restype = None
  windll = SimpleNamespace(
    shell32=SimpleNamespace(CommandLineToArgvW=command_line_to_argv),
    kernel32=SimpleNamespace(LocalFree=local_free),
  )
  monkeypatch.setattr(evidence, "os", SimpleNamespace(name="nt"))
  monkeypatch.setattr(evidence.ctypes, "windll", windll, raising=False)

  assert evidence._windows_command_line('clang++.exe "two words.obj"') == ["clang++.exe", "two words.obj"]
  assert command_line_to_argv.argtypes == (
    evidence.ctypes.c_wchar_p,
    evidence.ctypes.POINTER(evidence.ctypes.c_int),
  )
  assert command_line_to_argv.restype == evidence.ctypes.POINTER(evidence.ctypes.c_wchar_p)
  assert local_free.argtypes == (evidence.ctypes.c_void_p,)
  assert local_free.restype == evidence.ctypes.c_void_p
  assert len(freed) == 1


def test_excluded_default_libraries_do_not_count_as_selected():
  assert (
    evidence._library_names(
      ["lld-link.exe", "/DEFAULTLIB:libcmt", "/NODEFAULTLIB:libcmt.lib"],
      "",
    )
    == []
  )
  assert (
    evidence._library_names(
      ["lld-link.exe", "/DEFAULTLIB:libcmt", "/NODEFAULTLIB:libcmt.lib"],
      "Searching C:/toolchain/libcmt.lib",
    )
    == []
  )
  assert evidence._library_names(
    ["lld-link.exe", "/DEFAULTLIB:libcmt", "/NODEFAULTLIB:libcmt.lib"],
    "Loaded C:/toolchain/libcmt.lib(member.obj)",
  ) == ["libcmt.lib"]
  assert (
    evidence._library_names(
      ["lld-link.exe", "/DEFAULTLIB:libcmt", "/NODEFAULTLIB"],
      "",
    )
    == []
  )


def test_microsoft_terms_are_searched_and_retained(monkeypatch, tmp_path):
  installation = tmp_path / "Visual Studio"
  installation.mkdir()
  terms = installation / "License.txt"
  terms.write_text("MICROSOFT SOFTWARE LICENSE TERMS\nMicrosoft Visual Studio\n", encoding="utf-8")
  monkeypatch.setenv("ProgramFiles(x86)", str(tmp_path / "missing"))

  captured = evidence._terms_evidence(tmp_path / "evidence", {"installation_path": str(installation)})

  assert captured["identity_unrecovered"] is False
  assert captured["terms_text_captured"] is True
  assert captured["searched_roots"] == [str(installation)]
  assert len(captured["documents"]) == 1
  document = captured["documents"][0]
  assert document["path"] == str(terms)
  assert (tmp_path / "evidence" / document["retained_path"]).read_bytes() == terms.read_bytes()

  empty_installation = tmp_path / "Empty Visual Studio"
  empty_installation.mkdir()
  missing = evidence._terms_evidence(
    tmp_path / "other-evidence",
    {"installation_path": str(empty_installation)},
  )
  assert missing["identity_unrecovered"] is True
  assert missing["terms_text_captured"] is False
  assert missing["documents"] == []


def test_capture_requires_positive_static_roles_and_rejects_dynamic_runtime(tmp_path):
  data = pe_bytes()
  binary = tmp_path / "library.dll"
  binary.write_bytes(data)
  report = report_for(data)
  empty_contract = {
    "approved_evidence": {"native": None, "wheel": None},
    "schema": 1,
    "standing_contract": {"native": None, "wheel": None},
  }
  assert evidence.evaluate_contract(report, empty_contract, "capture", binary, "native") == []

  missing_role = report | {"static_library_roles": {"ucrt": report["static_library_roles"]["ucrt"]}}
  assert (
    "positive static CRT/UCRT"
    in evidence.evaluate_contract(missing_role, empty_contract, "capture", binary, "native")[0]
  )
  dynamic = report | {"imports": ["KERNEL32.DLL", "UCRTBASE.DLL"]}
  assert "dynamic runtime" in evidence.evaluate_contract(dynamic, empty_contract, "capture", binary, "native")[0]

  duplicated_import = report | {"imports": ["KERNEL32.DLL", "KERNEL32.DLL"]}
  assert (
    "malformed or duplicated"
    in evidence.evaluate_contract(duplicated_import, empty_contract, "capture", binary, "native")[0]
  )

  hollow_roles = {role: {"basename": basename} for role, basename in evidence.REQUIRED_STATIC_ROLES.items()}
  hollow_report = report | {"static_library_roles": hollow_roles}
  assert (
    "role evidence differs" in evidence.evaluate_contract(hollow_report, empty_contract, "capture", binary, "native")[0]
  )


def test_malformed_contract_profiles_are_deferred_to_the_verdict(tmp_path):
  data = pe_bytes()
  binary = tmp_path / "library.dll"
  binary.write_bytes(data)
  report = report_for(data)

  malformed = {"approved_evidence": [], "schema": 1, "standing_contract": []}
  assert evidence.evaluate_contract(report, malformed, "standing", binary, "native") == [
    "Windows standing_contract contract inventory differs"
  ]


def test_standing_contract_uses_exact_per_producer_imports(tmp_path):
  data = pe_bytes()
  binary = tmp_path / "library.dll"
  binary.write_bytes(data)
  report = report_for(data)
  contract = contract_for(report)
  assert evidence.evaluate_contract(report, contract, "standing", binary, "native") == []

  changed = report | {"imports": ["KERNEL32.DLL", "USER32.DLL"]}
  assert (
    "Standing Windows contract differs"
    in evidence.evaluate_contract(changed, contract, "standing", binary, "native")[0]
  )
  assert "producer differs" in evidence.evaluate_contract(report, contract, "standing", binary, "wheel")[0]


def test_approved_profile_compares_normalized_pe_not_cross_run_raw_hash(tmp_path):
  first_data = pe_bytes(timestamp=1)
  second_data = pe_bytes(timestamp=2)
  second_binary = tmp_path / "library.dll"
  second_binary.write_bytes(second_data)
  first = report_for(first_data)
  second = report_for(second_data)
  contract = contract_for(first)
  contract["standing_contract"]["native"] = evidence.standing_view(second)

  assert first["raw_dll_sha256"] != second["raw_dll_sha256"]
  assert evidence.evaluate_contract(second, contract, "verify-approved", second_binary, "native") == []

  changed_data = pe_bytes(timestamp=2, payload=1)
  changed_binary = tmp_path / "changed.dll"
  changed_binary.write_bytes(changed_data)
  changed = report_for(changed_data)
  assert (
    "Approved Windows evidence profile differs"
    in evidence.evaluate_contract(changed, contract, "verify-approved", changed_binary, "native")[0]
  )


def test_response_files_are_retained_and_fully_expanded(tmp_path):
  evidence_dir = tmp_path / "evidence"
  nested = tmp_path / "nested.rsp"
  nested.write_text('"two words.obj" kernel32.lib', encoding="utf-8")
  outer = tmp_path / "outer.rsp"
  outer.write_text(f'one.obj "@{nested}"', encoding="utf-8")

  expanded, records = evidence._expand_response_files([f"@{outer}", "-o", "library.dll"], evidence_dir)

  assert expanded == ["one.obj", "two words.obj", "kernel32.lib", "-o", "library.dll"]
  assert len(records) == 2
  assert all((evidence_dir / record["retained_path"]).is_file() for record in records)
  with pytest.raises(RuntimeError, match="does not exist"):
    evidence._expand_response_files([f"@{tmp_path / 'missing.rsp'}"], evidence_dir)


def test_windows_link_capture_is_target_specific_and_not_installed():
  cmake = (REPO / "src" / "shared_lib" / "CMakeLists.txt").read_text(encoding="utf-8")
  combined = cmake + (REPO / "src" / "CMakeLists.txt").read_text(encoding="utf-8")
  combined += (REPO / "bindings" / "python" / "CMakeLists.txt").read_text(encoding="utf-8")

  assert "CXX_LINKER_LAUNCHER" in cmake
  assert 'target_link_options(celestial_calendar PRIVATE "LINKER:/VERBOSE:LIB")' in cmake
  assert "/Brepro" not in combined
  assert "MultiThreadedDLL" not in combined
  assert "install" not in "\n".join(line for line in cmake.splitlines() if "EVIDENCE" in line)


@pytest.mark.parametrize("relative", ["build_and_test.yml", "python-wheel.yml"])
def test_windows_evidence_workflows_expose_only_the_three_contract_modes(relative):
  workflow = yaml.safe_load((REPO / ".github" / "workflows" / relative).read_text(encoding="utf-8"))
  dispatch = workflow[True]["workflow_dispatch"]

  assert dispatch["inputs"]["windows_evidence_mode"] == {
    "default": "standing",
    "description": "Windows link-evidence policy",
    "options": ["standing", "capture", "verify-approved"],
    "required": True,
    "type": "choice",
  }
  windows_job = workflow["jobs"]["windows" if relative == "build_and_test.yml" else "windows-amd64"]
  assert windows_job["env"]["WINDOWS_EVIDENCE_MODE"] == (
    "${{ github.event_name == 'workflow_dispatch' && inputs.windows_evidence_mode || 'standing' }}"
  )


def test_native_evidence_upload_precedes_enforcement_and_consumer_artifact():
  workflow = yaml.safe_load((REPO / ".github" / "workflows" / "build_and_test.yml").read_text(encoding="utf-8"))
  steps = workflow["jobs"]["windows"]["steps"]
  names = [step.get("name") for step in steps]
  upload = steps[names.index("Upload raw Windows link evidence")]

  assert names.index("Ensure Shared Lib Exists") < names.index("Upload raw Windows link evidence")
  assert names.index("Upload raw Windows link evidence") < names.index("Enforce Windows link evidence")
  assert names.index("Enforce Windows link evidence") < names.index("GitHub Artifact")
  assert upload["with"]["name"] == "windows-link-evidence-native"
  assert "WINDOWS_EVIDENCE_MODE != 'standing'" in upload["if"]
  assert "failure()" in upload["if"]
