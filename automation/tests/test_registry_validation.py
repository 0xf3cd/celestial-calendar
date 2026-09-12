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
import hashlib
import json

from types import SimpleNamespace

import pytest
import requests

from toolbox import registry_validation, registry_verifier
from toolbox.build_npm import ALIAS_NAME
from toolbox.release_validation import npm_candidate_tarballs, stage_release_candidate
from automation.tests.test_release_validation import VERSION as CURRENT_VERSION, write_candidate_inputs
from toolbox.registry_validation import (
  NPM_PACKAGE,
  PYPI_PACKAGE,
  RegistryPendingError,
  npm_metadata_url as package_metadata_url,
  npm_version_is_exact,
  pypi_metadata_url,
  pypi_version_is_exact,
  wait_for_candidate_registries,
)


VERSION = "0.6.0"
NPM_TARBALL_URL = f"https://registry.npmjs.org/@0xf3cd/celestial/-/celestial-{VERSION}.tgz"


def npm_metadata_url(version):
  return package_metadata_url(NPM_PACKAGE, version)


def candidate_metadata(candidate):
  evidence = candidate / "evidence"
  evidence.mkdir()
  (evidence / "npm-pack.json").write_text(
    json.dumps([{"name": NPM_PACKAGE, "version": VERSION, "filename": "package.tgz"}]), encoding="utf-8"
  )


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
    if isinstance(self.payload, Exception):
      raise self.payload
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
  routes.update(
    {entry["url"]: Response(content=wheel.read_bytes()) for entry, wheel in zip(payload["urls"], wheels, strict=False)}
  )
  return Session(routes)


def test_absent_npm_version_requires_publication(tmp_path):
  tarball = tmp_path / "package.tgz"
  tarball.write_bytes(b"candidate")
  session = Session({npm_metadata_url(VERSION): Response(status=404)})

  assert not npm_version_is_exact(tarball, VERSION, NPM_PACKAGE, session)
  assert [url for url, _timeout in session.calls] == [npm_metadata_url(VERSION)]


def test_existing_npm_version_requires_exact_bytes_and_sha512(tmp_path):
  content = b"candidate"
  tarball = tmp_path / "package.tgz"
  tarball.write_bytes(content)
  session = Session(
    {
      npm_metadata_url(VERSION): Response(payload=npm_metadata(content)),
      NPM_TARBALL_URL: Response(content=content),
    }
  )

  assert npm_version_is_exact(tarball, VERSION, NPM_PACKAGE, session)


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
  session = Session(
    {
      npm_metadata_url(VERSION): Response(payload=metadata),
      NPM_TARBALL_URL: Response(content=remote),
    }
  )

  with pytest.raises(RuntimeError, match=message) as error:
    npm_version_is_exact(tarball, VERSION, NPM_PACKAGE, session)
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
    registry_validation.classify_npm_candidate(tmp_path / "candidate", VERSION, "tagged-sha", NPM_PACKAGE)


@pytest.mark.parametrize("transient", ["connection", "metadata-503", "partial-inventory", "file-503"])
def test_registry_polling_retries_transient_registry_states(monkeypatch, tmp_path, transient):
  candidate = tmp_path / "candidate"
  (candidate / "pypi").mkdir(parents=True)
  (candidate / "npm").mkdir()
  candidate_metadata(candidate)
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
  routes.update(
    {entry["url"]: Response(content=wheel.read_bytes()) for entry, wheel in zip(complete["urls"], wheels, strict=True)}
  )
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
  candidate_metadata(candidate)
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
  candidate_metadata(candidate)
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
  candidate_metadata(candidate)
  pypi_wheels(candidate / "pypi")
  (candidate / "npm" / "package.tgz").write_bytes(b"candidate")
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})
  monkeypatch.setattr(registry_validation, "pypi_version_is_exact", lambda *_args: False)
  monkeypatch.setattr(registry_validation, "npm_version_is_exact", lambda *_args: False)
  sleeps = []

  with pytest.raises(
    RuntimeError, match=r"after 2 attempts: PyPI \(version is absent\); npm-primary \(version is absent\)"
  ):
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
  candidate_metadata(candidate)
  pypi_wheels(candidate / "pypi")
  (candidate / "npm" / "package.tgz").write_bytes(b"candidate")
  session = Session(
    {
      pypi_metadata_url(VERSION): [Response(status=503), Response(status=503)],
      npm_metadata_url(VERSION): Response(status=404),
    }
  )
  sleeps = []
  monkeypatch.setattr(registry_validation, "validate_release_candidate", lambda *_args: {})

  with pytest.raises(RuntimeError, match=r"PyPI \(Registry returned HTTP 503.*npm-primary \(version is absent\)"):
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
  (tmp_path / "candidate").mkdir()
  candidate_metadata(tmp_path / "candidate")
  monkeypatch.setattr(
    registry_verifier,
    "parse_args",
    lambda: SimpleNamespace(
      command="classify-npm",
      candidate=tmp_path / "candidate",
      version=VERSION,
      commit="tagged-sha",
      package=NPM_PACKAGE,
      github_output=output,
      github_summary=summary,
    ),
  )
  monkeypatch.setattr(registry_verifier, "classify_npm_candidate", lambda *_args: "exact")

  registry_verifier.main()

  assert (
    output.read_text(encoding="utf-8") == f"prior=value\nstate=exact\ntarball={tmp_path}/candidate/npm/package.tgz\n"
  )
  assert summary.read_text(encoding="utf-8") == "## Candidate\n- npm-primary (@0xf3cd/celestial): `exact`\n"


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
      package=None,
      github_output=None,
      github_summary=None,
    ),
  )
  monkeypatch.setattr(registry_verifier, "wait_for_candidate_registries", lambda *args: calls.append(args))

  registry_verifier.main()

  assert calls == [((tmp_path / "candidate").resolve(), VERSION, "tagged-sha")]


@pytest.fixture
def pair_candidate(tmp_path):
  assets, sources, notes = write_candidate_inputs(tmp_path)
  candidate = tmp_path / "candidate"
  stage_release_candidate(assets, sources, candidate, f"v{CURRENT_VERSION}", "tagged-sha", notes)
  return candidate


@pytest.mark.parametrize("package_name", [NPM_PACKAGE, ALIAS_NAME])
@pytest.mark.parametrize("state", ["exact", "absent", "bootstrap_required"])
def test_each_npm_identity_is_classified_from_fresh_metadata_and_bytes(pair_candidate, package_name, state):
  tarball = npm_candidate_tarballs(pair_candidate, CURRENT_VERSION)[package_name]
  version_url = package_metadata_url(package_name, CURRENT_VERSION)
  package_url = package_metadata_url(package_name)
  file_url = f"https://registry.npmjs.org/{tarball.name}"
  session = Session(
    {
      version_url: Response(
        payload=npm_metadata(tarball.read_bytes(), name=package_name, version=CURRENT_VERSION, url=file_url)
      )
      if state == "exact"
      else Response(status=404),
      package_url: Response(status=404) if state == "bootstrap_required" else Response(payload={"name": package_name}),
      file_url: Response(content=tarball.read_bytes()),
    }
  )
  assert (
    registry_validation.classify_npm_candidate(pair_candidate, CURRENT_VERSION, "tagged-sha", package_name, session)
    == state
  )
  assert [url for url, _ in session.calls] == [version_url, file_url if state == "exact" else package_url]


@pytest.mark.parametrize("package_name", [NPM_PACKAGE, ALIAS_NAME])
@pytest.mark.parametrize("endpoint", ["version", "package", "file"])
@pytest.mark.parametrize("failure", ["429", "503", "connection", "bad-json", "wrong-identity"])
def test_npm_classifier_never_treats_uncertainty_as_absence_or_exact(pair_candidate, package_name, endpoint, failure):
  tarball = npm_candidate_tarballs(pair_candidate, CURRENT_VERSION)[package_name]
  version_url = package_metadata_url(package_name, CURRENT_VERSION)
  package_url = package_metadata_url(package_name)
  file_url = f"https://registry.npmjs.org/{tarball.name}"
  routes = {
    version_url: Response(status=404)
    if endpoint == "package"
    else Response(
      payload=npm_metadata(
        tarball.read_bytes(),
        name=package_name,
        version=CURRENT_VERSION,
        url=file_url,
      )
    ),
    package_url: Response(payload={"name": package_name}),
    file_url: Response(content=tarball.read_bytes()),
  }
  url = {"version": version_url, "package": package_url, "file": file_url}[endpoint]
  if failure in ("429", "503"):
    routes[url] = Response(status=int(failure))
  elif failure == "connection":
    routes[url] = requests.ConnectionError("test transport failure")
  elif endpoint == "file":
    routes[url] = Response(content=b"wrong bytes")
  elif failure == "bad-json":
    routes[url] = Response(payload=ValueError("invalid json"))
  else:
    routes[url] = Response(payload={"name": "wrong", "version": CURRENT_VERSION})
  session = Session(routes)
  with pytest.raises((RuntimeError, requests.HTTPError)):
    registry_validation.classify_npm_candidate(pair_candidate, CURRENT_VERSION, "tagged-sha", package_name, session)
  assert [called for called, _ in session.calls].count(url) == 1


def test_npm_retry_reclassifies_both_and_retains_primary_success(pair_candidate):
  tarballs = npm_candidate_tarballs(pair_candidate, CURRENT_VERSION)
  routes = {}
  for name in tarballs:
    routes[package_metadata_url(name, CURRENT_VERSION)] = Response(status=404)
    routes[package_metadata_url(name)] = Response(payload={"name": name})
  session = Session(routes)

  def classify(name):
    return registry_validation.classify_npm_candidate(pair_candidate, CURRENT_VERSION, "tagged-sha", name, session)

  assert [classify(name) for name in tarballs] == ["absent", "absent"]
  primary_url = f"https://registry.npmjs.org/{tarballs[NPM_PACKAGE].name}"
  session.routes[package_metadata_url(NPM_PACKAGE, CURRENT_VERSION)] = Response(
    payload=npm_metadata(
      tarballs[NPM_PACKAGE].read_bytes(),
      version=CURRENT_VERSION,
      url=primary_url,
    )
  )
  session.routes[primary_url] = Response(content=tarballs[NPM_PACKAGE].read_bytes())
  session.routes[package_metadata_url(ALIAS_NAME)] = Response(status=404)
  assert [classify(name) for name in tarballs] == ["exact", "bootstrap_required"]
  alias_url = f"https://registry.npmjs.org/{tarballs[ALIAS_NAME].name}"
  session.routes[package_metadata_url(ALIAS_NAME, CURRENT_VERSION)] = Response(
    payload=npm_metadata(
      tarballs[ALIAS_NAME].read_bytes(),
      name=ALIAS_NAME,
      version=CURRENT_VERSION,
      url=alias_url,
    )
  )
  session.routes[alias_url] = Response(content=tarballs[ALIAS_NAME].read_bytes())
  assert [classify(name) for name in tarballs] == ["exact", "exact"]
  session.routes[alias_url] = Response(content=b"conflicting alias")
  with pytest.raises(RuntimeError, match="SHA-256"):
    classify(ALIAS_NAME)


def test_pair_polling_retains_individual_success_and_names_partial_completion(pair_candidate, monkeypatch):
  calls = {"PyPI": 0, NPM_PACKAGE: 0, ALIAS_NAME: 0}

  def pypi(*_args):
    calls["PyPI"] += 1
    return True

  def npm(_tarball, _version, name, _session):
    calls[name] += 1
    return name == NPM_PACKAGE

  monkeypatch.setattr(registry_validation, "pypi_version_is_exact", pypi)
  monkeypatch.setattr(registry_validation, "npm_version_is_exact", npm)
  sleeps = []
  with pytest.raises(RuntimeError, match=r"after 3 attempts: npm-alias \(version is absent\)$"):
    wait_for_candidate_registries(pair_candidate, CURRENT_VERSION, "tagged-sha", attempts=3, sleep=sleeps.append)
  assert calls == {"PyPI": 1, NPM_PACKAGE: 1, ALIAS_NAME: 3}
  assert sleeps == [10, 10]


@pytest.mark.parametrize("package_name", [NPM_PACKAGE, ALIAS_NAME])
@pytest.mark.parametrize("failure", ["429", "connection", "file-404", "metadata-503", "invalid-json"])
def test_pair_polling_retries_only_transient_states(pair_candidate, monkeypatch, package_name, failure):
  tarballs = npm_candidate_tarballs(pair_candidate, CURRENT_VERSION)
  routes = {}
  for name, tarball in tarballs.items():
    url = f"https://registry.npmjs.org/{tarball.name}"
    routes[package_metadata_url(name, CURRENT_VERSION)] = Response(
      payload=npm_metadata(
        tarball.read_bytes(),
        name=name,
        version=CURRENT_VERSION,
        url=url,
      )
    )
    routes[url] = Response(content=tarball.read_bytes())
  url = package_metadata_url(package_name, CURRENT_VERSION)
  if failure == "file-404":
    url = f"https://registry.npmjs.org/{tarballs[package_name].name}"
    failed = Response(status=404)
  elif failure == "connection":
    failed = requests.ConnectionError("interrupted")
  elif failure == "invalid-json":
    failed = Response(payload=ValueError("invalid JSON"))
  else:
    failed = Response(status=429 if failure == "429" else 503)
  routes[url] = [failed, routes[url]]
  session = Session(routes)
  sleeps = []
  monkeypatch.setattr(registry_validation, "pypi_version_is_exact", lambda *_args: True)
  if failure in ("429", "invalid-json"):
    with pytest.raises(RuntimeError, match=registry_validation.NPM_LABELS[package_name]):
      wait_for_candidate_registries(
        pair_candidate, CURRENT_VERSION, "tagged-sha", session=session, attempts=2, sleep=sleeps.append
      )
    assert sleeps == []
  else:
    wait_for_candidate_registries(
      pair_candidate, CURRENT_VERSION, "tagged-sha", session=session, attempts=2, sleep=sleeps.append
    )
    assert sleeps == [10]
    other = ALIAS_NAME if package_name == NPM_PACKAGE else NPM_PACKAGE
    assert [call for call, _ in session.calls].count(package_metadata_url(other, CURRENT_VERSION)) == 1
