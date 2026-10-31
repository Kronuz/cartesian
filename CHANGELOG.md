# Changelog

Notable changes to cartesian, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

## 2026-10-30

### Changed

- `std::hash<Cartesian>` mixes the bit patterns of `x*scale`, `y*scale`, `z*scale`,
  and the SRID, folding -0.0 into 0.0. Before, it formatted the point with
  `to_string()` and hashed the string, which was slow and made nearby points
  collide. It is about 300x faster (`build/cartesian_bench`). Hash values change;
  they came from `std::hash<std::string>` and no library in the family persists
  them. (`79ed663`)

### Added

- `bench/bench.cc` (`cartesian_bench`) times the hash against hashing `to_string()`.

## 2026-10-16

### Fixed

- `operator^=` wrote the third cross-product component into `x`, so `z` never
  updated. It assigns `z`. (`34bf069`)
- The member `operator*(double)` scaled `*this` in place, so for a non-const `c`,
  `c * 2.0` silently changed `c`. The member is gone: `c * s` and `s * c` resolve to
  the free overloads, which return a scaled copy. Nothing in htm or Xapiand used
  either operator for its side effect. (`34bf069`)
