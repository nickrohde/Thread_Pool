#pragma once

#ifndef __UTIL_HPP
#define __UTIL_HPP

#include <chrono>

typedef std::chrono::high_resolution_clock _Clock;
typedef _Clock::time_point _Timepoint;


template <typename D, typename T>
inline double chrono_duration(T start, T end)
{
	return std::chrono::duration_cast<D>(end - start).count();
}

#endif

