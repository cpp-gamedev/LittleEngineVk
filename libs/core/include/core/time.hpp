#pragma once
#include <chrono>
#include <core/fixed.hpp>
#include <core/std_types.hpp>

namespace le {
namespace stdch = std::chrono;
using namespace std::chrono_literals;

namespace time {
///
/// \brief Typedef for clock used for now()
///
using Clock = stdch::steady_clock;
///
/// \brief Typedef for a time point on Clock
///
using Point = Clock::time_point;
///
/// \brief Typedef for a duration of time
///
template <typename Rep, typename Period>
using Duration = stdch::duration<Rep, Period>;

///
/// \brief Obtain the current time Point
///
Point now() noexcept;
///
/// \brief Cast a Duration to another
///
template <typename Ret, typename Dur>
constexpr Ret cast(Dur&& dur) noexcept;
///
/// \brief Obtain the difference between two time Point instances as a Duration
///
template <typename Ret = Duration<f32, std::ratio<1>>>
constexpr Ret diff(Point const& from, Point const& to = now()) noexcept;
///
/// \brief Obtain the time difference and set out_from = to
///
template <typename Ret = Duration<f32, std::ratio<1>>>
constexpr Ret diffExchg(Point& out_from, Point const& to = now()) noexcept;
} // namespace time

///
/// \brief Typedef for f32 seconds
///
using Time_s = time::Duration<f32, std::ratio<1>>;
///
/// \brief Typedef for s64 milliseconds
///
using Time_ms = time::Duration<s64, std::milli>;
///
/// \brief Typedef for s64 microseconds
///
using Time_us = time::Duration<s64, std::micro>;

// impl
inline time::Point time::now() noexcept {
	return Clock::now();
}
template <typename Ret, typename Dur>
constexpr Ret time::cast(Dur&& dur) noexcept {
	return stdch::duration_cast<Ret>(dur);
}
template <typename Ret>
constexpr Ret time::diff(Point const& from, Point const& to) noexcept {
	return stdch::duration_cast<Ret>(to - from);
}
template <typename Ret>
constexpr Ret time::diffExchg(Point& from, Point const& to) noexcept {
	auto const ret = diff(from, to);
	from = to;
	return ret;
}
} // namespace le
