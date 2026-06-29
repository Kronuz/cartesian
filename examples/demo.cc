// A runnable tour of cartesian.
//
// Build (when this repo is the top-level project):
//   cmake -B build && cmake --build build && ./build/cartesian_demo
//
// The one idea worth taking away: Cartesian turns a geodetic coordinate
// (latitude, longitude, ellipsoid height on some SRID) into the geocentric
// (x, y, z) vector for that point, converting to WGS84 along the way, and back
// again. It also does the small bit of vector algebra you need on those points
// (dot, cross, norm, normalize) and can pretty-print degrees/minutes/seconds.
#include <cstdio>
#include <string>
#include <tuple>

#include "cartesian.h"

static void rule(const char* title) {
	std::printf("\n\033[1m── %s ──\033[0m\n", title);
}

// Convert one real city and print its geocentric vector and normalized form.
static void show(const char* name, double lat, double lon) {
	Cartesian c(lat, lon, 0.0, Cartesian::Units::DEGREES);
	std::printf("  %-14s lat=%9.4f lon=%10.4f\n", name, lat, lon);
	std::printf("    geocentric : %s\n", c.to_string().c_str());
	std::printf("    deg/min/sec: %s\n", c.toDegMinSec().c_str());

	Cartesian unit = c;
	unit.normalize();
	std::printf("    normalized : (% .6f % .6f % .6f), |orig| = %.1f m\n",
	            unit.x, unit.y, unit.z, unit.scale);
}

int main() {
	std::puts("cartesian demo");

	// --- 1. geodetic -> geocentric for real coordinates ----------------------
	rule("a lat/lon becomes a geocentric (x, y, z) vector");
	// San Francisco and Tokyo. The vectors are in meters from the Earth's
	// center; normalizing puts them on the unit sphere and remembers the radius.
	show("San Francisco", 37.7749, -122.4194);
	show("Tokyo", 35.6762, 139.6503);

	// --- 2. round-trip back to lat/lon ---------------------------------------
	rule("and converts straight back to lat/lon/height");
	// Build a point with a non-zero ellipsoid height, then recover it. The
	// inverse (Lin & Wang 1995) lands back on the input to sub-millimeter.
	Cartesian quito(-0.1807, -78.4678, 2850.0, Cartesian::Units::DEGREES);
	auto geo = quito.toGeodetic();
	std::printf("  Quito in  : lat=%.4f lon=%.4f height=%.1f m\n", -0.1807, -78.4678, 2850.0);
	std::printf("  Quito out : lat=%.4f lon=%.4f height=%.1f m\n",
	            std::get<0>(geo), std::get<1>(geo), std::get<2>(geo));

	// --- 3. a little vector algebra on the points ----------------------------
	rule("dot and cross products, and the angular distance");
	// The angle between two surface points (in radians) is acos of the dot
	// product of their unit vectors; distance() does exactly that.
	Cartesian sf(37.7749, -122.4194, 0.0, Cartesian::Units::DEGREES);
	Cartesian ny(40.7128, -74.0060, 0.0, Cartesian::Units::DEGREES);
	sf.normalize();
	ny.normalize();
	double angle = sf.distance(ny);
	std::printf("  SF·NY (dot of unit vectors) = %.6f\n", sf * ny);
	std::printf("  angle SF<->NY               = %.6f rad (%.2f deg)\n",
	            angle, angle * DEG_PER_RAD);

	std::puts("\ndone.");
	return 0;
}
