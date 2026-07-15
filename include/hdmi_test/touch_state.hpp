#pragma once

#include <array>
#include <cstddef>

namespace hdmi_test {

struct TouchPoint {
  int id = -1;
  float x = 0.0F;
  float y = 0.0F;
  bool active = false;
};

class TouchState {
 public:
  static constexpr std::size_t kMaxPoints = 5;

  void down(int id, float x, float y) { update(id, x, y, true); }
  void move(int id, float x, float y) { update(id, x, y, false); }

  void up(int id) {
    for (auto& point : points_) {
      if (point.active && point.id == id) point = {};
    }
  }

  void clear() { points_ = {}; }

  std::size_t active_count() const {
    std::size_t count = 0;
    for (const auto& point : points_) count += point.active ? 1U : 0U;
    return count;
  }

  const std::array<TouchPoint, kMaxPoints>& points() const { return points_; }

 private:
  void update(int id, float x, float y, bool add_if_missing) {
    for (auto& point : points_) {
      if (point.active && point.id == id) {
        point.x = x;
        point.y = y;
        return;
      }
    }
    if (!add_if_missing) return;
    for (auto& point : points_) {
      if (!point.active) {
        point = {id, x, y, true};
        return;
      }
    }
  }

  std::array<TouchPoint, kMaxPoints> points_{};
};

}  // namespace hdmi_test
