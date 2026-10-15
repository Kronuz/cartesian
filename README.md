# cartesian

[![CI](https://github.com/Kronuz/cartesian/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/cartesian/actions/workflows/ci.yml)

A small C++20 library for converting geodetic coordinates (latitude, longitude,
ellipsoid height on some SRID) to and from geocentric Cartesian `(x, y, z)`
vectors on the WGS84 ellipsoid, plus the bit of vector algebra you need on those
points. Extracted from [Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

One type, `Cartesian`, plus the conversions around it. You hand it a latitude,
longitude, and height in some supported SRID (a coordinate reference system) and
it gives you the geocentric `(x, y, z)` for that point in meters from the
Earth's center, converting the datum to WGS84 along the way via a 7-parameter
Helmert transform. The inverse (`toGeodetic`, `toLatLon`) takes a geocentric
point back to latitude / longitude / height, and `toDegMinSec` pretty-prints it.
On top of that, `Cartesian` is a 3-vector: dot product, cross product, addition,
subtraction, `norm`, `normalize`, `inverse`, and an angular `distance` between
two surface points.

It is what you reach for when you need to do geometry on points on the Earth:
turn surface coordinates into vectors you can take dot and cross products of,
measure the angle between two locations, or move between datums. Seventeen SRIDs
ship with their datum and ellipsoid parameters (WGS84, NAD83/27, OSGB36, ED50,
Tokyo, and more); WGS84 is the canonical one everything converts into.

## Install

This is not header-only. It ships a compiled translation unit, `cartesian.cc`,
plus the header `cartesian.h`. You build and link the `.cc`; the header alone
only declares the type and the conversions. It requires C++20 (for
`std::format`).

It has **no third-party dependencies** — only the standard library and system
headers. There is no FetchContent in the build.

With CMake `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  cartesian
  GIT_REPOSITORY https://github.com/Kronuz/cartesian.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(cartesian)

target_link_libraries(your_target PRIVATE cartesian::cartesian)
```

`CMakeLists.txt` requests `cxx_std_20` `PUBLIC`, so C++20 propagates to anything
linking it. Then:

```cpp
#include "cartesian.h"
```

The header keeps its original filename, so a codebase that already
`#include "cartesian.h"` just needs this repo on its include path.

## Usage

```cpp
#include "cartesian.h"

// Geodetic -> geocentric. Latitude, longitude, height, units, optional SRID.
Cartesian sf(37.7749, -122.4194, 0.0, Cartesian::Units::DEGREES);  // San Francisco
// sf.x, sf.y, sf.z are meters from the Earth's center, on WGS84.

// Radians work too.
Cartesian p(0.659, -2.137, 0.0, Cartesian::Units::RADIANS);

// A non-WGS84 SRID is Helmert-transformed to WGS84 on construction.
Cartesian uk(51.5, -0.12, 0.0, Cartesian::Units::DEGREES, OSGB36);

// Geocentric -> geodetic, back to lat/lon(/height).
auto [lat, lon, height] = sf.toGeodetic();
auto [lat2, lon2]       = sf.toLatLon();
std::string dms         = sf.toDegMinSec();   // "37°46'...''N  122°25'...''W  ..."

// Vector algebra on the points.
Cartesian a(1.0, 0.0, 0.0), b(0.0, 1.0, 0.0);
double dot = a * b;          // scalar (dot) product
Cartesian cross = a ^ b;     // vector (cross) product
double n = a.norm();         // length
a.normalize();               // onto the unit sphere; remembers length in a.scale

// Angle between two surface points (radians).
double angle = a.distance(b);

// Is an SRID known?
bool ok = Cartesian::is_SRID_supported(WGS84);   // true
```

## API reference

```cpp
Cartesian();                                            // (a, 0, 0): equator / prime meridian
Cartesian(double lat, double lon, double height,
          Units units, int SRID = WGS84);              // geodetic -> geocentric (-> WGS84)
Cartesian(double x, double y, double z, int SRID = WGS84);  // raw geocentric (-> WGS84)
```

- **`Units`** — `Cartesian::Units::DEGREES` or `::RADIANS`, for the lat/lon
  constructor.
- **`SRID`** — one of the supported reference systems (macros `WGS84`, `NAD83`,
  `NAD27`, `OSGB36`, `TM75`, `TM65`, `ED79`, `ED50`, `TOYA`, `DHDN`, `OEG`,
  `AGD84`, `SAD69`, `PUL42`, `MGI1901`, `GGRS87`, `WGS72`). Non-WGS84 inputs are
  Helmert-transformed to WGS84 on construction.

Conversions and helpers:

- `std::tuple<double,double,double> toGeodetic() const` — latitude, longitude
  (degrees), height (meters).
- `std::pair<double,double> toLatLon() const` — latitude, longitude (degrees).
- `std::string toDegMinSec() const` — `"D°M'S''N  D°M'S''E  height"`.
- `Cartesian& normalize()` / `Cartesian& inverse()` / `double norm() const`.
- `double distance(const Cartesian&) const` — angular distance (radians).
- `std::string to_string() const` — `"SRID=4326; (x y z)"`.
- `static bool is_SRID_supported(int)` / `int getSRID() const`.

Operators: `==` `!=` `<` `>`, `*` (dot), `^` / `^=` (cross), `+` `+=` `-` `-=`,
and scalar `*`. `std::hash<Cartesian>` is specialized (hashes `to_string()`).

Errors: an unsupported SRID, an out-of-range latitude (outside ±90°), or a
zero-norm `normalize()` throw `CartesianError`, which derives from
`std::runtime_error` (see Provenance).

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

No network is needed at any step — there are no dependencies to fetch. The test
checks the forward conversion against independently computed WGS84 values
(equator, 40N/0E, San Francisco), degrees-vs-radians agreement, the default
constructor, the `toLatLon` / `toGeodetic` / `toDegMinSec` round-trip (including
a non-zero height), the vector algebra (dot, cross, +, -, norm, normalize,
inverse), `is_SRID_supported`, and that an unsupported SRID and an out-of-range
latitude both throw `CartesianError`. It prints `all cartesian tests passed` and
exits 0.

## Examples

[`examples/demo.cc`](examples/demo.cc) is a runnable tour. A top-level CMake
build produces it next to the test:

```sh
cmake -B build && cmake --build build && ./build/cartesian_demo
```

It converts San Francisco and Tokyo to geocentric vectors and prints their
normalized form, round-trips Quito (with elevation) back to lat/lon/height, and
measures the angular distance between San Francisco and New York.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand). `cartesian.h` /
`cartesian.cc` are the original files; the standalone delta is pure decoupling.
`CartesianError` was reparented from Xapiand's `GeoSpatialError` -> `ClientError`
hierarchy to `std::runtime_error` (same name, same message constructors, so
existing `catch (const CartesianError&)` sites keep working); the
located-exception `THROW(CartesianError, "fmt", args...)` macros became
`throw CartesianError(std::format("fmt", args...))`; and `strings::format`
became `std::format`. That dropped both local includes (`"exception.h"` and
`"strings.hh"`), leaving a library that builds against the standard library
alone. See [ARCHITECTURE.md](ARCHITECTURE.md) for the design and
[AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE).
