#pragma once
#include <SYS/SYS_Math.h>

namespace hreeble {
	template <typename T>
	auto rand_range(T start, T end, uint seed) {
		return SYSfastCeil(SYSfastRandom(seed) * (end - start)) + start;
	}

	template <typename T>
	auto rand_choice(const T &collection, const uint &seed) {
		auto index = SYSfastFloor(SYSfastRandom(seed) * collection.enitries());
		return collection(index);
	}

	bool rand_bool(const uint &seed) {
		uint seed_ = seed;
		return SYSfastRandom(seed_) > 0.5 ? true : false;
	}
}

