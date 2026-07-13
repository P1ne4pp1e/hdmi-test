#include "hdmi_test/system_metrics.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace hdmi_test {

std::optional<CpuTimes> parse_cpu_times(std::string_view stat_contents) {
  std::istringstream input{std::string(stat_contents)};
  std::string label;
  if (!(input >> label) || label != "cpu") {
    return std::nullopt;
  }

  std::uint64_t values[10]{};
  std::size_t count = 0;
  while (count < std::size(values) && (input >> values[count])) {
    ++count;
  }
  if (count < 4) {
    return std::nullopt;
  }

  CpuTimes result{};
  for (std::size_t index = 0; index < count; ++index) {
    result.total += values[index];
  }
  result.idle = values[3] + (count > 4 ? values[4] : 0);
  return result;
}

double compute_cpu_percent(CpuTimes previous, CpuTimes current) {
  if (current.total <= previous.total || current.idle < previous.idle) {
    return 0.0;
  }
  const std::uint64_t total_delta = current.total - previous.total;
  if (total_delta == 0) {
    return 0.0;
  }
  const std::uint64_t idle_delta = std::min(current.idle - previous.idle, total_delta);
  return 100.0 * static_cast<double>(total_delta - idle_delta) /
         static_cast<double>(total_delta);
}

std::optional<MemoryUsage> parse_memory_usage(std::string_view meminfo_contents) {
  std::istringstream input{std::string(meminfo_contents)};
  std::string key;
  std::uint64_t value = 0;
  std::string unit;
  std::uint64_t total_kib = 0;
  std::uint64_t available_kib = 0;
  while (input >> key >> value >> unit) {
    if (key == "MemTotal:") {
      total_kib = value;
    } else if (key == "MemAvailable:") {
      available_kib = value;
    }
  }
  if (total_kib == 0 || available_kib > total_kib) {
    return std::nullopt;
  }
  return MemoryUsage{total_kib, total_kib - available_kib};
}

std::optional<double> parse_gpu_load_percent(std::string_view load_contents) {
  try {
    std::size_t consumed = 0;
    const double value = std::stod(std::string(load_contents), &consumed);
    if (consumed == 0 || !std::isfinite(value) || value < 0.0 || value > 1000.0) {
      return std::nullopt;
    }
    // Jetson's devfreq `load` node is expressed in per-mille (0..1000),
    // while the dashboard displays percentage.
    return value > 100.0 ? value / 10.0 : value;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace hdmi_test
