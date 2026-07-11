#pragma once

#include "hdmi_test/test_pattern.hpp"

#include <cstdint>
#include <string>

namespace hdmi_test {

class KmsDisplay {
 public:
  explicit KmsDisplay(std::string device_path);
  ~KmsDisplay();

  KmsDisplay(const KmsDisplay&) = delete;
  KmsDisplay& operator=(const KmsDisplay&) = delete;

  void initialize();
  [[nodiscard]] FrameBufferView frame_buffer() const;
  [[nodiscard]] std::string mode_label() const;

 private:
  void release() noexcept;

  std::string device_path_;
  int fd_ = -1;
  std::uint32_t connector_id_ = 0;
  std::uint32_t crtc_id_ = 0;
  std::uint32_t framebuffer_id_ = 0;
  std::uint32_t dumb_handle_ = 0;
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  std::uint32_t pitch_ = 0;
  std::uint64_t map_size_ = 0;
  void* mapped_pixels_ = nullptr;
  void* original_crtc_ = nullptr;
};

std::string find_drm_device();
std::string describe_drm_devices();

}  // namespace hdmi_test
