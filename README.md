# cartesian

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/cartesian/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/cartesian/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Geodetic coordinates for C++20: latitude, longitude and height to Earth-centered WGS84
(x, y, z) from 17 datums, and back. A point given on another datum goes through a
7-parameter Helmert transform to WGS84, then onto the ellipsoid as a vector in meters from
the Earth's center. Reach for it for geometry on points of the Earth's surface: angles
between places, datum conversion and vectors for spatial indexes.

## ✨ Highlights

- **Both directions:** construct from latitude, longitude and height in degrees or
  radians; get back `toGeodetic()`, `toLatLon()` or a degrees-minutes-seconds string.
- **17 reference systems:** WGS84, WGS72, NAD83, NAD27, OSGB36, ED50, Tokyo and ten more,
  by EPSG SRID.
- **Vector algebra:** dot (`*`) and cross (`^`) products, scaling, `norm()`,
  `normalize()`, and `distance()`, the angle between two unit vectors.
- **Hashable:** `std::hash<Cartesian>` agrees with `operator==`, so points work as
  `std::unordered_set` keys.
- **Errors as exceptions:** an unsupported SRID, an out-of-range latitude or a zero-norm
  `normalize()` throws `CartesianError`.

## 🚀 Quick start

```cpp
#include "cartesian.h"

#include <cstdio>

int main() {
	Cartesian sf(37.7749, -122.4194, 0.0, Cartesian::Units::DEGREES);  // San Francisco
	Cartesian ny(40.7128, -74.0060, 0.0, Cartesian::Units::DEGREES);   // New York
	sf.normalize();
	ny.normalize();
	std::printf("%.4f rad\n", sf.distance(ny));  // 0.6499 rad

	Cartesian london(51.5, -0.12, 0.0, Cartesian::Units::DEGREES, OSGB36);  // stored on WGS84
	auto [lat, lon] = london.toLatLon();
	std::printf("%.4f, %.4f\n", lat, lon);         // 51.5005, -0.1216
	std::printf("%s\n", london.toDegMinSec().c_str());
	// 51°30'1.840685''N  0°7'17.784688''W  46.104072
}
```

[`examples/demo.cc`](examples/demo.cc) round-trips Quito with its elevation and measures
San Francisco to New York.

## 📦 Installation

Requires C++20 and CMake 3.14 or newer; no other dependencies. CI builds and tests it on
Linux (x86_64 with GCC and Clang, arm64), macOS (arm64, x86_64) and FreeBSD, plus ASan,
UBSan and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(cartesian
  GIT_REPOSITORY https://github.com/Kronuz/cartesian.git
  GIT_TAG        a3c8a8c8e6e94e1a6c2259d1443ce75b082c707d)  # or main, to track the tip
FetchContent_MakeAvailable(cartesian)

target_link_libraries(my_app PRIVATE cartesian::cartesian)
```

`cartesian::cartesian` is a small compiled library (`cartesian.cc`). Include
`cartesian.h`.

## 🎯 When to use it

- Geometry on Earth coordinates: angles between places, datum conversion, vectors for
  spatial indexes such as [htm](https://github.com/Kronuz/htm).

Not for:

- Map projections (UTM, Web Mercator): it converts to and from Earth-centered
  coordinates only.
- Distances along the ellipsoid: `distance()` is the angle between two vectors, so
  normalize both points first; multiply by a radius for a spherical estimate.
- Points with a NaN coordinate in a set: such a point equals nothing, itself included,
  so a lookup never finds it.

## 📚 Documentation

- [docs/API.md](docs/API.md): constructors, SRIDs, conversions, operators, errors and
  caveats.
- [ARCHITECTURE.md](ARCHITECTURE.md): the ellipsoid math and the Helmert transform.
- [CHANGELOG.md](CHANGELOG.md): notable changes, with their measurements.
- [AGENTS.md](AGENTS.md): repository map, invariants and traps.
- [Catalog entry](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#cartesian)
  in Kronuz C++ Libraries.

## 🛠️ Building and testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`./build/cartesian_demo` runs the tour and `./build/cartesian_bench` times
`std::hash<Cartesian>`.

## 🤝 Contributing

Issues and pull requests are welcome; read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first.

## 📄 License

MIT, see [LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand),
where it backs geospatial search.
