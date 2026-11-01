// Benchmark: std::hash<Cartesian>, and with --compare two ways to hash a point without it.
//
//   hash<Cartesian>      the library's hash: the bits of x, y, z and the SRID, mixed.
//   hash-combine         std::hash<double> of x, y and z and std::hash<int> of the SRID, folded
//                        with the 64-bit golden-ratio combine (the shape of boost::hash_combine).
//                        It hashes the fields operator== compares, so it has the same equality
//                        semantics as hash<Cartesian>; the check below verifies it.
//   to_string() text     std::hash<std::string> of to_string(). Not a comparator in the same
//                        sense: the text keeps six significant digits of x*scale, y*scale and
//                        z*scale and has no SRID, so it merges points that differ and splits
//                        points that compare equal. The check counts both.
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target cartesian_bench
// Run:   ./build/cartesian_bench [--compare] [--check] [points]
//   --compare  also time hash-combine and the to_string() text hash, after the check passes
//   --check    run the same-results check and stop (the CTest test `cartesian_bench_check`)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "cartesian.h"

namespace {

// What a caller writes without hash<Cartesian>: the standard hash of each field operator==
// compares, folded into one value.
struct HashCombine {
	static size_t combine(size_t seed, size_t value) noexcept {
		return seed ^ (value + static_cast<size_t>(0x9e3779b97f4a7c15ULL) + (seed << 6) + (seed >> 2));
	}
	size_t operator()(const Cartesian& p) const noexcept {
		std::hash<double> hash;
		size_t seed = std::hash<int>{}(p.getSRID());
		seed = combine(seed, hash(p.x));
		seed = combine(seed, hash(p.y));
		seed = combine(seed, hash(p.z));
		return seed;
	}
};

struct HashText {
	size_t operator()(const Cartesian& p) const {
		return std::hash<std::string>{}(p.to_string());
	}
};

// Pairs of points where the hash and operator== disagree: equal points that hash differently
// (split) and different points that hash alike (merged). Two hashes give the same equality
// classes exactly when both counts are zero.
struct Agreement {
	size_t split = 0;
	size_t merged = 0;
	size_t classes = 0;  // distinct hash values
};

template <class Hash>
Agreement agreement(const std::vector<Cartesian>& points) {
	Hash hash;
	std::vector<size_t> h;
	h.reserve(points.size());
	for (auto& p : points) h.push_back(hash(p));
	Agreement result;
	for (size_t i = 0; i < points.size(); ++i) {
		for (size_t j = i + 1; j < points.size(); ++j) {
			bool equal = points[i] == points[j];
			result.split += equal && h[i] != h[j];
			result.merged += !equal && h[i] == h[j];
		}
	}
	result.classes = std::unordered_set<size_t>(h.begin(), h.end()).size();
	return result;
}

// Every point of a small domain: both zeros, signs, a half, both infinities, three scales
// (scale is not compared, so it makes equal points that differ in everything but x, y, z).
std::vector<Cartesian> domain() {
	const double inf = std::numeric_limits<double>::infinity();
	const double values[] = {0.0, -0.0, 1.0, -1.0, 0.5, inf, -inf};
	const double scales[] = {1.0, 2.0, 0.5};
	std::vector<Cartesian> points;
	for (double x : values)
		for (double y : values)
			for (double z : values)
				for (double scale : scales) {
					Cartesian p(x, y, z);
					p.scale = scale;
					points.push_back(p);
				}
	return points;
}

// hash<Cartesian> and hash-combine must both be consistent with operator== on the domain (equal
// points hash alike, different points do not collide) and give the same number of classes, and
// both must tell apart every one of the random points. Returns false on any disagreement.
bool check(const std::vector<Cartesian>& random_points) {
	auto points = domain();
	size_t classes = 0;
	{
		std::vector<Cartesian> representatives;
		for (auto& p : points) {
			if (std::none_of(representatives.begin(), representatives.end(), [&](auto& r) { return r == p; })) {
				representatives.push_back(p);
			}
		}
		classes = representatives.size();
	}
	auto library = agreement<std::hash<Cartesian>>(points);
	auto combined = agreement<HashCombine>(points);
	auto text = agreement<HashText>(points);
	bool ok = true;
	auto row = [&](const char* name, const Agreement& a, bool required) {
		bool same = a.split == 0 && a.merged == 0 && a.classes == classes;
		std::printf("  %-22s %4zu classes, %zu equal pairs split, %zu different pairs merged: %s\n", name, a.classes, a.split, a.merged,
			same ? "same classes as ==" : (required ? "DIFFERENT" : "different (not required)"));
		ok = ok && (same || !required);
	};
	std::printf("same-results check: %zu points (zeros, signs, infinities, scales), %zu classes by ==\n", points.size(), classes);
	row("hash<Cartesian>", library, true);
	row("hash-combine", combined, true);
	row("to_string() text", text, false);
	auto distinct = [&](auto hash) {
		std::unordered_set<size_t> seen;
		for (auto& p : random_points) seen.insert(hash(p));
		return seen.size() == random_points.size();
	};
	bool library_distinct = distinct(std::hash<Cartesian>{});
	bool combined_distinct = distinct(HashCombine{});
	std::printf("  %zu random points: hash<Cartesian> %s, hash-combine %s\n", random_points.size(),
		library_distinct ? "all distinct" : "COLLISION", combined_distinct ? "all distinct" : "COLLISION");
	return ok && library_distinct && combined_distinct;
}

}  // namespace

int main(int argc, char** argv) {
	bool compare = false, check_only = false;
	size_t n = 50000;
	for (int i = 1; i < argc; ++i) {
		if (!std::strcmp(argv[i], "--compare")) compare = true;
		else if (!std::strcmp(argv[i], "--check")) check_only = true;
		else n = std::strtoull(argv[i], nullptr, 10);
	}
	std::mt19937_64 rng(3);
	std::normal_distribution<double> g;
	std::vector<Cartesian> pts;
	for (size_t i = 0; i < n; ++i) pts.emplace_back(g(rng), g(rng), g(rng));
	if (compare || check_only) {
		if (!check(pts)) {
			std::printf("same-results check FAILED\n");
			return 1;
		}
		if (check_only) return 0;
	}
	volatile size_t sink = 0;
	auto time = [&](auto&& fn) {
		double best = 1e300;
		for (int rep = 0; rep < 9; ++rep) {
			auto t0 = std::chrono::steady_clock::now();
			size_t s = 0;
			for (auto& p : pts) s += fn(p);
			auto t1 = std::chrono::steady_clock::now();
			sink = sink + s;
			best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / n);
		}
		return best;
	};
	std::printf("ns per hash, best of 9 rounds over %zu points\n", n);
	std::printf("  hash<Cartesian>              %7.2f\n", time([](const Cartesian& p) { return std::hash<Cartesian>{}(p); }));
	if (compare) {
		std::printf("  hash-combine of hash<double> %7.2f   (same equality as ==, checked)\n", time(HashCombine{}));
		std::printf("  hash of to_string() text     %7.2f   (not the same equality: 6 digits, scaled, no SRID)\n", time(HashText{}));
	}
	return 0;
}
