#pragma once

#include <ranges>
#include <vector>

#include "nigiri/common/interval.h"

#include "utl/enumerate.h"
#include "utl/generator.h"
#include "utl/to_vec.h"
#include "utl/verify.h"

namespace icc {

template <typename Time>
struct state_change {
  friend bool operator==(state_change const&, state_change const&) = default;
  Time valid_from_;
  bool state_;
};

template <typename Time, bool Default = true>
std::vector<state_change<Time>> intervals_to_state_changes(
    std::vector<nigiri::interval<Time>> const& iv) {
  using Duration = typename Time::duration;
  auto ret = std::vector<state_change<Time>>{};
  ret.emplace_back(Time{Duration{0}}, Default);
  for (auto const& i : iv) {
    ret.emplace_back(i.from_, !Default);
    ret.emplace_back(i.to_, Default);
  }
  return ret;
}

template <typename Time>
utl::generator<std::pair<Time, std::vector<bool>>> get_state_changes(
    std::vector<std::vector<state_change<Time>>> const& c) {
  using It = std::vector<state_change<Time>>::const_iterator;

  struct range {
    bool is_finished() const { return curr_ == end_; }
    bool state_;
    It curr_, end_;
  };

  auto its = utl::to_vec(c, [](auto&& v) {
    utl::verify(!v.empty(), "empty state vector not allowed");
    return range{v[0].state_, v.begin(), v.end()};
  });

  auto const all_finished = [&]() {
    return std::ranges::all_of(its, [&](auto&& r) { return r.is_finished(); });
  };

  auto const next = [&]() -> range& {
    return *std::ranges::min_element(its, [&](auto&& a, auto&& b) {
      return a.curr_->valid_from_ < b.curr_->valid_from_;
    });
  };

  auto const get_state = [&]() -> std::vector<bool> {
    auto s = std::vector<bool>(its.size());
    for (auto const [i, r] : utl::enumerate(its)) {
      s[i] = r.state_;
    }
    return s;
  };

  auto pred_t = std::optional<std::pair<Time, std::vector<bool>>>{};
  while (!all_finished()) {
    auto& n = next();
    auto const t = n.curr_->valid_from_;
    n.state_ = n.curr_->state_;
    ++n.curr_;

    auto const state = std::pair{t, get_state()};
    if (!pred_t.has_value()) {
      pred_t = state;
      continue;
    }

    if (pred_t->first != state.first) {
      co_yield *pred_t;
    }
    pred_t = state;
  }

  if (pred_t.has_value()) {
    co_yield *pred_t;
  }
}

}  // namespace icc