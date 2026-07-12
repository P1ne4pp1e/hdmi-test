#include "hdmi_test/hik_camera.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main() {
  std::vector<hdmi_test::HikCameraInfo> devices;
  std::string error;
  if (!hdmi_test::HikCamera::enumerate(devices, error)) {
    std::fprintf(stderr, "HIKROBOT SDK enumeration error: %s\n", error.c_str());
    return 2;
  }
  if (devices.empty()) {
    std::fprintf(stderr, "No HIKROBOT USB camera detected. Check USB 3.0 cable and camera power.\n");
    return 1;
  }
  for (std::size_t index = 0; index < devices.size(); ++index) {
    const auto& device = devices[index];
    std::printf("[%zu] model=%s serial=%s user=%s\n", index, device.model.c_str(),
                device.serial_number.c_str(), device.user_name.c_str());
  }
  hdmi_test::HikCamera camera;
  if (!camera.open(error)) {
    std::fprintf(stderr, "Open camera failed: %s\n", error.c_str());
    return 3;
  }
  hdmi_test::HikCameraFrame frame;
  if (!camera.grab(frame, error)) {
    std::fprintf(stderr, "Capture failed: %s\n", error.c_str());
    return 4;
  }
  std::printf("Captured frame=%llu %dx%d BGR bytes=%zu\n",
              static_cast<unsigned long long>(frame.frame_number), frame.width, frame.height, frame.bgr.size());
  return 0;
}
