<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions between
    Gregorian and Chinese Lunar calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# celestial-calendar

ESM alias package for [@0xf3cd/celestial](https://www.npmjs.com/package/@0xf3cd/celestial). It re-exports
everything from the root entry and `/date`, including the TypeScript declarations, and depends on exactly the same
version of the primary package. The alias copies no implementation and contains no WebAssembly module.

[中文说明（@0xf3cd/celestial）](https://github.com/0xf3cd/celestial-calendar/blob/main/bindings/javascript/README_ZH.md)

This guide matches the current `0.7.0` source and does not mean that this version or the alias has been published to
npm. Until then, [build the primary and alias packages](https://github.com/0xf3cd/celestial-calendar/blob/main/docs/BUILDING.md#wasm)
from the full source and install both tarballs of the same version.

## Install and Examples

Once the version you need is on npm, install it in your application directory:

```sh
npm install celestial-calendar
```

Node 22 or newer is supported. Each JavaScript block below is a standalone ES module for `example.mjs` in your
application directory; run it with `node example.mjs` from that directory.

### Today's Lunar Date

Take the Gregorian date at fixed UTC+8 first, then convert it to a lunar date; do not pass a civil moment that
carries `fraction` directly.

```js
import * as celestial from "celestial-calendar";
import { dateToCivilAtOffset } from "celestial-calendar/date";

const eastEight = dateToCivilAtOffset(new Date(), 480);
const { year, month, day } = eastEight;
const gregorian = { year, month, day };

await celestial.init();
console.log("UTC+8 Gregorian date:", gregorian);
console.log("Lunar date:", celestial.lunar.fromGregorian("algo3", gregorian));
```

### Jieqi and UTC Date Fields

The example below queries the UT1 moment of Lichun and, separately, the UTC civil fields of the current clock; no
time-scale conversion connects the two outputs. The Jieqi `momentUt1` is neither UTC nor a UTC+8 wall clock.

```js
import * as celestial from "celestial-calendar";
import { dateToCivilUtc } from "celestial-calendar/date";

await celestial.init();
console.log(celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN));
console.log(dateToCivilUtc(new Date()));
```

## Relationship to the Primary Package

`/date` does not load WASM and needs no initialization; it only bridges civil fields in UTC or at a fixed offset,
with no UTC/UT1/TT conversion. When both package names resolve to the same primary installation, they share export
references and initialization state; this does not guarantee deduplication with mixed versions or arbitrary
dependency graphs. For the API contract, the date basis of the lunar algorithms, and browser use, see the
[primary package guide](https://github.com/0xf3cd/celestial-calendar/tree/main/bindings/javascript).
The package of the same name on PyPI is a separate Python distribution.

## License

This alias's project-authored material is licensed under MIT; see `LICENSE`.
For retained third-party material in the dependency, see `@0xf3cd/celestial/THIRD_PARTY_NOTICES.txt`
in the installed primary package. That inventory belongs to the dependency and is not bundled in this alias.
