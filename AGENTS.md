# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
cartesian.h                  Declares Cartesian, Units, the SRID macros, ellipsoid_t / datum_t, the constants, and the self-contained CartesianError. Header. From Xapiand (exception decoupled).
cartesian.cc                 The datum/ellipsoid tables and every conversion: forward, Helmert -> WGS84, inverse (Lin & Wang), vector algebra. From Xapiand (THROW + strings::format decoupled).
test/test.cc                 Runnable smoke test: forward conversion vs. computed values, round-trip, vector algebra, SRID support, error paths.
examples/demo.cc             A runnable tour (not a test).
CMakeLists.txt               Library `cartesian` (+ alias cartesian::cartesian); NO dependencies; CTest test `cartesian`.
LICENSE                      MIT, Copyright (c) 2015-2019 Dubalu LLC.
README.md                    What it is, install, usage, API.
ARCHITECTURE.md              Internal design of the conversions, tables, and the decoupling delta.
```

This is not header-only. `cartesian.cc` must be compiled and linked; the header
only declares the type and the conversions. The CMake target is a library with
**no third-party dependencies** — there is no FetchContent.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

No network is needed at any step. Expected output ends with
`all cartesian tests passed`, exit 0. The test target is `cartesian_test`; the
registered CTest name is `cartesian`. The test and demo are only added when this
repo is the top-level project (CMakeLists.txt), so consumers vendoring it via
`FetchContent` / `add_subdirectory` won't build them.

## Dependencies

None beyond the standard library and system headers. The extraction severed both
of the original Xapiand couplings (the located-exception `THROW` / `CartesianError`
and `strings::format`), so this builds against `std` alone. `std::format`
(C++20) is used in `to_string()` and the error messages; don't reintroduce a
formatting dependency to replace it.

## Conventions

- **C++20.** The target requests `cxx_std_20` `PUBLIC` (needed for `std::format`,
  and to stay uniform with the sibling libraries). Don't drop the target below it.
- **Filenames are stable.** The header and source keep their original Xapiand
  names (`cartesian.h`, `cartesian.cc`) so a consumer that already
  `#include`s the header just needs this repo on the include path. Don't rename
  them.
- Tabs for indentation, double quotes in code, no em dashes in prose.
- MIT-licensed; keep the copyright header (Copyright (c) 2015-2019 Dubalu LLC) on
  source files.

## Load-bearing invariants

- **`CartesianError` is `std::runtime_error`-based.** It is defined inline at the
  top of `cartesian.h` with the same name and message-string constructors as the
  Xapiand original. Xapiand catches this exact type. Don't reintroduce a
  `GeoSpatialError` / `ClientError` / located-exception base. The de-vendor
  boundary in Xapiand is responsible for remapping it back to an HTTP 400
  (`ClientError`); that remap is the consumer's job, not this library's.
- **Everything is normalized to WGS84 on construction.** A non-WGS84 SRID is
  Helmert-transformed (`toWGS84`) the moment a `Cartesian` is built, and `SRID`
  is set to `WGS84`. The rest of the API assumes `x, y, z` are WGS84 geocentric.
  Don't move the transform out of the constructors or the inverse conversions
  will use the wrong ellipsoid.
- **The datum and ellipsoid tables are the geodesy constants.** `ellipsoids[12]`
  and `map_datums` carry hand-entered axis lengths, eccentricities, and Helmert
  parameters sourced from NGA/EPSG references (citations are in the comments).
  Don't "tidy" the numbers; they are precise on purpose. An SRID missing from
  `map_datums` is the error path (`.at()` throws `std::out_of_range`, caught and
  rethrown as `CartesianError`).
- **Latitude range is ±π/2.** `toCartesian` throws `CartesianError("Latitude
  out-of-range")` outside `[-PI_HALF, PI_HALF]`. Keep the check; the test pins it.
- **The inverse is iterative (Lin & Wang 1995).** `toGeodetic` / `toLatLon`
  Newton-iterate up to ten times, stopping under `DBL_TOLERANCE`. They multiply
  by `scale` first so a normalized point still inverts. Don't change the
  iteration cap or tolerance without re-checking the round-trip test.

## How to extend

- **Add a new SRID.** Add its ellipsoid to `ellipsoids[]` (if new) and a
  `{ SRID, { ... } }` entry to `map_datums` with the seven Helmert parameters,
  with a comment citing the source. Add the `#define` to `cartesian.h`. Extend
  the test's `is_SRID_supported` case and, ideally, a round-trip assertion.
- **Always extend the smoke test.** `test/test.cc` is the only executable check.
  For a new conversion or SRID, assert against an independently computed value
  (the existing forward cases were computed from the prime-vertical formula), not
  against the library's own output.

## Traps

- **`std::format` placeholders are load-bearing.** The error messages and
  `to_string()` use `{}` / `{:.6}`. These came straight from the original
  `THROW` / `strings::format` format strings; keep the same placeholders and
  message text so existing log/scrape expectations and the test's `e.what()`
  substring checks keep matching.
- **`operator^=` and `operator*(double)` match the upstream quirks.** The
  in-place cross-assign `^=` and the member `operator*(double)` are copied
  verbatim from Xapiand (including an apparent typo in `^=` writing `x` twice).
  If you "fix" them, you diverge from upstream; treat any change as a deliberate
  fork, not a cleanup, and reconcile it upstream.
- **Geocentric values are large.** `x, y, z` are meters in the millions. Compare
  them with an absolute tolerance (the test uses 1e-3, a millimeter), not
  equality.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). The decoupling delta is small and
mechanical: `CartesianError` reparented from `GeoSpatialError`/`ClientError` to
`std::runtime_error`; `THROW(CartesianError, "fmt", a)` rewritten as
`throw CartesianError(std::format("fmt", a))`; `strings::format` swapped for
`std::format`; `#include "exception.h"` and `#include "strings.hh"` dropped; and
`<cstdint>` added so the header is self-contained for `uint8_t`. Everything else
is the original source. Any change here should be reconcilable with upstream as a
plain edit.
