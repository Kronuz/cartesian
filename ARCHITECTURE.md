# Architecture

The internals of `cartesian`: how a geodetic coordinate becomes a geocentric
vector and back, what the datum tables hold, and what changed when the library
was lifted out of Xapiand. For usage see `README.md`; for the repo map and
invariants see `AGENTS.md`.

## Shape

A header with the type and a single compiled translation unit with the tables
and the math:

```
  cartesian.h    declares Cartesian, the Units enum, the SRID macros, the
                 ellipsoid_t / datum_t structs, the constants, and the
                 self-contained CartesianError exception.
  cartesian.cc   the datum/ellipsoid tables and every conversion: forward
                 (geodetic -> geocentric), the Helmert transform to WGS84, the
                 inverse (geocentric -> geodetic), and the vector algebra.
```

`Cartesian` stores three doubles `x, y, z` (geocentric, meters), an `int SRID`,
and a `scale` factor (1 until `normalize()` records the original norm). The
constructors do the work: the lat/lon form converts to geocentric and, if the
SRID is not already WGS84, Helmert-transforms it; the raw `(x, y, z)` form just
transforms if needed.

## The datum and ellipsoid tables

Two static tables in `cartesian.cc` carry the geodesy constants:

- `ellipsoids[12]` — each entry is a biaxial ellipsoid: name, semi-major axis
  `a`, semi-minor axis `b`, and first eccentricity squared `e2`. WGS84 (the GPS
  ellipsoid) is entry 0.
- `map_datums` — a `std::unordered_map<int, datum_t>` keyed by SRID. Each datum
  names an ellipsoid plus the seven Helmert parameters (three translations in
  meters, three rotations in radians, one scale factor) needed to bring that
  datum onto WGS84. WGS84 itself maps to all-zero parameters (the identity
  transform).

Looking up an SRID that is not in `map_datums` is the error path: `.at()` throws
`std::out_of_range`, which the conversions catch and rethrow as `CartesianError`.

## Forward: geodetic to geocentric

`toCartesian` takes latitude, longitude, height, and units. Degrees are scaled
to radians by `RAD_PER_DEG`. Latitude outside ±π/2 (`PI_HALF`) throws
`CartesianError("Latitude out-of-range")`. Then the standard prime-vertical
formula runs on the datum's ellipsoid:

```cpp
double N = a / std::sqrt(1 - e2 * sin_lat * sin_lat);   // radius of curvature
x = (N + height) * cos_lat * std::cos(lon);
y = (N + height) * cos_lat * std::sin(lon);
z = ((1 - e2) * N + height) * sin_lat;
```

A point on the equator at the prime meridian (lat 0, lon 0, height 0) lands
exactly on the major axis, `(a, 0, 0)` — which is also what the default
constructor produces.

## The Helmert transform to WGS84

`toWGS84` applies the seven-parameter (Bursa-Wolf) transform using the datum's
translation, rotation, and scale parameters, then sets `SRID = WGS84`. Every
non-WGS84 input is normalized to WGS84 on construction, so once a `Cartesian`
exists, its `x, y, z` are WGS84 geocentric and the rest of the API does not have
to think about datums. WGS84 inputs skip the transform entirely (the constructor
guards on `SRID != WGS84`).

## Inverse: geocentric to geodetic

`toGeodetic` and `toLatLon` go the other way, using the iterative method of Lin
and Wang (1995). There is no closed form for latitude on an ellipsoid, so the
code seeds an estimate `m0` from the point's distance to the polar axis and its
`z`, then Newton-iterates (up to ten passes, stopping when the step falls under
`DBL_TOLERANCE`). From the converged `m` it recovers the foot-of-perpendicular
on the ellipsoid, and from that the latitude (`atan2`), longitude (a numerically
stable `2 * atan2(y, x + p)`), and signed height. `toLatLon` is the same
iteration without the height step. Both multiply through by `scale` first, so a
normalized point still inverts correctly.

`toDegMinSec` builds on `toGeodetic`, splitting the degree values into degrees,
minutes, and seconds and tagging the hemispheres (`N`/`S`, `E`/`W`).

## Vector algebra

Because the geocentric form is just a 3-vector, `Cartesian` carries the
operators you need for geometry on the sphere: `*` is the dot (scalar) product,
`^` / `^=` the cross (vector) product, `+` `-` componentwise, `norm` the
Euclidean length, `inverse` the negation, and `normalize` the projection onto
the unit sphere (storing the original length in `scale`). `distance` is the
angular distance between two surface points, `acos` of their dot product. The
comparison operators order by `y`, then `x`, then `z`, which is what the
geospatial indexing upstream relied on.

## Decoupling delta from Xapiand

The library is the geometry core of Xapiand's `src/geospatial/cartesian.{h,cc}`.
The changes made during extraction are behavior-preserving:

- **Exception reparented.** `CartesianError` derived from Xapiand's
  `GeoSpatialError` -> `ClientError` (a located-exception `Exception`). Here it
  derives from `std::runtime_error`, defined inline at the top of `cartesian.h`
  with the same name and message-string constructors, so existing
  `catch (const CartesianError&)` sites still work. The `#include "exception.h"`
  is gone.
- **`THROW(CartesianError, "fmt", a)` -> `throw CartesianError(std::format("fmt", a))`.**
  The located-exception `THROW` macro needs a `BaseException`-shaped constructor
  (`__func__, __FILE__, __LINE__, ...`); a `std::runtime_error` subclass does not
  have it, so the throws are spelled out directly. The format strings and `{}`
  placeholders are unchanged (they were already fmt/`std::format` style).
- **`strings::format` -> `std::format`** (`<format>`), dropping `strings.hh`.
  The one call site was `to_string()`.
- **`<cstdint>` added** to `cartesian.h`. The `enum class Units : uint8_t` relied
  on `uint8_t` arriving transitively through `exception.h`; with that include
  gone, the header now pulls in `<cstdint>` itself so it stays self-contained.

The result has no third-party dependencies and no FetchContent in the build: it
compiles against the standard library and system headers alone.

## Why this shape

The type exists to make geometry on points on the Earth straightforward: convert
once to a geocentric vector, normalizing the datum to WGS84, and then everything
downstream is plain 3-vector algebra. Keeping the datum tables and the iterative
inverse in one compiled `.cc` means consumers link a small library rather than
dragging a header full of static tables into every translation unit. The
conversions are the standard textbook ones (prime-vertical forward, Bursa-Wolf
Helmert, Lin & Wang inverse); nothing here is novel, which is the point — it
should agree with any other correct WGS84 implementation to sub-millimeter.
