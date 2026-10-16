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
#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

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

	// toGeodetic recovers latitude, longitude, and height (meters). Height was
	// zero, so it should come back within a millimeter of zero.
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


int main() {
	test_to_cartesian();
	test_default_ctor();
	test_round_trip();
	test_vector_algebra();
	test_srid_support();
	test_errors_throw();
	std::printf("all cartesian tests passed\n");
	return 0;
}
