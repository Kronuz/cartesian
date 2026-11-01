// Smoke test for the standalone cartesian library.
//
// Exercises the geocentric coordinate type and the WGS84 / geodesy
// conversions: constructing a Cartesian from lat/lon/height (degrees and
// radians) and checking the geocentric x/y/z against values computed straight
// from the conversion formula; the lat/lon round-trip through toGeodetic /
// toLatLon; the vector algebra (dot, cross, +, -, norm, normalize, inverse);
// the default constructor; is_SRID_supported; and that an unsupported SRID and
// an out-of-range latitude both throw CartesianError (now a
// std::runtime_error, so a std::exception catch sees the message too).
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <tuple>
#include <utility>
#include <vector>

#include "cartesian.h"


// Absolute-tolerance compare for doubles. Geocentric coordinates are in meters
// in the millions, so a tolerance of 1e-3 (a millimeter) is plenty tight.
static bool close(double got, double want, double tol) {
	return std::abs(got - want) <= tol;
}


// ---------------------------------------------------------------------------
// Construct from lat/lon/height and check the geocentric vector against values
// computed independently from the WGS84 forward formula.
// ---------------------------------------------------------------------------

static void test_to_cartesian() {
	// Equator on the prime meridian, height 0: the point sits on the major
	// axis, so it is exactly (a, 0, 0) with a = WGS84 semi-major axis.
	Cartesian eq(0.0, 0.0, 0.0, Cartesian::Units::DEGREES);
	assert(close(eq.x, 6378137.0, 1e-3));
	assert(close(eq.y, 0.0, 1e-3));
	assert(close(eq.z, 0.0, 1e-3));
	assert(eq.getSRID() == WGS84);

	// 40N, 0E, height 0. Values from the same N = a / sqrt(1 - e2 sin^2 lat)
	// prime-vertical formula the library uses.
	Cartesian p(40.0, 0.0, 0.0, Cartesian::Units::DEGREES);
	assert(close(p.x, 4892707.600072692, 1e-3));
	assert(close(p.y, 0.0, 1e-3));
	assert(close(p.z, 4077985.572200376, 1e-3));

	// A real city: San Francisco, 37.7749N, 122.4194W. Longitude is negative,
	// so both x and y are negative; z is positive (northern hemisphere).
	Cartesian sf(37.7749, -122.4194, 0.0, Cartesian::Units::DEGREES);
	assert(close(sf.x, -2706174.8466110784, 1e-3));
	assert(close(sf.y, -4261059.489296481, 1e-3));
	assert(close(sf.z, 3885725.490023605, 1e-3));

	// Degrees and radians must agree: 40 degrees == 40 * RAD_PER_DEG radians.
	Cartesian p_rad(40.0 * RAD_PER_DEG, 0.0, 0.0, Cartesian::Units::RADIANS);
	assert(close(p_rad.x, p.x, 1e-6));
	assert(close(p_rad.y, p.y, 1e-6));
	assert(close(p_rad.z, p.z, 1e-6));

	std::printf("cartesian forward OK: equator, 40N/0E, San Francisco, deg==rad\n");
}


// ---------------------------------------------------------------------------
// The default constructor is the equator / prime meridian point (a, 0, 0).
// ---------------------------------------------------------------------------

static void test_default_ctor() {
	Cartesian d;
	assert(d.getSRID() == WGS84);
	assert(close(d.x, 6378137.0, 1e-3));
	assert(close(d.y, 0.0, 1e-3));
	assert(close(d.z, 0.0, 1e-3));

	std::printf("cartesian default ctor OK: (a, 0, 0) on WGS84\n");
}


// ---------------------------------------------------------------------------
// Round-trip: lat/lon -> cartesian -> lat/lon (and -> geodetic with height).
// ---------------------------------------------------------------------------

static void test_round_trip() {
	const double lat = 37.7749, lon = -122.4194, height = 0.0;
	Cartesian sf(lat, lon, height, Cartesian::Units::DEGREES);

	// toLatLon recovers the input latitude and longitude in degrees.
	auto ll = sf.toLatLon();
	assert(close(ll.first, lat, 1e-7));
	assert(close(ll.second, lon, 1e-7));

	// toGeodetic recovers latitude, longitude, and height (meters). The input
	// height is zero, so it comes back within a millimeter of zero.
	auto geo = sf.toGeodetic();
	assert(close(std::get<0>(geo), lat, 1e-7));
	assert(close(std::get<1>(geo), lon, 1e-7));
	assert(close(std::get<2>(geo), height, 1e-3));

	// A non-zero ellipsoid height round-trips too.
	Cartesian high(10.0, 20.0, 1234.5, Cartesian::Units::DEGREES);
	auto hgeo = high.toGeodetic();
	assert(close(std::get<0>(hgeo), 10.0, 1e-7));
	assert(close(std::get<1>(hgeo), 20.0, 1e-7));
	assert(close(std::get<2>(hgeo), 1234.5, 1e-3));

	// toDegMinSec produces a non-empty string for a northern/western point and
	// tags the hemispheres ('N' and 'W' appear for San Francisco).
	std::string dms = sf.toDegMinSec();
	assert(!dms.empty());
	assert(dms.find('N') != std::string::npos);
	assert(dms.find('W') != std::string::npos);

	std::printf("cartesian round-trip OK: toLatLon / toGeodetic / toDegMinSec\n");
}


// ---------------------------------------------------------------------------
// Vector algebra: dot, cross, +, -, norm, normalize, inverse, to_string.
// ---------------------------------------------------------------------------

static void test_vector_algebra() {
	Cartesian a(1.0, 0.0, 0.0);
	Cartesian b(0.0, 1.0, 0.0);

	// Dot product of orthogonal unit axes is zero.
	assert(close(a * b, 0.0, 1e-12));

	// Cross product of x_hat and y_hat is z_hat.
	Cartesian c = a ^ b;
	assert(close(c.x, 0.0, 1e-12));
	assert(close(c.y, 0.0, 1e-12));
	assert(close(c.z, 1.0, 1e-12));

	// ^= is the in-place cross product: every component, z included.
	Cartesian u(1.0, 2.0, 3.0), q(4.0, 5.0, 6.0);
	Cartesian expect = u ^ q;   // (-3, 6, -3)
	u ^= q;
	assert(close(u.x, expect.x, 1e-12) && close(u.y, expect.y, 1e-12) && close(u.z, expect.z, 1e-12));
	assert(close(u.z, -3.0, 1e-12));

	// Scaling returns a new vector and leaves its operand alone.
	Cartesian w(1.0, -2.0, 0.5);
	Cartesian w3 = w * 3.0;
	assert(close(w.x, 1.0, 1e-12) && close(w.y, -2.0, 1e-12) && close(w.z, 0.5, 1e-12));
	assert(close(w3.x, 3.0, 1e-12) && close(w3.y, -6.0, 1e-12) && close(w3.z, 1.5, 1e-12));
	Cartesian w2 = 2.0 * w;
	assert(close(w2.y, -4.0, 1e-12) && close(w.y, -2.0, 1e-12));

	// Addition and subtraction are componentwise.
	Cartesian sum = a + b;
	assert(close(sum.x, 1.0, 1e-12) && close(sum.y, 1.0, 1e-12) && close(sum.z, 0.0, 1e-12));
	Cartesian diff = a - b;
	assert(close(diff.x, 1.0, 1e-12) && close(diff.y, -1.0, 1e-12) && close(diff.z, 0.0, 1e-12));

	// norm of (3, 4, 0) is 5; normalize scales it onto the unit sphere and the
	// stored scale factor is the original norm.
	Cartesian v(3.0, 4.0, 0.0);
	assert(close(v.norm(), 5.0, 1e-12));
	v.normalize();
	assert(close(v.norm(), 1.0, 1e-12));
	assert(close(v.scale, 5.0, 1e-12));

	// inverse negates every component.
	Cartesian n(1.0, -2.0, 3.0);
	n.inverse();
	assert(close(n.x, -1.0, 1e-12) && close(n.y, 2.0, 1e-12) && close(n.z, -3.0, 1e-12));

	// to_string carries the default CRS prefix.
	assert(a.to_string().find("SRID=4326;") != std::string::npos);

	std::printf("cartesian algebra OK: dot, cross, +, -, norm, normalize, inverse\n");
}


// ---------------------------------------------------------------------------
// is_SRID_supported reports the supported set.
// ---------------------------------------------------------------------------

static void test_srid_support() {
	assert(Cartesian::is_SRID_supported(WGS84));
	assert(Cartesian::is_SRID_supported(NAD83));
	assert(!Cartesian::is_SRID_supported(9999));

	std::printf("cartesian SRID support OK: WGS84/NAD83 yes, 9999 no\n");
}


// ---------------------------------------------------------------------------
// Error paths: an unsupported SRID and an out-of-range latitude both throw
// CartesianError. Since CartesianError now derives from std::runtime_error, a
// std::exception catch also sees it (and the message text is preserved).
// ---------------------------------------------------------------------------

static void test_errors_throw() {
	// Unsupported SRID: the lat/lon constructor looks the datum up and throws.
	bool threw_srid = false;
	try {
		Cartesian bad(10.0, 10.0, 0.0, Cartesian::Units::DEGREES, 9999);
		(void)bad;
	} catch (const CartesianError& e) {
		threw_srid = true;
		// Message text preserved from the original THROW format string.
		assert(std::string(e.what()).find("9999") != std::string::npos);
		assert(std::string(e.what()).find("not supported") != std::string::npos);
	}
	assert(threw_srid);

	// The same error is catchable as a plain std::exception (decoupled base).
	bool threw_as_std = false;
	try {
		Cartesian bad2(0.0, 0.0, 0.0, Cartesian::Units::DEGREES, 9999);
		(void)bad2;
	} catch (const std::exception&) {
		threw_as_std = true;
	}
	assert(threw_as_std);

	// Out-of-range latitude: 200 degrees is past +/- 90, so it throws.
	bool threw_lat = false;
	try {
		Cartesian bad3(200.0, 0.0, 0.0, Cartesian::Units::DEGREES);
		(void)bad3;
	} catch (const CartesianError& e) {
		threw_lat = true;
		assert(std::string(e.what()).find("Latitude out-of-range") != std::string::npos);
	}
	assert(threw_lat);

	std::printf("cartesian errors OK: unsupported SRID + out-of-range latitude throw\n");
}


// ---------------------------------------------------------------------------
// std::hash<Cartesian> hashes what operator== compares (x, y, z, SRID), so
// a == b implies hash(a) == hash(b): scale is not compared and is not hashed,
// and 0.0 and -0.0 hash alike. A point with a NaN coordinate equals nothing, so
// the property says nothing about it. Distinct points hash differently, and the
// type works as an unordered key.
// ---------------------------------------------------------------------------

#define CHECK(c) do { if (!(c)) { std::printf("CHECK failed: %s (line %d)\n", #c, __LINE__); std::fflush(stdout); std::abort(); } } while (0)

static uint64_t bits_of(double v) {
	uint64_t b;
	std::memcpy(&b, &v, sizeof b);
	return b;
}

static Cartesian with_scale(double x, double y, double z, double scale) {
	Cartesian p(x, y, z);
	p.scale = scale;
	return p;
}

static void test_hash() {
	std::hash<Cartesian> h;
	CHECK(h(Cartesian(1.0, 2.0, 3.0)) == h(Cartesian(1.0, 2.0, 3.0)));
	CHECK(h(Cartesian(0.0, 0.0, 1.0)) == h(Cartesian(-0.0, -0.0, 1.0)));
	CHECK(h(Cartesian(1.0, 2.0, 3.0)) != h(Cartesian(1.0, 3.0, 2.0)));
	CHECK(h(Cartesian(1.0, 2.0, 3.0)) != h(Cartesian(1.0, 2.0, 3.0 + 1e-9)));
	CHECK(h(Cartesian(1.0, 2.0, 3.0, WGS84)) != h(Cartesian(1.0, 2.0, 3.0, OSGB36)));

	std::unordered_set<Cartesian> set;
	for (int i = 0; i < 1000; ++i) {
		set.insert(Cartesian(i * 0.001, 1.0, 2.0));
		set.insert(Cartesian(i * 0.001, 1.0, 2.0));
	}
	CHECK(set.size() == 1000);
	CHECK(set.count(Cartesian(0.5, 1.0, 2.0)) == 1);
}


// A normalized point keeps the old norm in `scale`; operator== ignores it, so the
// point equals the same coordinates built directly, and so must its hash.
static void test_hash_scale() {
	std::hash<Cartesian> h;
	Cartesian n(3.0, 4.0, 0.0);
	n.normalize();
	Cartesian m(0.6, 0.8, 0.0);
	CHECK(n.scale == 5.0 && m.scale == 1.0);
	CHECK(n == m);
	CHECK(h(n) == h(m));
	std::unordered_set<Cartesian> set{m};
	CHECK(set.count(n) == 1);
	set.insert(n);
	CHECK(set.size() == 1);

	// The scale of a point is not part of its identity at all.
	for (double scale : {0.0, -0.0, 1e-300, 2.0, 1e300, std::numeric_limits<double>::infinity(),
			-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
		CHECK(h(with_scale(1.0, -2.0, 3.0, scale)) == h(Cartesian(1.0, -2.0, 3.0)));
	}
}


// Exhaustive over a small domain: every point over a set of special coordinates
// (both zeros, signs, a denormal-adjacent value, infinities, NaN) with a set of
// scales, every ordered pair. Wherever a == b the hashes are equal; where the
// coordinates differ in bits (zeros aside) the hashes differ.
static void test_hash_domain() {
	std::hash<Cartesian> h;
	const double inf = std::numeric_limits<double>::infinity();
	const double nan = std::numeric_limits<double>::quiet_NaN();
	const double values[] = {0.0, -0.0, 1.0, -1.0, 0.5, 1e-300, inf, nan};
	const double scales[] = {1.0, 5.0, 0.25, 0.0, -0.0, inf, nan};

	struct Item {
		Cartesian p;
		size_t hash;
	};
	std::vector<Item> items;
	for (double x : values) for (double y : values) for (double z : values) for (double s : scales) {
		Cartesian p = with_scale(x, y, z, s);
		items.push_back({p, h(p)});
	}

	size_t equal_pairs = 0;
	for (const auto& a : items) {
		for (const auto& b : items) {
			if (a.p == b.p) {
				++equal_pairs;
				if (a.hash != b.hash) {
					std::printf("hash differs for equal points (%g %g %g scale %g) and (%g %g %g scale %g)\n",
						a.p.x, a.p.y, a.p.z, a.p.scale, b.p.x, b.p.y, b.p.z, b.p.scale);
					std::fflush(stdout);
					std::abort();
				}
			}
		}
	}
	// Each point equals itself under every non-NaN scale, so equal pairs are far
	// more numerous than points.
	CHECK(equal_pairs > items.size());

	// Distinct coordinate triples (zeros folded) give distinct hashes.
	std::unordered_set<uint64_t> hashes;
	std::set<std::array<uint64_t, 3>> distinct;
	auto fold = [](double v) { return v == 0.0 ? uint64_t(0) : bits_of(v); };
	for (const auto& it : items) {
		if (distinct.insert(std::array<uint64_t, 3>{fold(it.p.x), fold(it.p.y), fold(it.p.z)}).second) {
			CHECK(hashes.insert(it.hash).second);
		}
	}
}


// Random points with random scales: a copy of a point with another scale, and a
// copy with the signs of its zeros flipped, equal the point and hash as it does.
static void test_hash_random() {
	std::hash<Cartesian> h;
	std::mt19937_64 rng(20261031);
	std::normal_distribution<double> g(0.0, 1e6);
	const double specials[] = {0.0, -0.0, 1.0, -1.0, 1e-300, -1e-300, 1e300,
		std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::max()};
	auto coord = [&]() -> double {
		switch (rng() % 4) {
			case 0: return specials[rng() % (sizeof specials / sizeof *specials)];
			case 1: { uint64_t b = rng(); double v; std::memcpy(&v, &b, sizeof v); return std::isnan(v) ? 0.0 : v; }
			case 2: return static_cast<double>(static_cast<int64_t>(rng() % 2001) - 1000);
			default: return g(rng);
		}
	};
	auto scale = [&]() -> double {
		switch (rng() % 4) {
			case 0: return 1.0;
			case 1: return specials[rng() % (sizeof specials / sizeof *specials)];
			case 2: return std::numeric_limits<double>::quiet_NaN();
			default: return g(rng);
		}
	};
	for (int i = 0; i < 1000000; ++i) {
		double x = coord(), y = coord(), z = coord();
		Cartesian a = with_scale(x, y, z, scale());
		Cartesian b = with_scale(x == 0.0 ? -x : x, y == 0.0 ? -y : y, z == 0.0 ? -z : z, scale());
		CHECK(a == b);
		CHECK(h(a) == h(b));
	}
}


int main() {
	test_hash();
	test_hash_scale();
	test_hash_domain();
	test_hash_random();
	test_to_cartesian();
	test_default_ctor();
	test_round_trip();
	test_vector_algebra();
	test_srid_support();
	test_errors_throw();
	std::printf("all cartesian tests passed\n");
	return 0;
}
