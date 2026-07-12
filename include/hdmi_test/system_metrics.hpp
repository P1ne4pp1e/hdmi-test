#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace hdmi_test {

struct CpuTimes {
  std::uint64_t total = 0;
  std::uint64_t idle = 0;
};

struct MemoryUsage {
  std::uint64_t total_kib = 0;
  std::uint64_t used_kib = 0;
};

std::optional<CpuTimes> parse_cpu_times(std::string_view stat_contents);
double compute_cpu_percent(CpuTimes previous, CpuTimes current);
std::optional<MemoryUsage> parse_memory_usage(std::string_view meminfo_contents);
std::optional<double> parse_gpu_load_percent(std::string_view load_contents);

}  // namespace hdmi_test
