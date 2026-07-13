#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace hdmi_test {

class LatencyWindow {
 public:
  explicit LatencyWindow(std::size_t capacity) : values_(capacity) {}

  void add(double milliseconds) {
    if (values_.empty() || milliseconds < 0.0) return;
    values_[next_] = milliseconds;
    next_ = (next_ + 1U) % values_.size();
    count_ = std::min(count_ + 1U, values_.size());
  }
  bool empty() const { return count_ == 0U; }
  double percentile(double fraction) const {
    if (empty()) return 0.0;
    std::vector<double> ordered(values_.begin(), values_.begin() + static_cast<std::ptrdiff_t>(count_));
    std::sort(ordered.begin(), ordered.end());
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    if (clamped == 0.0) return ordered.front();
    const std::size_t index = static_cast<std::size_t>(std::ceil(clamped * ordered.size()) - 1.0);
    return ordered[std::min(index, ordered.size() - 1U)];
  }

 private:
  std::vector<double> values_;
  std::size_t next_ = 0;
  std::size_t count_ = 0;
};

}  // namespace hdmi_test
