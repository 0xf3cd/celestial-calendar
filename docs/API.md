# API Reference

This reference covers C++ declarations and the C interface in `celestial.h`.
Bulk coefficient data and source listings are omitted.
Browse **Namespaces**, **Classes**, or **Files**, or search for a function name.
The lunar headers retain their English and Chinese descriptions; the rest is primarily English.

Read each function's units, time scale and supported range before using it. A civil date is not
an instant, and UT1 and TT inputs are not interchangeable.

For installation, examples and Python/JavaScript package guides, see the
[repository README](https://github.com/0xf3cd/celestial-calendar#readme).
Project-authored material uses MIT. The generated directory includes `LICENSE` and
`THIRD_PARTY_NOTICES.txt`; retained material keeps the source terms stated there.

## Build This Reference

Install Doxygen 1.15.0, then run `python project.py --docs` from the source root.
Open `build/api-docs/html/index.html`. No compiler, Graphviz or package build is required.
The same command runs in **Core Tests**, which uploads the `celestial-api-html` artifact.
Documentation is opt-in, is not part of `--all`, and is not a release artifact or hosted site.

The build rejects documentation syntax errors and checks representative generated pages.
It does not require every declaration or parameter to have a description.
