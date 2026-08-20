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
import hashlib

from types import SimpleNamespace

import pytest
import requests

from toolbox import registry_validation, registry_verifier
from toolbox.registry_validation import (
  NPM_PACKAGE,
  PYPI_PACKAGE,
  RegistryPendingError,
  npm_metadata_url,
  npm_version_is_exact,
  pypi_metadata_url,
  pypi_version_is_exact,
  wait_for_candidate_registries,
)


VERSION = "0.6.0"
NPM_TARBALL_URL = f"https://registry.npmjs.org/@0xf3cd/celestial/-/celestial-{VERSION}.tgz"


class Response:
  def __init__(self, *, status=200, payload=None, content=b"", url=""):
    self.status_code = status
    self.payload = payload
    self.content = content
    self.url = url

  def raise_for_status(self):
    if self.status_code >= 400:
      raise requests.HTTPError(f"status {self.status_code}")

  def json(self):
    return self.payload


class Session:
  def __init__(self, routes):
    self.routes = routes
    self.calls = []

  def get(self, url, *, timeout):
    self.calls.append((url, timeout))
    response = self.routes[url]
    if isinstance(response, list):
      response = response.pop(0)
    if isinstance(response, requests.RequestException):
      raise response
    if not response.url:
      response.url = url
    return response


def npm_metadata(content, *, integrity=None, name=NPM_PACKAGE, version=VERSION, url=NPM_TARBALL_URL):
  digest = base64.b64encode(hashlib.sha512(content).digest()).decode()
  return {
    "name": name,
    "version": version,
    "dist": {"tarball": url, "integrity": integrity or f"sha512-{digest}"},
  }


def pypi_wheels(tmp_path):
  wheels = []
  for index in range(4):
    path = tmp_path / f"celestial_calendar-{VERSION}-py3-none-platform_{index}.whl"
    path.write_bytes(f"wheel-{index}".encode())
    wheels.append(path)
  return wheels


def pypi_metadata(wheels):
  return {
    "info": {"name": PYPI_PACKAGE, "version": VERSION},
    "urls": [
      {
        "filename": wheel.name,
        "packagetype": "bdist_wheel",
        "size": wheel.stat().st_size,
        "digests": {"sha256": hashlib.sha256(wheel.read_bytes()).hexdigest()},
        "url": f"https://files.pythonhosted.org/packages/{wheel.name}",
      }
      for wheel in wheels
    ],
  }


def pypi_session(wheels, metadata=None):
  payload = metadata or pypi_metadata(wheels)
  routes = {pypi_metadata_url(VERSION): Response(payload=payload)}
  routes.update({
    entry["url"]: Response(content=wheel.read_bytes())
    for entry, wheel in zip(payload["urls"], wheels, strict=False)
  })
  return Session(routes)


def test_absent_npm_version_requires_publication(tmp_path):
  tarball = tmp_path / "package.tgz"
  tarball.write_bytes(b"candidate")
  session = Session({npm_metadata_url(VERSION): Response(status=404)})

  assert not npm_version_is_exact(tarball, VERSION, session)
  assert [url for url, _timeout in session.calls] == [npm_metadata_url(VERSION)]


def test_existing_npm_version_requires_exact_bytes_and_sha512(tmp_path):
  content = b"candidate"
  tarball = tmp_path / "package.tgz"
  tarball.write_bytes(content)
  session = Session({
    npm_metadata_url(VERSION): Response(payload=npm_metadata(content)),
    NPM_TARBALL_URL: Response(content=content),
  })

  assert npm_version_is_exact(tarball, VERSION, session)


@pytest.mark.parametrize(
  ("metadata", "remote", "message"),
  [
    (npm_metadata(b"remote", name="wrong"), b"remote", "package identity"),
    (npm_metadata(b"candidate", integrity="sha512-invalid"), b"candidate", "invalid SHA-512"),
    (npm_metadata(b"other"), b"candidate", "SHA-512 integrity mismatch"),
    (npm_metadata(b"remote"), b"remote", "SHA-256 does not match the release candidate"),
  ],
)
def test_existing_npm_version_rejects_any_identity_mismatch(tmp_path, metadata, remote, message):
  tarball = tmp_path / "package.tgz"
  tarball.write_bytes(b"candidate")
  session = Session({
    npm_metadata_url(VERSION): Response(payload=metadata),
    NPM_TARBALL_URL: Response(content=remote),
  })

  with pytest.raises(RuntimeError, match=message) as error:
    npm_version_is_exact(tarball, VERSION, session)
  assert not isinstance(error.value, RegistryPendingError)


def test_pypi_version_requires_four_exact_wheel_streams(tmp_path):
  wheels = pypi_wheels(tmp_path)

  assert pypi_version_is_exact(wheels, VERSION, pypi_session(wheels))


def test_absent_pypi_version_remains_pending(tmp_path):
  wheels = pypi_wheels(tmp_path)
  session = Session({pypi_metadata_url(VERSION): Response(status=404)})

  assert not pypi_version_is_exact(wheels, VERSION, session)


@pytest.mark.parametrize("mutation", ["sdist", "hash", "size", "extra"])
def test_pypi_version_rejects_inventory_and_metadata_mutations(tmp_path, mutation):
  wheels = pypi_wheels(tmp_path)
  metadata = pypi_metadata(wheels)
  if mutation == "sdist":
    metadata["urls"][0]["packagetype"] = "sdist"
  elif mutation == "hash":
    metadata["urls"][0]["digests"]["sha256"] = "0" * 64
  elif mutation == "size":
    metadata["urls"][0]["size"] += 1
  else:
    metadata["urls"].append({**metadata["urls"][0], "filename": "extra.whl"})

  with pytest.raises(RuntimeError) as error:
    pypi_version_is_exact(wheels, VERSION, pypi_session(wheels, metadata))
  assert not isinstance(error.value, RegistryPendingError)


@pytest.mark.parametrize(("field", "value"), [("size", True), ("size", -1), ("sha256", 0)])
def test_pypi_version_rejects_invalid_file_identity_types(tmp_path, field, value):
  wheels = pypi_wheels(tmp_path)
  metadata = pypi_metadata(wheels)
  target = metadata["urls"][0]
  if field == "sha256":
    target["digests"][field] = value
  else:
    target[field] = value

  with pytest.raises(RuntimeError, match="Invalid PyPI file identity") as error:
    pypi_version_is_exact(wheels, VERSION, pypi_session(wheels, metadata))
  assert not isinstance(error.value, RegistryPendingError)


@pytest.mark.parametrize("mutation", ["bytes", "url", "redirect"])
def test_pypi_version_rejects_file_transport_mutations(tmp_path, mutation):
  wheels = pypi_wheels(tmp_path)
  metadata = pypi_metadata(wheels)
  if mutation == "url":
    metadata["urls"][0]["url"] = f"http://example.invalid/{wheels[0].name}"
  session = pypi_session(wheels, metadata)
  first_url = metadata["urls"][0]["url"]
  if mutation == "bytes":
    session.routes[first_url].content = b"different-wheel"
  elif mutation == "redirect":
    session.routes[first_url].url = f"https://example.invalid/{wheels[0].name}"

  with pytest.raises(RuntimeError) as error:
    pypi_version_is_exact(wheels, VERSION, session)
  assert not isinstance(error.value, RegistryPendingError)


def test_npm_classification_validates_the_complete_candidate_first(monkeypatch, tmp_path):
  def reject_candidate(*_args):
    raise RuntimeError("invalid complete candidate")

  monkeypatch.setattr(registry_validation, "validate_release_candidate", reject_candidate)

  with pytest.raises(RuntimeError, match="invalid complete candidate"):
    registry_validation.classify_npm_candidate(tmp_path / "candidate", VERSION, "tagged-sha")


@pytest.mark.parametrize("transient", ["connection", "metadata-503", "partial-inventory", "file-503"])
def test_registry_polling_retries_transient_registry_states(monkeypatch, tmp_path, transient):
  candidate = tmp_path / "candidate"
  (candidate / "pypi").mkdir(parents=True)
  (candidate / "npm").mkdir()
  wheels = pypi_wheels(candidate / "pypi")
  tarball = candidate / "npm" / "package.tgz"
  tarball.write_bytes(b"candidate")
  complete = pypi_metadata(wheels)
  if transient == "connection":
    metadata_responses = [requests.ConnectionError("connection reset"), Response(payload=complete)]
  elif transient == "metadata-503":
    metadata_responses = [Response(status=503), Response(payload=complete)]
  elif transient == "partial-inventory":
    metadata_responses = [
      Response(payload={**complete, "urls": complete["urls"][:-1]}),
      Response(payload=complete),
    ]
  else:
    metadata_responses = [Response(payload=complete), Response(payload=complete)]
  routes = {
    pypi_metadata_url(VERSION): metadata_responses,
    npm_metadata_url(VERSION): Response(payload=npm_metadata(tarball.read_bytes())),
    NPM_TARBALL_URL: Response(content=tarball.read_bytes()),
  }
  routes.update({
    entry["url"]: Response(content=wheel.read_bytes())
    for entry, wheel in zip(complete["urls"], wheels, strict=True)
  })
  if transient == "file-503":
    first_url = complete["urls"][0]["url"]
    routes[first_url] = [Response(status=503), routes[first_url]]
  session = Session(routes)
  sleeps = []
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})

  wait_for_candidate_registries(
    candidate,
    VERSION,
    "tagged-sha",
    session=session,
    attempts=2,
    delay_seconds=3,
    sleep=sleeps.append,
  )

  called_urls = [url for url, _timeout in session.calls]
  assert called_urls.count(pypi_metadata_url(VERSION)) == 2
  assert called_urls.count(npm_metadata_url(VERSION)) == 1
  assert sleeps == [3]


def test_registry_polling_aborts_immediately_on_identity_mismatch(monkeypatch, tmp_path):
  candidate = tmp_path / "candidate"
  (candidate / "pypi").mkdir(parents=True)
  (candidate / "npm").mkdir()
  wheels = pypi_wheels(candidate / "pypi")
  (candidate / "npm" / "package.tgz").write_bytes(b"candidate")
  session = pypi_session(wheels)
  session.routes[session.routes[pypi_metadata_url(VERSION)].payload["urls"][0]["url"]].content = b"wrong"
  sleeps = []
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})

  with pytest.raises(RuntimeError, match="PyPI file bytes") as error:
    wait_for_candidate_registries(
      candidate,
      VERSION,
      "tagged-sha",
      session=session,
      attempts=2,
      delay_seconds=3,
      sleep=sleeps.append,
    )

  assert not isinstance(error.value, RegistryPendingError)
  assert sleeps == []


def test_registry_polling_retains_each_success(monkeypatch, tmp_path):
  candidate = tmp_path / "candidate"
  (candidate / "pypi").mkdir(parents=True)
  (candidate / "npm").mkdir()
  for wheel in pypi_wheels(candidate / "pypi"):
    assert wheel.is_file()
  (candidate / "npm" / "package.tgz").write_bytes(b"candidate")
  pypi_results = iter((False, True))
  npm_results = iter((False, False, True))
  calls = {"pypi": 0, "npm": 0}
  sleeps = []
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})

  def pypi(*_args):
    calls["pypi"] += 1
    return next(pypi_results)

  def npm(*_args):
    calls["npm"] += 1
    return next(npm_results)

  monkeypatch.setattr(registry_validation, "pypi_version_is_exact", pypi)
  monkeypatch.setattr(registry_validation, "npm_version_is_exact", npm)

  wait_for_candidate_registries(
    candidate,
    VERSION,
    "tagged-sha",
    attempts=3,
    delay_seconds=7,
    sleep=sleeps.append,
  )

  assert calls == {"pypi": 2, "npm": 3}
  assert sleeps == [7, 7]


def test_registry_polling_has_a_fixed_ceiling(monkeypatch, tmp_path):
  candidate = tmp_path / "candidate"
  (candidate / "pypi").mkdir(parents=True)
  (candidate / "npm").mkdir()
  pypi_wheels(candidate / "pypi")
  (candidate / "npm" / "package.tgz").write_bytes(b"candidate")
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})
  monkeypatch.setattr(registry_validation, "pypi_version_is_exact", lambda *_args: False)
  monkeypatch.setattr(registry_validation, "npm_version_is_exact", lambda *_args: False)
  sleeps = []

  with pytest.raises(RuntimeError, match=r"after 2 attempts: PyPI \(version is absent\); npm \(version is absent\)"):
    wait_for_candidate_registries(
      candidate,
      VERSION,
      "tagged-sha",
      attempts=2,
      delay_seconds=3,
      sleep=sleeps.append,
    )

  assert sleeps == [3]


def test_registry_polling_ceiling_reports_the_last_transient_state(monkeypatch, tmp_path):
  candidate = tmp_path / "candidate"
  (candidate / "pypi").mkdir(parents=True)
  (candidate / "npm").mkdir()
  pypi_wheels(candidate / "pypi")
  (candidate / "npm" / "package.tgz").write_bytes(b"candidate")
  session = Session({
    pypi_metadata_url(VERSION): [Response(status=503), Response(status=503)],
    npm_metadata_url(VERSION): Response(status=404),
  })
  sleeps = []
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})

  with pytest.raises(RuntimeError, match=r"PyPI \(Registry returned HTTP 503.*npm \(version is absent\)"):
    wait_for_candidate_registries(
      candidate,
      VERSION,
      "tagged-sha",
      session=session,
      attempts=2,
      delay_seconds=3,
      sleep=sleeps.append,
    )

  assert sleeps == [3]


def test_registry_cli_records_the_npm_decision_in_github_step_files(monkeypatch, tmp_path):
  output = tmp_path / "output"
  summary = tmp_path / "summary"
  output.write_text("prior=value\n", encoding="utf-8")
  summary.write_text("## Candidate\n", encoding="utf-8")
  monkeypatch.setattr(
    registry_verifier,
    "parse_args",
    lambda: SimpleNamespace(
      command="classify-npm",
      candidate=tmp_path / "candidate",
      version=VERSION,
      commit="tagged-sha",
      github_output=output,
      github_summary=summary,
    ),
  )
  monkeypatch.setattr(registry_verifier, "classify_npm_candidate", lambda *_args: False)

  registry_verifier.main()

  assert output.read_text(encoding="utf-8") == "prior=value\npublish_required=false\n"
  assert summary.read_text(encoding="utf-8") == "## Candidate\n- npm publication required: `false`\n"


def test_registry_cli_verify_uses_no_github_step_files(monkeypatch, tmp_path):
  calls = []
  monkeypatch.setattr(
    registry_verifier,
    "parse_args",
    lambda: SimpleNamespace(
      command="verify",
      candidate=tmp_path / "candidate",
      version=VERSION,
      commit="tagged-sha",
      github_output=None,
      github_summary=None,
    ),
  )
  monkeypatch.setattr(registry_verifier, "wait_for_candidate_registries", lambda *args: calls.append(args))

  registry_verifier.main()

  assert calls == [((tmp_path / "candidate").resolve(), VERSION, "tagged-sha")]
