#ifndef TIMER_H
#define TIMER_H

#include "asio.hpp"
#include <ctime>
#include <chrono>
#include <iostream>

struct time_t_clock {
  using duration = std::chrono::steady_clock::duration;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<time_t_clock>;

  static constexpr bool is_steady = false;

  static time_point now() noexcept {
    return time_point() + std::chrono::seconds(std::time(0));
  }
};

struct time_t_wait_traits {
  static time_t_clock::duration to_wait_duration(const time_t_clock::duration& d) {
    if(d > std::chrono::seconds(1)) {
      return d - std::chrono::seconds(1);
    }
    else if(d > std::chrono::seconds(0)) {
      return std::chrono::milliseconds(10);
    }
    else {
      return std::chrono::seconds(0);
    }
  }

  static time_t_clock::duration to_wait_duration(const time_t_clock::time_point& d) {
    return to_wait_duration(d - time_t_clock::now());
  }
};

using timer_type = asio::basic_waitable_timer<time_t_clock, time_t_wait_traits>;

#endif // TIMER_H
