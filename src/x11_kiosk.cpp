#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "hdmi_test/system_metrics.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace {

std::atomic_bool keep_running{true};

void handle_signal(int) { keep_running = false; }

unsigned long rgb(unsigned char red, unsigned char green, unsigned char blue) {
  return (static_cast<unsigned long>(red) << 16U) |
         (static_cast<unsigned long>(green) << 8U) |
         static_cast<unsigned long>(blue);
}

std::string read_file(const char* path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct LiveMetrics {
  double cpu_percent = 0.0;
  double gpu_percent = 0.0;
  hdmi_test::MemoryUsage memory{};
};

class SystemMetricsSampler {
 public:
  LiveMetrics sample() {
    LiveMetrics metrics{};
    const auto current_cpu = hdmi_test::parse_cpu_times(read_file("/proc/stat"));
    if (current_cpu.has_value()) {
      if (previous_cpu_.has_value()) {
        metrics.cpu_percent = hdmi_test::compute_cpu_percent(*previous_cpu_, *current_cpu);
      }
      previous_cpu_ = current_cpu;
    }
    const auto memory = hdmi_test::parse_memory_usage(read_file("/proc/meminfo"));
    if (memory.has_value()) {
      metrics.memory = *memory;
    }
    const auto gpu = hdmi_test::parse_gpu_load_percent(
        read_file("/sys/devices/platform/bus@0/17000000.gpu/load"));
    if (gpu.has_value()) {
      metrics.gpu_percent = *gpu;
    }
    return metrics;
  }

 private:
  std::optional<hdmi_test::CpuTimes> previous_cpu_;
};

void draw(Display* display, Drawable target, GC graphics, int width, int height,
          std::uint64_t frame_number, double frames_per_second,
          std::chrono::seconds uptime, const LiveMetrics& metrics) {
  XSetForeground(display, graphics, rgb(11, 18, 32));
  XFillRectangle(display, target, graphics, 0, 0, static_cast<unsigned>(width),
                 static_cast<unsigned>(height));

  const int header_height = height / 6;
  XSetForeground(display, graphics, rgb(20, 32, 52));
  XFillRectangle(display, target, graphics, 0, 0, static_cast<unsigned>(width),
                 static_cast<unsigned>(header_height));

  XSetForeground(display, graphics, rgb(224, 231, 255));
  XDrawString(display, target, graphics, 32, header_height / 2 + 6,
              "HDMI DISPLAY TEST", 17);

  XSetForeground(display, graphics, rgb(31, 47, 72));
  for (int x = 0; x < width; x += 80) {
    XFillRectangle(display, target, graphics, x, header_height, 1,
                   static_cast<unsigned>(height - header_height));
  }
  for (int y = header_height; y < height; y += 80) {
    XFillRectangle(display, target, graphics, 0, y, static_cast<unsigned>(width), 1);
  }

  const int card_top = header_height + 64;
  const int card_height = height / 3;
  const int card_width = (width - 160) / 3;
  const unsigned long colors[] = {rgb(30, 200, 150), rgb(239, 83, 80),
                                  rgb(255, 183, 77)};
  const char* labels[] = {"DISPLAY LINK", "FRAME RATE", "FRAME COUNT"};
  char values[3][48]{};
  std::snprintf(values[0], sizeof(values[0]), "59.97 HZ OUTPUT");
  std::snprintf(values[1], sizeof(values[1]), "%.1f / 60 FPS", frames_per_second);
  std::snprintf(values[2], sizeof(values[2]), "%llu", static_cast<unsigned long long>(frame_number));
  for (int index = 0; index < 3; ++index) {
    const int left = 40 + index * (card_width + 40);
    XSetForeground(display, graphics, colors[index]);
    XFillRectangle(display, target, graphics, left, card_top,
                   static_cast<unsigned>(card_width),
                   static_cast<unsigned>(card_height));
    XSetForeground(display, graphics, rgb(11, 18, 32));
    XDrawString(display, target, graphics, left + 18, card_top + 32,
                labels[index], static_cast<int>(std::strlen(labels[index])));
    XDrawString(display, target, graphics, left + 18, card_top + 58,
                values[index], static_cast<int>(std::strlen(values[index])));
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
  localtime_r(&seconds, &local_time);
  char time_label[48]{};
  std::strftime(time_label, sizeof(time_label), "RUNNING  %Y-%m-%d  %H:%M:%S",
                &local_time);
  XSetForeground(display, graphics, rgb(224, 231, 255));
  XDrawString(display, target, graphics, 32, height - 54, time_label,
              static_cast<int>(std::strlen(time_label)));
  char debug_label[160]{};
  char host_name[64]{};
  gethostname(host_name, sizeof(host_name) - 1);
  const char* display_name = std::getenv("DISPLAY");
  std::snprintf(debug_label, sizeof(debug_label),
                "CPU %.1f%% | GPU %.1f%% | RAM %llu/%llu MiB | HOST %s | DISPLAY %s | %dx%d | DOUBLE BUFFER | UPTIME %llds",
                metrics.cpu_percent, metrics.gpu_percent,
                static_cast<unsigned long long>(metrics.memory.used_kib / 1024),
                static_cast<unsigned long long>(metrics.memory.total_kib / 1024),
                host_name, display_name == nullptr ? "unknown" : display_name, width,
                height, static_cast<long long>(uptime.count()));
  XDrawString(display, target, graphics, 32, height - 28, debug_label,
              static_cast<int>(std::strlen(debug_label)));
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1 && (std::strcmp(argv[1], "--help") == 0 ||
                   std::strcmp(argv[1], "-h") == 0)) {
    std::cout << "Usage: hdmi_x11_kiosk\n"
                 "Starts a fullscreen C++ test screen on the active X display.\n";
    return 0;
  }

  Display* display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    std::cerr << "Cannot open X display. Set DISPLAY and XAUTHORITY.\n";
    return 1;
  }
  const int screen = DefaultScreen(display);
  const int width = DisplayWidth(display, screen);
  const int height = DisplayHeight(display, screen);
  const Window root = RootWindow(display, screen);

  XSetWindowAttributes attributes{};
  attributes.event_mask = ExposureMask | StructureNotifyMask;
  const Window window = XCreateWindow(
      display, root, 0, 0, static_cast<unsigned>(width), static_cast<unsigned>(height),
      0, CopyFromParent, InputOutput, CopyFromParent,
      CWEventMask, &attributes);
  XStoreName(display, window, "HDMI Display Test");
  Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  const Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
  XMapWindow(display, window);
  XEvent fullscreen_event{};
  fullscreen_event.xclient.type = ClientMessage;
  fullscreen_event.xclient.window = window;
  fullscreen_event.xclient.message_type = wm_state;
  fullscreen_event.xclient.format = 32;
  fullscreen_event.xclient.data.l[0] = 1;
  fullscreen_event.xclient.data.l[1] = static_cast<long>(fullscreen);
  XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask,
             &fullscreen_event);
  XRaiseWindow(display, window);
  const GC graphics = XCreateGC(display, window, 0, nullptr);
  const Pixmap back_buffer = XCreatePixmap(display, window, static_cast<unsigned>(width),
                                           static_cast<unsigned>(height),
                                           static_cast<unsigned>(DefaultDepth(display, screen)));
  if (back_buffer == 0) {
    std::cerr << "Cannot create X11 back buffer.\n";
    XFreeGC(display, graphics);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 1;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  const auto start_time = std::chrono::steady_clock::now();
  auto sample_start = start_time;
  auto next_frame = start_time;
  std::uint64_t frame_number = 0;
  std::uint64_t sampled_frames = 0;
  double frames_per_second = 0.0;
  SystemMetricsSampler metrics_sampler;
  LiveMetrics metrics = metrics_sampler.sample();
  auto next_metrics_sample = start_time + std::chrono::seconds(1);
  while (keep_running) {
    while (XPending(display) > 0) {
      XEvent event{};
      XNextEvent(display, &event);
    }
    const auto now = std::chrono::steady_clock::now();
    const auto sample_elapsed = now - sample_start;
    if (sample_elapsed >= std::chrono::seconds(1)) {
      frames_per_second = static_cast<double>(sampled_frames) /
                          std::chrono::duration<double>(sample_elapsed).count();
      sample_start = now;
      sampled_frames = 0;
    }
    if (now >= next_metrics_sample) {
      metrics = metrics_sampler.sample();
      next_metrics_sample = now + std::chrono::seconds(1);
    }
    draw(display, back_buffer, graphics, width, height, frame_number, frames_per_second,
         std::chrono::duration_cast<std::chrono::seconds>(now - start_time), metrics);
    XCopyArea(display, back_buffer, window, graphics, 0, 0, static_cast<unsigned>(width),
              static_cast<unsigned>(height), 0, 0);
    XFlush(display);
    ++frame_number;
    ++sampled_frames;
    next_frame += std::chrono::nanoseconds(16'666'667);
    if (next_frame < now) {
      next_frame = now;
    }
    std::this_thread::sleep_until(next_frame);
  }

  XFreePixmap(display, back_buffer);
  XFreeGC(display, graphics);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  return 0;
}
