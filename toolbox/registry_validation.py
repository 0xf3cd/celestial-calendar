#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import base64
import binascii
import hashlib
import hmac
import re
import time
import shutil

from pathlib import Path
from urllib.parse import quote, unquote, urlparse

import requests

from toolbox.build_npm import ALIAS_NAME, PACKAGE_NAME as NPM_PACKAGE
from toolbox.release_validation import (
  PYTHON_ARTIFACTS,
  npm_candidate_tarballs,
  python_wheel_groups,
  validate_release_candidate,
)
from toolbox.build_python_aliases import PRIMARY_PACKAGE, PYPI_PACKAGES


PYPI_PACKAGE = PRIMARY_PACKAGE
PYPI_FILE_HOST = "files.pythonhosted.org"
NPM_REGISTRY_HOST = "registry.npmjs.org"
REQUEST_TIMEOUT = 30
POLL_ATTEMPTS = 30
POLL_DELAY_SECONDS = 10
NPM_LABELS = {NPM_PACKAGE: "npm-primary", ALIAS_NAME: "npm-alias"}


class RegistryPendingError(RuntimeError):
  """A registry transport or indexing state that may settle within the polling ceiling."""


def _registry_get(url: str, session: object):
  try:
    response = session.get(url, timeout=REQUEST_TIMEOUT)
  except requests.RequestException as error:
    raise RegistryPendingError(f"Registry request failed transiently for {url}: {error}") from error
  if response.status_code == 404:
    return response
  if response.status_code >= 500:
    raise RegistryPendingError(f"Registry returned HTTP {response.status_code} for {url}")
  response.raise_for_status()
  return response


def pypi_metadata_url(version: str, package_name: str = PYPI_PACKAGE) -> str:
  """Return the version-specific PyPI JSON endpoint."""
  if package_name not in PYPI_PACKAGES:
    raise ValueError(f"Unknown PyPI package: {package_name}")
  return f"https://pypi.org/pypi/{package_name}/{quote(version, safe='')}/json"


def npm_metadata_url(package_name: str, version: str | None = None) -> str:
  """Return the package-level or version-specific npm registry endpoint."""
  if package_name not in NPM_LABELS:
    raise ValueError(f"Unknown npm package: {package_name}")
  package = quote(package_name, safe="@")
  return f"https://{NPM_REGISTRY_HOST}/{package}" + (f"/{quote(version, safe='')}" if version is not None else "")


def _metadata(url: str, session: object) -> dict | None:
  response = _registry_get(url, session)
  if response.status_code == 404:
    return None
  try:
    payload = response.json()
  except (ValueError, UnicodeDecodeError) as error:
    raise RuntimeError(f"Registry returned invalid JSON for {url}: {error}") from error
  if not isinstance(payload, dict):
    raise RuntimeError(f"Registry returned a non-object for {url}")
  return payload


def _download(url: str, expected_host: str, session: object) -> bytes:
  parsed = urlparse(url)
  if parsed.scheme != "https" or parsed.hostname != expected_host:
    raise RuntimeError(f"Unexpected registry file URL: {url}")
  response = _registry_get(url, session)
  if response.status_code == 404:
    raise RegistryPendingError(f"Registry file is not yet available: {url}")
  final_url = getattr(response, "url", url)
  final = urlparse(final_url)
  if final.scheme != "https" or final.hostname != expected_host:
    raise RuntimeError(f"Registry file redirected outside {expected_host}: {final_url}")
  if not isinstance(response.content, bytes):
    raise RuntimeError(f"Registry returned a non-byte payload for {url}")
  return response.content


def _validate_ssri(content: bytes, integrity: object) -> None:
  if not isinstance(integrity, str):
    raise RuntimeError("npm metadata has no SHA-512 integrity")
  tokens = [token for token in integrity.split() if token.startswith("sha512-")]
  if len(tokens) != 1:
    raise RuntimeError("npm metadata must contain exactly one SHA-512 integrity")
  try:
    expected = base64.b64decode(tokens[0].removeprefix("sha512-"), validate=True)
  except (binascii.Error, ValueError) as error:
    raise RuntimeError("npm metadata contains invalid SHA-512 integrity") from error
  if len(expected) != hashlib.sha512().digest_size or not hmac.compare_digest(
    hashlib.sha512(content).digest(), expected
  ):
    raise RuntimeError("npm registry SHA-512 integrity mismatch")


def npm_version_is_exact(tarball: Path, version: str, package_name: str, session: object = requests) -> bool:
  """Return false for an absent npm version; reject anything present but non-identical."""
  metadata = _metadata(npm_metadata_url(package_name, version), session)
  if metadata is None:
    return False
  if metadata.get("name") != package_name or metadata.get("version") != version:
    raise RuntimeError("npm registry package identity mismatch")
  dist = metadata.get("dist")
  if not isinstance(dist, dict) or not isinstance(dist.get("tarball"), str):
    raise RuntimeError("npm registry metadata has no tarball")
  content = _download(dist["tarball"], NPM_REGISTRY_HOST, session)
  candidate = tarball.read_bytes()
  if not hmac.compare_digest(hashlib.sha256(content).digest(), hashlib.sha256(candidate).digest()):
    raise RuntimeError("npm registry tarball SHA-256 does not match the release candidate")
  if content != candidate:
    raise RuntimeError("npm registry tarball bytes do not match the release candidate")
  _validate_ssri(content, dist.get("integrity"))
  return True


def _pypi_missing_wheels(
  wheels: list[Path], version: str, package_name: str, session: object
) -> tuple[bool, list[Path]]:
  """Verify every listed file before returning any missing candidate wheels."""
  expected = {wheel.name: wheel for wheel in wheels}
  count = len(PYTHON_ARTIFACTS) if package_name == PYPI_PACKAGE else 1
  if package_name not in PYPI_PACKAGES or len(expected) != len(wheels) or len(expected) != count:
    raise RuntimeError(f"PyPI candidate for {package_name} must contain {count} distinct wheels")
  metadata = _metadata(pypi_metadata_url(version, package_name), session)
  if metadata is None:
    return False, list(wheels)
  info = metadata.get("info")
  if not isinstance(info, dict) or info.get("name") != package_name or info.get("version") != version:
    raise RuntimeError("PyPI registry package identity mismatch")
  urls = metadata.get("urls")
  if not isinstance(urls, list) or not urls or any(not isinstance(entry, dict) for entry in urls):
    raise RuntimeError("PyPI registry has no valid file inventory")
  entries = {}
  for entry in urls:
    filename = entry.get("filename")
    if not isinstance(filename, str) or filename in entries:
      raise RuntimeError("PyPI registry contains an invalid or duplicate filename")
    entries[filename] = entry
  if not set(entries) <= set(expected):
    raise RuntimeError(
      f"PyPI wheel inventory mismatch: missing={sorted(set(expected) - set(entries))}, "
      f"extra={sorted(set(entries) - set(expected))}"
    )
  for filename, entry in entries.items():
    wheel = expected[filename]
    digests = entry.get("digests")
    size = entry.get("size")
    url = entry.get("url")
    sha256 = digests.get("sha256") if isinstance(digests, dict) else None
    valid_digest = isinstance(sha256, str) and re.fullmatch(r"[0-9a-f]{64}", sha256) is not None
    valid_size = type(size) is int and size >= 0
    valid_url = isinstance(url, str) and unquote(Path(urlparse(url).path).name) == filename
    # JSON booleans are Python integers; a registry file size must be an exact integer.
    if (
      entry.get("packagetype") != "bdist_wheel"
      or entry.get("yanked", False) is not False
      or not valid_digest
      or not valid_size
      or not valid_url
    ):
      raise RuntimeError(f"Invalid PyPI file identity: {filename}")
    candidate = wheel.read_bytes()
    if size != len(candidate) or sha256 != hashlib.sha256(candidate).hexdigest():
      raise RuntimeError(f"PyPI metadata does not match the candidate: {filename}")
    if _download(url, PYPI_FILE_HOST, session) != candidate:
      raise RuntimeError(f"PyPI file bytes do not match the candidate: {filename}")
  return True, [wheel for wheel in wheels if wheel.name not in entries]


def pypi_version_is_exact(
  wheels: list[Path], version: str, session: object = requests, package_name: str = PYPI_PACKAGE
) -> bool:
  """Return false only for an absent version; poll verified incomplete inventories."""
  present, missing = _pypi_missing_wheels(wheels, version, package_name, session)
  if present and missing:
    raise RegistryPendingError(f"PyPI wheel inventory is still incomplete: missing={sorted(p.name for p in missing)}")
  return present


def wait_for_pypi(
  wheels: list[Path],
  version: str,
  package_name: str,
  session: object = requests,
  attempts: int = POLL_ATTEMPTS,
  delay_seconds: int = POLL_DELAY_SECONDS,
  sleep: object = time.sleep,
) -> None:
  """Wait for exact primary bytes before publishing its dependent aliases."""
  if attempts < 1 or delay_seconds < 0:
    raise ValueError("PyPI polling requires positive attempts and a non-negative delay")
  reason = "version is absent"
  for attempt in range(attempts):
    try:
      if pypi_version_is_exact(wheels, version, session, package_name):
        return
      reason = "version is absent"
    except RegistryPendingError as error:
      reason = str(error)
    if attempt + 1 < attempts:
      sleep(delay_seconds)
  raise RuntimeError(f"PyPI {package_name} did not become exact after {attempts} attempts: {reason}")


def prepare_pypi_candidate(
  candidate: Path,
  version: str,
  commit: str,
  package_name: str,
  upload_to: Path,
  session: object = requests,
) -> str:
  """Stage only proven-missing frozen wheels; exact versions are verified no-ops."""
  candidate = candidate.resolve()
  upload_to = upload_to.resolve()
  if upload_to.exists() or upload_to == candidate or candidate in upload_to.parents:
    raise FileExistsError(f"Refusing to overwrite or modify the PyPI candidate: {upload_to}")
  validate_release_candidate(candidate, f"v{version}", commit)
  groups = python_wheel_groups((candidate / "pypi").iterdir(), version)
  if package_name not in groups:
    raise ValueError(f"PyPI identity is not in candidate: {package_name}")
  if package_name != PYPI_PACKAGE:
    wait_for_pypi(groups[PYPI_PACKAGE], version, PYPI_PACKAGE, session)
  present, missing = _pypi_missing_wheels(groups[package_name], version, package_name, session)
  state = "absent" if not present else "partial" if missing else "exact"
  upload_to.mkdir(parents=True)
  for wheel in missing:
    target = upload_to / wheel.name
    shutil.copyfile(wheel, target)
    if target.read_bytes() != wheel.read_bytes():
      raise RuntimeError(f"PyPI upload staging byte mismatch: {wheel.name}")
  return state


def classify_npm_candidate(
  candidate: Path,
  version: str,
  commit: str,
  package_name: str,
  session: object = requests,
) -> str:
  """Return exact, absent or bootstrap_required; reject conflicts and uncertain registry responses."""
  validate_release_candidate(candidate, f"v{version}", commit)
  tarballs = npm_candidate_tarballs(candidate, version)
  if package_name not in tarballs:
    raise ValueError(f"npm identity is not in candidate: {package_name}")
  if npm_version_is_exact(tarballs[package_name], version, package_name, session):
    return "exact"
  metadata = _metadata(npm_metadata_url(package_name), session)
  if metadata is None:
    return "bootstrap_required"
  if metadata.get("name") != package_name:
    raise RuntimeError(f"npm package-level identity mismatch: {package_name}")
  return "absent"


def wait_for_candidate_registries(
  candidate: Path,
  version: str,
  commit: str,
  session: object = requests,
  attempts: int = POLL_ATTEMPTS,
  delay_seconds: int = POLL_DELAY_SECONDS,
  sleep: object = time.sleep,
) -> None:
  """Poll both registries to a fixed ceiling, retaining success across attempts."""
  if attempts < 1 or delay_seconds < 0:
    raise ValueError("Registry polling requires positive attempts and a non-negative delay")
  validate_release_candidate(candidate, f"v{version}", commit)
  groups = python_wheel_groups((candidate / "pypi").iterdir(), version)
  tarballs = npm_candidate_tarballs(candidate, version)

  pypi_ready = dict.fromkeys(groups, False)
  npm_ready = dict.fromkeys(tarballs, False)
  pypi_reasons = dict.fromkeys(groups, "version is absent")
  npm_reasons = dict.fromkeys(tarballs, "version is absent")
  for attempt in range(1, attempts + 1):
    for name, wheels in groups.items():
      if not pypi_ready[name]:
        try:
          pypi_ready[name] = pypi_version_is_exact(wheels, version, session, name)
          pypi_reasons[name] = "version is absent"
        except RegistryPendingError as error:
          pypi_reasons[name] = str(error)
        except (RuntimeError, requests.HTTPError) as error:
          raise RuntimeError(f"PyPI {name}: {error}") from error
    for name, tarball in tarballs.items():
      if not npm_ready[name]:
        try:
          npm_ready[name] = npm_version_is_exact(tarball, version, name, session)
          npm_reasons[name] = "version is absent"
        except RegistryPendingError as error:
          npm_reasons[name] = str(error)
        except (RuntimeError, requests.HTTPError) as error:
          raise RuntimeError(f"{NPM_LABELS[name]}: {error}") from error
    if all(pypi_ready.values()) and all(npm_ready.values()):
      return
    if attempt != attempts:
      sleep(delay_seconds)
  pending = [f"PyPI {name} ({pypi_reasons[name]})" for name, ready in pypi_ready.items() if not ready]
  pending.extend(f"{NPM_LABELS[name]} ({npm_reasons[name]})" for name, ready in npm_ready.items() if not ready)
  raise RuntimeError(f"Registry version did not become available after {attempts} attempts: {'; '.join(pending)}")
