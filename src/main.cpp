#include "hdmi_test/kms_display.hpp"
#include "hdmi_test/test_pattern.hpp"

#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

#include <unistd.h>

namespace {

std::atomic_bool keep_running{true};

void handle_signal(int) { keep_running = false; }

void print_usage(std::ostream& output) {
  output << "Usage: hdmi_test [--device /dev/dri/cardN]\n"
         << "       hdmi_test --probe\n"
         << "Direct DRM/KMS display test screen. Press Ctrl-C to restore the "
            "previous display state and exit.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string device_path;
  bool probe_only = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--help" || argument == "-h") {
      print_usage(std::cout);
      return 0;
    }
    if (argument == "--probe") {
      probe_only = true;
      continue;
    }
    if (argument == "--device" && index + 1 < argc) {
      device_path = argv[++index];
      continue;
    }
    std::cerr << "Unknown or incomplete argument: " << argument << '\n';
    print_usage(std::cerr);
    return 2;
  }

  if (probe_only) {
    if (!device_path.empty()) {
      std::cerr << "--probe cannot be combined with --device.\n";
      return 2;
    }
    std::cout << hdmi_test::describe_drm_devices();
    return 0;
  }

  if (device_path.empty()) {
    device_path = hdmi_test::find_drm_device();
  }
  if (device_path.empty()) {
    std::cerr << "No DRM device found. Expected /dev/dri/card*. "
                 "Ensure the GPU/display driver is loaded and this user can access it.\n";
    return 1;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  try {
    hdmi_test::KmsDisplay display(device_path);
    display.initialize();
    hdmi_test::draw_test_pattern(display.frame_buffer());
    std::cout << "Displaying DRM/KMS test pattern on " << device_path << " at "
              << display.mode_label() << ". Press Ctrl-C to exit.\n";
    while (keep_running) {
      pause();
    }
  } catch (const std::exception& error) {
    std::cerr << "Failed to display test pattern: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
