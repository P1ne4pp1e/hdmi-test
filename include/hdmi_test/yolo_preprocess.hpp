#pragma once

#include <cuda_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace hdmi_test {

class YoloGpuPreprocessor {
 public:
  YoloGpuPreprocessor() = default;
  ~YoloGpuPreprocessor();
  YoloGpuPreprocessor(const YoloGpuPreprocessor&) = delete;
  YoloGpuPreprocessor& operator=(const YoloGpuPreprocessor&) = delete;

  bool run(const std::uint8_t* bgr, int width, int height, float* chw_output,
           cudaStream_t stream, std::string& error);

 private:
  std::uint8_t* device_bgr_ = nullptr;
  std::size_t capacity_bytes_ = 0;
};

}  // namespace hdmi_test
