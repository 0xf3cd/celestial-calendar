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
import { spawnSync } from "node:child_process";
import { readFile, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

// Other consumers check C/C++ and Python; shell fences are instructions and text is expected output.
export const nonPrograms = {
  sh: "shell installation/build instructions",
  powershell: "PowerShell build instructions",
  c: "C quickstart, compiled separately",
  cpp: "C++ quickstart, compiled by source automation",
  python: "Python program, executed by the wheel consumer",
  text: "displayed expected output",
};

export function readFences(text) {
  const blocks = [];
  let opening;
  let body = [];
  let comment = false;
  for (const line of text.replaceAll("\r\n", "\n").match(/[^\n]*\n|[^\n]+$/g) ?? []) {
    if (opening !== undefined) {
      const [marker, info, indent] = opening;
      const closing = new RegExp(`^ {0,3}${marker[0]}{${marker.length},}[ \\t]*\\n?$`);
      if (closing.test(line)) {
        blocks.push([info, body.join("")]);
        opening = undefined;
        body = [];
      } else {
        body.push(line.slice(Math.min(indent, line.match(/^ */)[0].length)));
      }
      continue;
    }
    if (comment) {
      comment = !line.includes("-->");
      continue;
    }
    if (line.trimStart().startsWith("<!--")) {
      comment = !line.includes("-->");
      continue;
    }
    const match = /^( {0,3})(`{3,}|~{3,})([^\n]*)\n?$/.exec(line);
    if (match !== null) {
      const [, indent, marker, info] = match;
      assert(!(marker[0] === "`" && info.includes("`")), "unsupported fence info");
      opening = [marker, info.trim(), indent.length];
    }
  }
  assert.equal(opening, undefined, "unclosed documentation fence");
  return blocks;
}

export function programs(text) {
  const blocks = readFences(text);
  const unknown = blocks.map(([info]) => info).filter((info) => info !== "js" && !Object.hasOwn(nonPrograms, info));
  assert.deepEqual(unknown, [], "unclassified documentation fences");
  const selected = blocks.filter(([info]) => info === "js").map(([, code]) => code);
  assert(selected.length > 0 && selected.every((code) => code.trim()), "missing JavaScript documentation programs");
  return selected;
}

export function expectedOutput(text) {
  const outputs = readFences(text).filter(([info]) => info === "text").map(([, code]) => code);
  assert(outputs.length === 1 && outputs[0].trim(), "expected exactly one displayed output");
  return outputs[0];
}

export async function runPrograms(text, consumer, packageName, expected) {
  const selected = programs(text);
  let executed = 0;
  for (const code of selected) {
    // Only substitute the established package identity, never imports or example state.
    await writeFile(join(consumer, "doc-example.mjs"), code.replaceAll("@0xf3cd/celestial", packageName));
    const result = spawnSync(process.execPath, ["doc-example.mjs"], { cwd: consumer, encoding: "utf8" });
    assert.equal(result.status, 0, `documentation example failed: ${result.stdout}\n${result.stderr}`);
    if (expected !== undefined) assert.equal(result.stdout.replaceAll("\r\n", "\n"), expected, "displayed output mismatch");
    executed += 1;
  }
  assert.equal(executed, selected.length);
  return executed;
}

export async function runDocExamples(consumer, packageName) {
  const root = await readFile(new URL("../../../../README.md", import.meta.url), "utf8");
  const count = await runPrograms(root, consumer, packageName, expectedOutput(root));
  console.log(`PASS displayed root JavaScript programs ${count}/${count} via ${packageName}`);
  for (const name of packageName === "celestial-calendar" ? ["@0xf3cd/celestial", "celestial-calendar"] : [packageName]) {
    const directory = name === "celestial-calendar" ? "javascript-alias" : "javascript";
    const source = await readFile(new URL(`../../../${directory}/README.md`, import.meta.url), "utf8");
    const installed = await readFile(join(consumer, "node_modules", name, "README.md"), "utf8");
    assert.equal(installed.replaceAll("\r\n", "\n"), source.replaceAll("\r\n", "\n"), "installed README differs from candidate");
    const ran = await runPrograms(installed, consumer, name === "@0xf3cd/celestial" ? packageName : name);
    console.log(`PASS displayed ${name} JavaScript programs ${ran}/${ran} and installed guide bytes`);
  }
}

// Core invokes this pure-reader path without installing a package or loading WASM.
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  assert.equal(process.argv[2], "--fences-json");
  assert.equal(process.argv.length, 4);
  console.log(JSON.stringify(readFences(await readFile(process.argv[3], "utf8"))));
}
