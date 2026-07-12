#include "hdmi_test/system_metrics.hpp"

#include <cassert>

int main() {
  const auto cpu = hdmi_test::parse_cpu_times(
      "cpu  100 20 30 400 10 5 5 0 0 0\n"
      "cpu0 50 10 15 200 5 2 3 0 0 0\n");
  assert(cpu.has_value());
  assert(cpu->total == 570);
  assert(cpu->idle == 410);

  assert(hdmi_test::compute_cpu_percent({570, 410}, {670, 430}) == 80.0);
  assert(hdmi_test::compute_cpu_percent({570, 410}, {570, 410}) == 0.0);

  const auto memory = hdmi_test::parse_memory_usage(
      "MemTotal:        4000 kB\nMemFree: 100 kB\nMemAvailable: 1500 kB\n");
  assert(memory.has_value());
  assert(memory->total_kib == 4000);
  assert(memory->used_kib == 2500);

  const auto gpu = hdmi_test::parse_gpu_load_percent("59\n");
  assert(gpu.has_value());
  assert(*gpu == 59.0);
  assert(!hdmi_test::parse_gpu_load_percent("invalid\n").has_value());
  return 0;
}
