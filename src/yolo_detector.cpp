#include "hdmi_test/yolo_detector.hpp"

#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <memory>
#include <numeric>
#include <sstream>

namespace hdmi_test {
namespace {
constexpr int kInputSize = 640;
constexpr int kClassCount = 80;
constexpr int kPredictionCount = 8400;
constexpr float kConfidenceThreshold = 0.35F;
constexpr float kIouThreshold = 0.45F;

class TensorRtLogger final : public nvinfer1::ILogger {
 public:
  void log(Severity severity, const char* message) noexcept override {
    if (severity <= Severity::kWARNING) std::fprintf(stderr, "TensorRT: %s\n", message);
  }
};

std::string trt_error(const char* action) {
  std::ostringstream output;
  output << action << " failed: " << cudaGetErrorString(cudaGetLastError());
  return output.str();
}

float intersection_over_union(const YoloDetection& first, const YoloDetection& second) {
  const float left = std::max(first.left, second.left);
  const float top = std::max(first.top, second.top);
  const float right = std::min(first.right, second.right);
  const float bottom = std::min(first.bottom, second.bottom);
  const float intersection = std::max(0.0F, right - left) * std::max(0.0F, bottom - top);
  const float first_area = std::max(0.0F, first.right - first.left) * std::max(0.0F, first.bottom - first.top);
  const float second_area = std::max(0.0F, second.right - second.left) * std::max(0.0F, second.bottom - second.top);
  const float denominator = first_area + second_area - intersection;
  return denominator > 0.0F ? intersection / denominator : 0.0F;
}

void draw_pixel(std::vector<std::uint8_t>& image, int width, int height, int x, int y,
                std::array<std::uint8_t, 3> color) {
  if (x < 0 || y < 0 || x >= width || y >= height) return;
  const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 3U;
  image[offset] = color[0];
  image[offset + 1] = color[1];
  image[offset + 2] = color[2];
}

void draw_box(std::vector<std::uint8_t>& image, int width, int height, const YoloDetection& detection) {
  constexpr std::array<std::array<std::uint8_t, 3>, 6> kColors{{
      {44U, 229U, 255U}, {255U, 132U, 48U}, {255U, 92U, 220U},
      {81U, 255U, 121U}, {224U, 157U, 40U}, {238U, 238U, 238U},
  }};
  const auto color = kColors[static_cast<std::size_t>(detection.class_id) % kColors.size()];
  const int left = std::clamp(static_cast<int>(std::floor(detection.left)), 0, width - 1);
  const int right = std::clamp(static_cast<int>(std::ceil(detection.right)), 0, width - 1);
  const int top = std::clamp(static_cast<int>(std::floor(detection.top)), 0, height - 1);
  const int bottom = std::clamp(static_cast<int>(std::ceil(detection.bottom)), 0, height - 1);
  // Source frames are reduced to a 352×264 HDMI panel. Eight source pixels
  // preserve a clearly visible two-pixel edge after that downscale.
  for (int thickness = 0; thickness < 8; ++thickness) {
    for (int x = left; x <= right; ++x) {
      draw_pixel(image, width, height, x, top + thickness, color);
      draw_pixel(image, width, height, x, bottom - thickness, color);
    }
    for (int y = top; y <= bottom; ++y) {
      draw_pixel(image, width, height, left + thickness, y, color);
      draw_pixel(image, width, height, right - thickness, y, color);
    }
  }
}

}  // namespace

std::vector<YoloDetection> non_maximum_suppression(std::vector<YoloDetection> detections,
                                                    float iou_threshold) {
  std::sort(detections.begin(), detections.end(), [](const auto& first, const auto& second) {
    return first.confidence > second.confidence;
  });
  std::vector<YoloDetection> selected;
  for (const auto& candidate : detections) {
    bool overlaps = false;
    for (const auto& accepted : selected) {
      if (candidate.class_id == accepted.class_id && intersection_over_union(candidate, accepted) > iou_threshold) {
        overlaps = true;
        break;
      }
    }
    if (!overlaps) selected.push_back(candidate);
  }
  return selected;
}

class YoloDetector::Impl {
 public:
  struct OutputBinding {
    std::string name;
    void* data = nullptr;
  };
  Impl(std::string onnx_path, std::string engine_path)
      : onnx_path_(std::move(onnx_path)), engine_path_(std::move(engine_path)) {}
  ~Impl() {
    if (stream_) cudaStreamDestroy(stream_);
    if (input_) cudaFree(input_);
    for (auto& output : outputs_) if (output.data) cudaFree(output.data);
  }

  bool initialize(std::string& error) {
    std::vector<char> serialized;
    std::ifstream engine_file(engine_path_, std::ios::binary);
    if (engine_file) {
      serialized.assign(std::istreambuf_iterator<char>(engine_file), std::istreambuf_iterator<char>());
    } else if (!build_engine(serialized, error)) {
      return false;
    }
    runtime_.reset(nvinfer1::createInferRuntime(logger_));
    if (!runtime_) { error = "create TensorRT runtime failed"; return false; }
    engine_.reset(runtime_->deserializeCudaEngine(serialized.data(), serialized.size()));
    if (!engine_) { error = "deserialize TensorRT engine failed"; return false; }
    context_.reset(engine_->createExecutionContext());
    if (!context_) { error = "create TensorRT execution context failed"; return false; }
    for (int index = 0; index < engine_->getNbIOTensors(); ++index) {
      const char* tensor_name = engine_->getIOTensorName(index);
      if (engine_->getTensorIOMode(tensor_name) == nvinfer1::TensorIOMode::kINPUT) input_name_ = tensor_name;
      if (engine_->getTensorIOMode(tensor_name) == nvinfer1::TensorIOMode::kOUTPUT) outputs_.push_back({tensor_name, nullptr});
    }
    if (input_name_.empty() || outputs_.empty()) { error = "TensorRT engine has invalid IO tensors"; return false; }
    const std::size_t input_bytes = 3U * kInputSize * kInputSize * sizeof(float);
    if (cudaMalloc(&input_, input_bytes) != cudaSuccess ||
        cudaStreamCreate(&stream_) != cudaSuccess) { error = trt_error("allocate TensorRT buffers"); return false; }
    nvinfer1::Dims input_shape{};
    input_shape.nbDims = 4;
    input_shape.d[0] = 1;
    input_shape.d[1] = 3;
    input_shape.d[2] = kInputSize;
    input_shape.d[3] = kInputSize;
    if (!context_->setInputShape(input_name_.c_str(), input_shape)) {
      error = "set TensorRT YOLO input shape failed";
      return false;
    }
    for (auto& output : outputs_) {
      const auto shape = context_->getTensorShape(output.name.c_str());
      std::size_t elements = 1;
      for (int dimension = 0; dimension < shape.nbDims; ++dimension) {
        if (shape.d[dimension] <= 0) { error = "TensorRT YOLO output has unresolved shape"; return false; }
        elements *= static_cast<std::size_t>(shape.d[dimension]);
      }
      // All exported YOLO outputs are float tensors.  This deliberately allocates
      // enough space for the three additional head outputs emitted by this ONNX.
      if (cudaMalloc(&output.data, elements * sizeof(float)) != cudaSuccess) { error = trt_error("allocate YOLO output"); return false; }
      if (output.name == "output0") output_ = output.data;
    }
    if (!output_ || !context_->setInputTensorAddress(input_name_.c_str(), input_)) {
      error = "bind TensorRT IO tensors failed";
      return false;
    }
    for (const auto& output : outputs_) {
      if (!context_->setOutputTensorAddress(output.name.c_str(), output.data)) { error = "bind TensorRT output failed"; return false; }
    }
    initialized_ = true;
    return true;
  }

  bool infer(const std::uint8_t* bgr, int width, int height, std::uint64_t frame_number,
             YoloFrame& result, std::string& error) {
    if (!initialized_) { error = "YOLO detector is not initialized"; return false; }
    const auto preprocess_begin = std::chrono::steady_clock::now();
    const float scale = std::min(static_cast<float>(kInputSize) / width, static_cast<float>(kInputSize) / height);
    const int resized_width = static_cast<int>(std::round(width * scale));
    const int resized_height = static_cast<int>(std::round(height * scale));
    const int padding_x = (kInputSize - resized_width) / 2;
    const int padding_y = (kInputSize - resized_height) / 2;
    input_host_.assign(3U * kInputSize * kInputSize, 114.0F / 255.0F);
    for (int y = 0; y < resized_height; ++y) {
      const int source_y = std::min(static_cast<int>(y / scale), height - 1);
      for (int x = 0; x < resized_width; ++x) {
        const int source_x = std::min(static_cast<int>(x / scale), width - 1);
        const std::size_t source = (static_cast<std::size_t>(source_y) * width + source_x) * 3U;
        const std::size_t destination = static_cast<std::size_t>(padding_y + y) * kInputSize + padding_x + x;
        input_host_[destination] = bgr[source + 2U] / 255.0F;
        input_host_[kInputSize * kInputSize + destination] = bgr[source + 1U] / 255.0F;
        input_host_[2U * kInputSize * kInputSize + destination] = bgr[source] / 255.0F;
      }
    }
    if (cudaMemcpyAsync(input_, input_host_.data(), input_host_.size() * sizeof(float), cudaMemcpyHostToDevice, stream_) != cudaSuccess) {
      error = trt_error("copy YOLO input to GPU"); return false;
    }
    const auto inference_begin = std::chrono::steady_clock::now();
    if (!context_->setInputTensorAddress(input_name_.c_str(), input_)) { error = "refresh TensorRT input failed"; return false; }
    for (const auto& output : outputs_) {
      if (!context_->setOutputTensorAddress(output.name.c_str(), output.data)) { error = "refresh TensorRT output failed"; return false; }
    }
    if (!context_->enqueueV3(stream_) ||
        cudaMemcpyAsync(output_host_.data(), output_, output_host_.size() * sizeof(float), cudaMemcpyDeviceToHost, stream_) != cudaSuccess ||
        cudaStreamSynchronize(stream_) != cudaSuccess) {
      error = trt_error("run TensorRT inference");
      return false;
    }
    const auto postprocess_begin = std::chrono::steady_clock::now();
    std::vector<YoloDetection> candidates;
    for (int prediction = 0; prediction < kPredictionCount; ++prediction) {
      int class_id = 0;
      float confidence = output_host_[4U * kPredictionCount + prediction];
      for (int class_index = 1; class_index < kClassCount; ++class_index) {
        const float score = output_host_[static_cast<std::size_t>(4 + class_index) * kPredictionCount + prediction];
        if (score > confidence) { confidence = score; class_id = class_index; }
      }
      if (confidence < kConfidenceThreshold) continue;
      const float center_x = output_host_[prediction];
      const float center_y = output_host_[kPredictionCount + prediction];
      const float box_width = output_host_[2U * kPredictionCount + prediction];
      const float box_height = output_host_[3U * kPredictionCount + prediction];
      YoloDetection detection{(center_x - box_width * 0.5F - padding_x) / scale,
                              (center_y - box_height * 0.5F - padding_y) / scale,
                              (center_x + box_width * 0.5F - padding_x) / scale,
                              (center_y + box_height * 0.5F - padding_y) / scale,
                              confidence, class_id};
      detection.left = std::clamp(detection.left, 0.0F, static_cast<float>(width));
      detection.top = std::clamp(detection.top, 0.0F, static_cast<float>(height));
      detection.right = std::clamp(detection.right, 0.0F, static_cast<float>(width));
      detection.bottom = std::clamp(detection.bottom, 0.0F, static_cast<float>(height));
      candidates.push_back(detection);
    }
    result.width = width;
    result.height = height;
    result.source_frame_number = frame_number;
    result.bgr.assign(bgr, bgr + static_cast<std::size_t>(width) * height * 3U);
    result.detections = non_maximum_suppression(std::move(candidates), kIouThreshold);
    for (const auto& detection : result.detections) draw_box(result.bgr, width, height, detection);
    const auto end = std::chrono::steady_clock::now();
    result.timings = {std::chrono::duration<double, std::milli>(inference_begin - preprocess_begin).count(),
                      std::chrono::duration<double, std::milli>(postprocess_begin - inference_begin).count(),
                      std::chrono::duration<double, std::milli>(end - postprocess_begin).count()};
    return true;
  }

 private:
  struct RuntimeDeleter { template <typename T> void operator()(T* value) const { if (value) delete value; } };
  bool build_engine(std::vector<char>& serialized, std::string& error) {
    std::unique_ptr<nvinfer1::IBuilder, RuntimeDeleter> builder(nvinfer1::createInferBuilder(logger_));
    if (!builder) { error = "create TensorRT builder failed"; return false; }
    // TensorRT 10 creates explicit-batch networks by default; the old flag is deprecated.
    std::unique_ptr<nvinfer1::INetworkDefinition, RuntimeDeleter> network(builder->createNetworkV2(0U));
    std::unique_ptr<nvinfer1::IBuilderConfig, RuntimeDeleter> config(builder->createBuilderConfig());
    std::unique_ptr<nvonnxparser::IParser, RuntimeDeleter> parser(nvonnxparser::createParser(*network, logger_));
    if (!network || !config || !parser) { error = "create TensorRT ONNX builder objects failed"; return false; }
    if (!parser->parseFromFile(onnx_path_.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
      error = "parse YOLOv8 ONNX failed";
      return false;
    }
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 512U * 1024U * 1024U);
    if (builder->platformHasFastFp16()) config->setFlag(nvinfer1::BuilderFlag::kFP16);
    std::unique_ptr<nvinfer1::IHostMemory, RuntimeDeleter> plan(builder->buildSerializedNetwork(*network, *config));
    if (!plan) { error = "build TensorRT YOLO engine failed"; return false; }
    serialized.assign(static_cast<const char*>(plan->data()), static_cast<const char*>(plan->data()) + plan->size());
    std::ofstream engine_file(engine_path_, std::ios::binary);
    if (!engine_file) { error = "write TensorRT engine failed"; return false; }
    engine_file.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    return engine_file.good();
  }

  std::string onnx_path_;
  std::string engine_path_;
  TensorRtLogger logger_;
  std::unique_ptr<nvinfer1::IRuntime, RuntimeDeleter> runtime_;
  std::unique_ptr<nvinfer1::ICudaEngine, RuntimeDeleter> engine_;
  std::unique_ptr<nvinfer1::IExecutionContext, RuntimeDeleter> context_;
  std::string input_name_;
  void* input_ = nullptr;
  void* output_ = nullptr;
  std::vector<OutputBinding> outputs_;
  cudaStream_t stream_ = nullptr;
  std::vector<float> input_host_;
  std::vector<float> output_host_ = std::vector<float>(84U * kPredictionCount);
  bool initialized_ = false;
};

YoloDetector::YoloDetector(std::string onnx_path, std::string engine_path)
    : impl_(std::make_unique<Impl>(std::move(onnx_path), std::move(engine_path))) {}
YoloDetector::~YoloDetector() = default;
bool YoloDetector::initialize(std::string& error) { return impl_->initialize(error); }
bool YoloDetector::infer(const std::uint8_t* bgr, int width, int height, std::uint64_t frame_number,
                         YoloFrame& result, std::string& error) {
  return impl_->infer(bgr, width, height, frame_number, result, error);
}

}  // namespace hdmi_test
