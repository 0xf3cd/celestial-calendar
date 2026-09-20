/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { lstat, readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

import { runPackageConsumer } from "../support/package_consumer.mjs";

if (process.argv.length !== 4) {
  throw new Error("usage: node artifact_consumer_test.mjs <artifact-directory> <npm-cli.js>");
}

const artifact = resolve(process.argv[2]);
const dependencies = {};
let expectedVersion;
for (const [name, stem] of [["@0xf3cd/celestial", "npm-pack"], ["celestial-calendar", "npm-alias-pack"]]) {
  const metadata = JSON.parse(await readFile(resolve(artifact, `${stem}.json`), "utf8"));
  assert(Array.isArray(metadata) && metadata.length === 1, `${stem}: expected one package`);
  const pack = metadata[0];
  assert(pack && typeof pack === "object" && !Array.isArray(pack), `${stem}: expected a package object`);
  assert.equal(pack.name, name, `${stem}: package name`);
  assert.equal(typeof pack.version, "string", `${stem}: package version`);
  assert.equal(pack.version.match(/^\d+\.\d+\.\d+$/)?.[0], pack.version, `${stem}: package version`);
  expectedVersion ??= pack.version;
  assert.equal(pack.version, expectedVersion, "npm pair version drift");
  assert.equal(typeof pack.filename, "string", `${stem}: tarball basename`);
  assert.equal(
    pack.filename.match(/^[a-zA-Z0-9][a-zA-Z0-9._-]*\.tgz$/)?.[0], pack.filename,
    `${stem}: unsafe tarball basename`,
  );
  const tarball = resolve(artifact, pack.filename);
  assert((await lstat(tarball)).isFile(), `${stem}: tarball must be a regular file`);
  const digest = createHash("sha256").update(await readFile(tarball)).digest("hex");
  assert.equal(
    await readFile(resolve(artifact, `${stem}.sha256`), "utf8"),
    `${digest}  ${pack.filename}\n`,
    `${stem}: tarball SHA-256 sidecar mismatch`,
  );
  dependencies[name] = pathToFileURL(tarball).href;
}

await runPackageConsumer({
  dependencies,
  packageName: "celestial-calendar",
  expectedVersion,
  installArgs: ["--offline", "--ignore-scripts", "--no-audit", "--no-fund", "--package-lock=false"],
  prefix: "celestial artifact consumer ",
  npmCli: resolve(process.argv[3]),
  success: `PASS artifact pair ${expectedVersion}: installed API, alias identity, and /date without WASM`,
});
