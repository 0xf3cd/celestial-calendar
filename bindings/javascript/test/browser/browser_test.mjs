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
import { spawn, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { createReadStream, existsSync } from "node:fs";
import { appendFile, cp, mkdir, readFile, readdir, rm, stat, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import puppeteer, { PUPPETEER_REVISIONS } from "puppeteer-core";
import { computeExecutablePath, detectBrowserPlatform, getDownloadUrl, install } from "@puppeteer/browsers";

if (process.argv.length !== 4) throw new Error("usage: node browser_test.mjs <primary.tgz> <alias.tgz>");

const HOST = "127.0.0.1";
const PORT = 4321;
const ORIGIN = `http://${HOST}:${PORT}`;
const HERE = dirname(fileURLToPath(import.meta.url));
const PACKAGE_ROOT = resolve(HERE, "../..");
const FIXTURE = resolve(HERE, "fixture");
const WORK = resolve(PACKAGE_ROOT, "build/browser-consumer");
const TARBALL = resolve(process.argv[2]);
const ALIAS_TARBALL = resolve(process.argv[3]);
const ASTRO = resolve(PACKAGE_ROOT, "node_modules/astro/bin/astro.mjs");
const VITE = resolve(PACKAGE_ROOT, "node_modules/vite/bin/vite.js");
const NPM_CACHE = resolve(PACKAGE_ROOT, "build/npm-cache");
const BROWSER_CACHE = resolve(PACKAGE_ROOT, "build/browsers");
const BROWSER_RECEIPTS = resolve(PACKAGE_ROOT, "build/browser-downloads.jsonl");

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
    dependencies: { "@0xf3cd/celestial": `file:${TARBALL}`, "celestial-calendar": `file:${ALIAS_TARBALL}` },
  }, null, 2),
);
run("npm", ["install", "--offline", "--ignore-scripts", "--no-audit", "--no-fund", "--package-lock=false"], WORK);
run(process.execPath, [ASTRO, "build"], WORK);

const golden = JSON.parse(await readFile(new URL("../../../../toolbox/bindings_golden.json", import.meta.url), "utf8"));
const moon = golden.sections.moon.entries[0];
const bitsOf = (hex) => Buffer.from(hex.slice(2), "hex").readDoubleBE(0);
const assets = await readdir(resolve(WORK, "dist/assets"));
const wasmAssets = assets.filter((name) => name.endsWith(".wasm"));
assert.equal(wasmAssets.length, 1, `expected one WASM asset: ${JSON.stringify(wasmAssets)}`);
const wasmBytes = (await stat(resolve(WORK, "dist/assets", wasmAssets[0]))).size;
const installedManifest = JSON.parse(
  await readFile(resolve(WORK, "node_modules/@0xf3cd/celestial/package.json"), "utf8"),
);

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
  const tested = [];
  for (const name of ["chrome", "firefox"]) {
    const buildId = PUPPETEER_REVISIONS[name];
    const platform = detectBrowserPlatform();
    const options = { browser: name, buildId, platform, cacheDir: BROWSER_CACHE };
    if (!existsSync(computeExecutablePath(options))) {
      const archive = await install({ ...options, unpack: false });
      try {
        const hash = createHash("sha256");
        for await (const chunk of createReadStream(archive)) hash.update(chunk);
        await appendFile(BROWSER_RECEIPTS, `${JSON.stringify({
          browser: name,
          buildId,
          platform,
          requestedUrl: getDownloadUrl(name, platform, buildId).href,
          sha256: hash.digest("hex"),
          recordedAt: new Date().toISOString(),
        })}\n`, "utf8");
      } catch (error) {
        console.warn(`Could not record ${name} archive receipt (${error.code ?? error.name}).`);
      }
    }
    const installed = await install(options);
    browser = await puppeteer.launch({
      browser: name,
      executablePath: installed.executablePath,
      headless: true,
      args: name === "chrome" ? ["--no-sandbox", "--disable-dev-shm-usage"] : [],
    });
    const version = await browser.version();
    assert(version.endsWith(`/${buildId.replace(/^stable_/, "")}`), `${name}: unexpected browser ${version}`);
    const page = await browser.newPage();
    const requests = [];
    const responses = [];
    page.on("request", (request) => requests.push(request.url()));
    page.on("response", (response) => responses.push({ url: response.url(), status: response.status() }));

    await page.goto(ORIGIN, { waitUntil: "networkidle0" });
    await page.waitForFunction(() => window.__CELESTIAL_IMPORTED__ === true, { timeout: 60_000 });
    const dateResult = await page.$eval("#date-result", (output) => JSON.parse(output.textContent));
    assert.deepEqual(dateResult, {
      civil: { year: 2024, month: 2, day: 4, fraction: 0, hour: 0, minute: 0, second: 0 },
      iso: "2024-02-03T16:00:00.000Z",
    });
    assert.equal(
      requests.filter((url) => new URL(url).pathname.endsWith(".wasm")).length,
      0,
      "import or date conversion fetched WASM",
    );

    await page.evaluate((jdeTt) => window.__START_CELESTIAL__(jdeTt), bitsOf(moon.jde_bits));
    await page.waitForFunction(() => window.__CELESTIAL_RESULT__ !== undefined, { timeout: 60_000 });
    const result = await page.evaluate(() => window.__CELESTIAL_RESULT__);
    assert(!result.fatal, result.fatal);
    assert.equal(result.translated, true, "recording failure was not translated");
    assert.equal(result.survived, true, "module did not survive a translated failure");
    assert.equal(result.sharedExports, true, "alias exports differ from the resolved primary");
    assert.deepEqual(Object.keys(result.lichun).sort(), ["jieqi", "momentUt1"]);
    assert.equal(result.lichunMatches, true);
    assert.equal(result.lichun.momentUt1.year, 2024);
    assert(Number.isInteger(result.lichun.momentUt1.hour));
    assert(Number.isInteger(result.lichun.momentUt1.minute));
    assert(Number.isFinite(result.lichun.momentUt1.second));
    assert(Number.isFinite(result.jdUt1));
    // Same native-output reference and WASM/libm lunar-value cap as the raw protocol replay.
    assert(Math.abs(result.illumination - bitsOf(moon.illumination_bits)) <= 1e-9, `${name}: reference illumination`);

    const wasmRequests = requests.filter((url) => new URL(url).pathname.endsWith(".wasm"));
    assert.equal(wasmRequests.length, 1, `expected one WASM request: ${JSON.stringify(wasmRequests)}`);
    assert.equal(new URL(wasmRequests[0]).origin, ORIGIN, "WASM did not load from the package build");
    assert.equal(responses.find(({ url }) => url === wasmRequests[0])?.status, 200, "WASM request failed");
    assert.equal(
      requests.some((url) => /github\.com|githubusercontent\.com|\/releases\/download\//i.test(url)),
      false,
      `release fallback observed: ${JSON.stringify(requests)}`,
    );

    console.log(JSON.stringify({
      browser: name,
      version,
      package: `${installedManifest.name}@${installedManifest.version}`,
      wasmUrl: wasmRequests[0],
      wasmAsset: `dist/assets/${wasmAssets[0]}`,
      wasmBytes,
      illumination: result.illumination,
      importedWithoutFetch: true,
      dateConvertedWithoutFetch: true,
      exceptionTranslated: true,
      moduleSurvived: true,
    }));
    tested.push(name);
    await browser.close();
    browser = undefined;
  }
  assert.deepEqual(tested, ["chrome", "firefox"], "both pinned browser consumers completed");
} finally {
  if (browser) await browser.close();
  server.kill("SIGTERM");
  await new Promise((resolvePromise) => {
    if (server.exitCode !== null) resolvePromise();
    else server.once("exit", resolvePromise);
  });
}
