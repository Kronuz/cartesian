// Benchmark: std::hash<Cartesian> (bit mixing) against the original
// (hash of to_string()).
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target cartesian_bench
// Run:   ./build/cartesian_bench [points]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <random>
#include <vector>

#include "cartesian.h"

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 50000;
	std::mt19937_64 rng(3);
	std::normal_distribution<double> g;
	std::vector<Cartesian> pts;
	for (size_t i = 0; i < n; ++i) pts.emplace_back(g(rng), g(rng), g(rng));
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
	std::printf("hash<Cartesian>  old(to_string) %7.1f ns   new(bits) %6.1f ns\n",
		time([](const Cartesian& p) { return std::hash<std::string>{}(p.to_string()); }),
		time([](const Cartesian& p) { return std::hash<Cartesian>{}(p); }));
	return 0;
}
