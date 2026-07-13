#include "hdmi_test/yolo_preprocess.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

int main() {
  constexpr int kInputSize = 640;
  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0) return 0;
  const std::vector<std::uint8_t> bgr{10U, 20U, 30U, 40U, 50U, 60U};
  float* device_output = nullptr;
  cudaStream_t stream = nullptr;
  assert(cudaMalloc(reinterpret_cast<void**>(&device_output), 3U * kInputSize * kInputSize * sizeof(float)) == cudaSuccess);
  assert(cudaStreamCreate(&stream) == cudaSuccess);
  hdmi_test::YoloGpuPreprocessor preprocessor;
  std::string error;
  assert(preprocessor.run(bgr.data(), 2, 1, device_output, stream, error));
  std::vector<float> output(3U * kInputSize * kInputSize);
  assert(cudaMemcpyAsync(output.data(), device_output, output.size() * sizeof(float), cudaMemcpyDeviceToHost, stream) == cudaSuccess);
  assert(cudaStreamSynchronize(stream) == cudaSuccess);
  const int content = 160 * kInputSize;
  assert(std::fabs(output[content] - 30.0F / 255.0F) < 0.0001F);
  assert(std::fabs(output[kInputSize * kInputSize + content] - 20.0F / 255.0F) < 0.0001F);
  assert(std::fabs(output[2 * kInputSize * kInputSize + content] - 10.0F / 255.0F) < 0.0001F);
  assert(std::fabs(output[0] - 114.0F / 255.0F) < 0.0001F);
  cudaStreamDestroy(stream);
  cudaFree(device_output);
}
