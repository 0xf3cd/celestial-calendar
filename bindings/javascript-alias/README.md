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

An ESM alias for [@0xf3cd/celestial](https://www.npmjs.com/package/@0xf3cd/celestial).
It forwards every root and `/date` export, including TypeScript declarations, and depends on the exact
same primary version. It contains no copied implementation or WebAssembly module.

```sh
npm install celestial-calendar
```

```js
import * as celestial from "celestial-calendar";
import { dateToCivilUtc } from "celestial-calendar/date";

await celestial.init();
console.log(celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN));
console.log(dateToCivilUtc(new Date()));
```

Node 22 or newer is supported. `/date` does not load WASM or require initialization.
When both names resolve to the same primary installation, they share exported references and initialized
state. This does not promise deduplication across mixed versions or arbitrary dependency graphs.
See the [primary documentation](https://github.com/0xf3cd/celestial-calendar/tree/main/bindings/javascript)
for API contracts and browser usage. The PyPI package with this name is a separate Python distribution.

## License

This alias's project-authored material is licensed under MIT; see `LICENSE`.
For retained third-party material in the dependency, see `@0xf3cd/celestial/THIRD_PARTY_NOTICES.txt`
in the installed primary package. That inventory belongs to the dependency and is not bundled in this alias.
