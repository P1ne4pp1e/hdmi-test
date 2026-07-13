#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hdmi_test {

struct YoloDetection {
  float left = 0.0F;
  float top = 0.0F;
  float right = 0.0F;
  float bottom = 0.0F;
  float confidence = 0.0F;
  int class_id = 0;
};

struct YoloTimings {
  double preprocess_ms = 0.0;
  double inference_ms = 0.0;
  double postprocess_ms = 0.0;
};

struct YoloFrame {
  int width = 0;
  int height = 0;
  std::uint64_t source_frame_number = 0;
  std::vector<std::uint8_t> bgr;
  std::vector<YoloDetection> detections;
  YoloTimings timings;
};

std::vector<YoloDetection> non_maximum_suppression(std::vector<YoloDetection> detections,
                                                    float iou_threshold);

class YoloDetector {
 public:
  YoloDetector(std::string onnx_path, std::string engine_path);
  ~YoloDetector();
  YoloDetector(const YoloDetector&) = delete;
  YoloDetector& operator=(const YoloDetector&) = delete;

  bool initialize(std::string& error);
  bool infer(const std::uint8_t* bgr, int width, int height, std::uint64_t frame_number,
             YoloFrame& result, std::string& error);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hdmi_test
