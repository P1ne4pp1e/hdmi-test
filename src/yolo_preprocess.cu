#include "hdmi_test/yolo_preprocess.hpp"

#include <algorithm>
#include <cmath>

namespace hdmi_test {
namespace {
constexpr int kInputSize = 640;

__global__ void bgr_letterbox_to_rgb_chw(const std::uint8_t* bgr, int source_width, int source_height,
                                          float* output, float scale, int padding_x, int padding_y) {
  const int index = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  constexpr int kOutputElements = 3 * kInputSize * kInputSize;
  if (index >= kOutputElements) return;
  const int channel = index / (kInputSize * kInputSize);
  const int pixel = index % (kInputSize * kInputSize);
  const int x = pixel % kInputSize;
  const int y = pixel / kInputSize;
  const int resized_width = static_cast<int>(roundf(source_width * scale));
  const int resized_height = static_cast<int>(roundf(source_height * scale));
  if (x < padding_x || x >= padding_x + resized_width || y < padding_y || y >= padding_y + resized_height) {
    output[index] = 114.0F / 255.0F;
    return;
  }
  const int source_x = min(static_cast<int>((x - padding_x) / scale), source_width - 1);
  const int source_y = min(static_cast<int>((y - padding_y) / scale), source_height - 1);
  const int source = (source_y * source_width + source_x) * 3;
  // Camera frames are BGR, while YOLOv8 consumes normalized RGB CHW.
  const int component = channel == 0 ? 2 : (channel == 1 ? 1 : 0);
  output[index] = static_cast<float>(bgr[source + component]) / 255.0F;
}

std::string cuda_error(const char* action) {
  return std::string(action) + " failed: " + cudaGetErrorString(cudaGetLastError());
}
}  // namespace

YoloGpuPreprocessor::~YoloGpuPreprocessor() {
  if (device_bgr_) cudaFree(device_bgr_);
}

bool YoloGpuPreprocessor::run(const std::uint8_t* bgr, int width, int height, float* chw_output,
                              cudaStream_t stream, std::string& error) {
  if (!bgr || width <= 0 || height <= 0 || !chw_output) {
    error = "invalid GPU preprocessing input";
    return false;
  }
  const std::size_t bytes = static_cast<std::size_t>(width) * height * 3U;
  if (bytes > capacity_bytes_) {
    if (device_bgr_) cudaFree(device_bgr_);
    device_bgr_ = nullptr;
    capacity_bytes_ = 0;
    if (cudaMalloc(&device_bgr_, bytes) != cudaSuccess) {
      error = cuda_error("allocate camera upload buffer");
      return false;
    }
    capacity_bytes_ = bytes;
  }
  if (cudaMemcpyAsync(device_bgr_, bgr, bytes, cudaMemcpyHostToDevice, stream) != cudaSuccess) {
    error = cuda_error("upload camera frame");
    return false;
  }
  const float scale = std::min(static_cast<float>(kInputSize) / width, static_cast<float>(kInputSize) / height);
  const int resized_width = static_cast<int>(std::round(width * scale));
  const int resized_height = static_cast<int>(std::round(height * scale));
  const int padding_x = (kInputSize - resized_width) / 2;
  const int padding_y = (kInputSize - resized_height) / 2;
  constexpr int kThreads = 256;
  constexpr int kElements = 3 * kInputSize * kInputSize;
  bgr_letterbox_to_rgb_chw<<<(kElements + kThreads - 1) / kThreads, kThreads, 0, stream>>>(
      device_bgr_, width, height, chw_output, scale, padding_x, padding_y);
  if (cudaGetLastError() != cudaSuccess) {
    error = cuda_error("launch GPU preprocessing kernel");
    return false;
  }
  return true;
}

}  // namespace hdmi_test
