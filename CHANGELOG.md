# Changelog

Notable changes to cartesian, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

Upgrading a pin from before 2026-10-30: [the consumer upgrade note](https://github.com/Kronuz/cpp-libraries/blob/main/docs/UPGRADING-2026-10.md#cartesian) lists what changes for callers, how to find affected inputs, the commits not to pin, and the speedup.

## 2026-10-31

### Fixed

- `std::hash<Cartesian>` hashes `x`, `y`, `z` and the SRID, the fields `operator==`
  compares. It hashed `x*scale`, `y*scale`, `z*scale`, so two points that compare
  equal but differ in `scale` (`Cartesian(3, 4, 0)` after `normalize()` and
  `Cartesian(0.6, 0.8, 0)`) hashed differently and an `unordered_set` did not find
  one with the other. The original `to_string()` hash had the same flaw. The test
  checks `a == b` implies `hash(a) == hash(b)` over every pair of an exhaustive
  small domain (both zeros, signs, infinities, NaN, seven scales) and over a million
  random points and scales. A point with a NaN coordinate equals nothing, itself
  included, so it still cannot be found in a set. (`7066e9e`)

### Performance

`cartesian_bench`, the `std::hash<Cartesian>` row, ns per call, five interleaved runs,
Apple clang 17, `-O3`. Hashing three coordinates instead of three coordinates and
three products by `scale` is faster.

| build | before | after |
|---|---|---|
| arm64 (M4 Pro) | 0.806 | 0.765 |
| x86-64 (i9-9980HK) | 2.36 | 2.13 |
| x86-64, `-march=haswell` | 1.85 | 1.56 |

A first version of the fix folded -0.0 into 0.0 on the bits (shift, compare, select);
it measured 0.84 ns on arm64 and 2.71 ns on x86-64, so the fold is the add of 0.0
again. (`49f4075`)

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
