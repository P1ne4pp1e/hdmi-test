#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hdmi_test {

struct HikCameraInfo {
  std::string model;
  std::string serial_number;
  std::string user_name;
};

struct HikCameraConfig {
  int device_index = 0;
  double exposure_time_us = 10000.0;
  double gain_db = 15.0;
  bool flip_horizontal = false;
  bool flip_vertical = false;
};

struct HikCameraFrame {
  int width = 0;
  int height = 0;
  std::uint64_t frame_number = 0;
  std::vector<std::uint8_t> bgr;
};

// Adapted from pip-vision-2027's HikCamera backend for an SDK-only probe.
// Frames own their BGR data so an SDK buffer is never retained after release.
class HikCamera {
 public:
  explicit HikCamera(HikCameraConfig config = {});
  ~HikCamera();
  HikCamera(const HikCamera&) = delete;
  HikCamera& operator=(const HikCamera&) = delete;

  static bool enumerate(std::vector<HikCameraInfo>& devices, std::string& error);
  bool open(std::string& error);
  bool grab(HikCameraFrame& frame, std::string& error);
  void close();
  bool is_open() const;

 private:
  bool apply_configuration(std::string& error);

  HikCameraConfig config_;
  void* handle_ = nullptr;
  bool sdk_initialized_ = false;
};

}  // namespace hdmi_test
