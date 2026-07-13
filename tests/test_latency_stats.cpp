#include "hdmi_test/latency_stats.hpp"

#include <cassert>

int main() {
  hdmi_test::LatencyWindow window(4);
  assert(window.empty());
  window.add(40.0);
  window.add(10.0);
  window.add(30.0);
  window.add(20.0);
  assert(window.percentile(0.50) == 20.0);
  assert(window.percentile(0.95) == 40.0);
  window.add(50.0);
  assert(window.percentile(0.50) == 20.0);
  assert(window.percentile(0.99) == 50.0);
}
