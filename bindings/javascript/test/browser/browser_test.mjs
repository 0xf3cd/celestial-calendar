/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import assert from "node:assert/strict";
import { spawn, spawnSync } from "node:child_process";
import { cp, mkdir, readFile, readdir, rm, stat, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import puppeteer from "puppeteer-core";

if (process.argv.length !== 3) throw new Error("usage: node browser_test.mjs <package.tgz>");

const HOST = "127.0.0.1";
const PORT = 4321;
const ORIGIN = `http://${HOST}:${PORT}`;
const HERE = dirname(fileURLToPath(import.meta.url));
const PACKAGE_ROOT = resolve(HERE, "../..");
const FIXTURE = resolve(HERE, "fixture");
const WORK = resolve(PACKAGE_ROOT, "build/browser-consumer");
const TARBALL = resolve(process.argv[2]);
const ASTRO = resolve(PACKAGE_ROOT, "node_modules/astro/bin/astro.mjs");
const VITE = resolve(PACKAGE_ROOT, "node_modules/vite/bin/vite.js");
const NPM_CACHE = resolve(PACKAGE_ROOT, "build/npm-cache");

const run = (command, args, cwd) => {
  const completed = spawnSync(command, args, {
    cwd,
    encoding: "utf8",
    env: { ...process.env, npm_config_cache: NPM_CACHE },
  });
  if (completed.status !== 0) {
    throw new Error(
      `${command} ${args.join(" ")} failed (${completed.status})\n${completed.stdout}\n${completed.stderr}`,
    );
  }
};

await rm(WORK, { recursive: true, force: true });
await mkdir(WORK, { recursive: true });
await cp(FIXTURE, WORK, { recursive: true });
await writeFile(
  resolve(WORK, "package.json"),
  JSON.stringify({
    name: "celestial-browser-consumer",
    version: "0.0.0",
    private: true,
    type: "module",
    dependencies: { "@0xf3cd/celestial": `file:${TARBALL}` },
  }, null, 2),
);
run("npm", ["install", "--offline", "--ignore-scripts", "--no-audit", "--no-fund", "--package-lock=false"], WORK);
run(process.execPath, [ASTRO, "build"], WORK);

const server = spawn(process.execPath, [VITE, "preview", "--host", HOST, "--port", String(PORT)], {
  cwd: WORK,
  stdio: ["ignore", "pipe", "pipe"],
});
const serverOutput = [];
server.stdout.on("data", (chunk) => serverOutput.push(chunk.toString()));
server.stderr.on("data", (chunk) => serverOutput.push(chunk.toString()));

const waitForServer = async () => {
  for (let attempt = 0; attempt < 100; ++attempt) {
    try {
      const response = await fetch(ORIGIN);
      if (response.ok) return;
    } catch {
      // The preview process has not bound the port yet.
    }
    await new Promise((resolvePromise) => setTimeout(resolvePromise, 100));
  }
  throw new Error(`preview did not start:\n${serverOutput.join("")}`);
};

let browser;
try {
  await waitForServer();
  browser = await puppeteer.launch({
    executablePath: process.env.CHROME_PATH || "/usr/bin/google-chrome-stable",
    headless: true,
    args: ["--no-sandbox", "--disable-dev-shm-usage"],
  });
  const page = await browser.newPage();
  const requests = [];
  const responses = [];
  page.on("request", (request) => requests.push(request.url()));
  page.on("response", (response) => responses.push({ url: response.url(), status: response.status() }));

  await page.goto(ORIGIN, { waitUntil: "networkidle0" });
  await page.waitForFunction(() => window.__CELESTIAL_IMPORTED__ === true, { timeout: 60_000 });
  assert.equal(requests.filter((url) => new URL(url).pathname.endsWith(".wasm")).length, 0, "import fetched WASM");

  await page.evaluate(() => window.__START_CELESTIAL__());
  await page.waitForFunction(() => window.__CELESTIAL_RESULT__ !== undefined, { timeout: 60_000 });
  const result = await page.evaluate(() => window.__CELESTIAL_RESULT__);
  assert(!result.fatal, result.fatal);
  assert.equal(result.translated, true, "recording failure was not translated");
  assert.equal(result.survived, true, "module did not survive a translated failure");

  const wasmRequests = requests.filter((url) => new URL(url).pathname.endsWith(".wasm"));
  assert.equal(wasmRequests.length, 1, `expected one WASM request: ${JSON.stringify(wasmRequests)}`);
  assert.equal(new URL(wasmRequests[0]).origin, ORIGIN, "WASM did not load from the package build");
  assert.equal(responses.find(({ url }) => url === wasmRequests[0])?.status, 200, "WASM request failed");
  assert.equal(
    requests.some((url) => /github\.com|githubusercontent\.com|\/releases\/download\//i.test(url)),
    false,
    `release fallback observed: ${JSON.stringify(requests)}`,
  );

  const assets = await readdir(resolve(WORK, "dist/assets"));
  const wasmAssets = assets.filter((name) => name.endsWith(".wasm"));
  assert.equal(wasmAssets.length, 1, `expected one WASM asset: ${JSON.stringify(wasmAssets)}`);
  const wasmBytes = (await stat(resolve(WORK, "dist/assets", wasmAssets[0]))).size;
  const installedManifest = JSON.parse(
    await readFile(resolve(WORK, "node_modules/@0xf3cd/celestial/package.json"), "utf8"),
  );

  console.log(JSON.stringify({
    chrome: await browser.version(),
    package: `${installedManifest.name}@${installedManifest.version}`,
    wasmUrl: wasmRequests[0],
    wasmAsset: `dist/assets/${wasmAssets[0]}`,
    wasmBytes,
    importedWithoutFetch: true,
    exceptionTranslated: true,
    moduleSurvived: true,
  }));
} finally {
  if (browser) await browser.close();
  server.kill("SIGTERM");
  await new Promise((resolvePromise) => {
    if (server.exitCode !== null) resolvePromise();
    else server.once("exit", resolvePromise);
  });
}
