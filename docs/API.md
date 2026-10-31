# cartesian API reference

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
and scalar `*`. `std::hash<Cartesian>` is specialized (mixes the bits of the scaled `x y z` and the SRID; 0.0 and -0.0 hash alike).

Errors: an unsupported SRID, an out-of-range latitude (outside ±90°), or a
zero-norm `normalize()` throw `CartesianError`, which derives from
`std::runtime_error` (see Provenance).

## Caveats

- **Scaling returns a copy**: `c * scale` and `scale * c` never modify `c`.
- Geocentric coordinates are meters in the millions; compare with a tolerance.
