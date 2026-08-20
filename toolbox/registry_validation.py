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

import base64
import binascii
import hashlib
import hmac
import re
import time

from pathlib import Path
from urllib.parse import quote, unquote, urlparse

import requests

from toolbox.build_npm import PACKAGE_NAME as NPM_PACKAGE
from toolbox.release_validation import validate_release_candidate


PYPI_PACKAGE = "celestial-calendar"
PYPI_FILE_HOST = "files.pythonhosted.org"
NPM_REGISTRY_HOST = "registry.npmjs.org"
REQUEST_TIMEOUT = 30
POLL_ATTEMPTS = 30
POLL_DELAY_SECONDS = 10


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


def pypi_metadata_url(version: str) -> str:
  """Return the version-specific PyPI JSON endpoint."""
  return f"https://pypi.org/pypi/{PYPI_PACKAGE}/{quote(version, safe='')}/json"


def npm_metadata_url(version: str) -> str:
  """Return the version-specific npm registry endpoint."""
  package = quote(NPM_PACKAGE, safe="@")
  return f"https://{NPM_REGISTRY_HOST}/{package}/{quote(version, safe='')}"


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
  if (
    len(expected) != hashlib.sha512().digest_size
    or not hmac.compare_digest(hashlib.sha512(content).digest(), expected)
  ):
    raise RuntimeError("npm registry SHA-512 integrity mismatch")


def npm_version_is_exact(tarball: Path, version: str, session: object = requests) -> bool:
  """Return false for an absent npm version; reject anything present but non-identical."""
  metadata = _metadata(npm_metadata_url(version), session)
  if metadata is None:
    return False
  if metadata.get("name") != NPM_PACKAGE or metadata.get("version") != version:
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


def pypi_version_is_exact(wheels: list[Path], version: str, session: object = requests) -> bool:
  """Return false for an absent PyPI version; otherwise prove all four wheel bytes."""
  metadata = _metadata(pypi_metadata_url(version), session)
  if metadata is None:
    return False
  info = metadata.get("info")
  if not isinstance(info, dict) or info.get("name") != PYPI_PACKAGE or info.get("version") != version:
    raise RuntimeError("PyPI registry package identity mismatch")
  urls = metadata.get("urls")
  if not isinstance(urls, list) or any(not isinstance(entry, dict) for entry in urls):
    raise RuntimeError("PyPI registry has no valid file inventory")
  entries = {}
  for entry in urls:
    filename = entry.get("filename")
    if not isinstance(filename, str) or filename in entries:
      raise RuntimeError("PyPI registry contains an invalid or duplicate filename")
    entries[filename] = entry
  expected = {wheel.name: wheel for wheel in wheels}
  if len(expected) != 4:
    raise RuntimeError("PyPI candidate must contain four distinct wheels")
  if set(entries) < set(expected):
    raise RegistryPendingError(
      f"PyPI wheel inventory is still incomplete: missing={sorted(set(expected) - set(entries))}"
    )
  if set(entries) != set(expected):
    raise RuntimeError(
      f"PyPI wheel inventory mismatch: missing={sorted(set(expected) - set(entries))}, "
      f"extra={sorted(set(entries) - set(expected))}"
    )
  for filename, wheel in expected.items():
    entry = entries[filename]
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
  return True


def classify_npm_candidate(candidate: Path, version: str, commit: str, session: object = requests) -> bool:
  """Return whether npm publication is required after validating the complete candidate."""
  validate_release_candidate(candidate, f"v{version}", commit)
  tarballs = list((candidate / "npm").glob("*.tgz"))
  if len(tarballs) != 1:
    raise RuntimeError("npm candidate must contain exactly one tarball")
  return not npm_version_is_exact(tarballs[0], version, session)


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
  wheels = sorted((candidate / "pypi").glob("*.whl"))
  tarballs = list((candidate / "npm").glob("*.tgz"))
  if len(wheels) != 4 or len(tarballs) != 1:
    raise RuntimeError("Invalid registry candidate inventory")

  pypi_ready = False
  npm_ready = False
  pending_reasons = {"PyPI": "version is absent", "npm": "version is absent"}
  for attempt in range(1, attempts + 1):
    if not pypi_ready:
      try:
        pypi_ready = pypi_version_is_exact(wheels, version, session)
        pending_reasons["PyPI"] = "version is absent"
      except RegistryPendingError as error:
        pending_reasons["PyPI"] = str(error)
    if not npm_ready:
      try:
        npm_ready = npm_version_is_exact(tarballs[0], version, session)
        pending_reasons["npm"] = "version is absent"
      except RegistryPendingError as error:
        pending_reasons["npm"] = str(error)
    if pypi_ready and npm_ready:
      return
    if attempt != attempts:
      sleep(delay_seconds)
  pending = [
    f"{name} ({pending_reasons[name]})"
    for name, ready in (("PyPI", pypi_ready), ("npm", npm_ready))
    if not ready
  ]
  raise RuntimeError(f"Registry version did not become available after {attempts} attempts: {'; '.join(pending)}")
