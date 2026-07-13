#include "xdg-shell-client-protocol.h"
#include "hdmi_test/font_renderer.hpp"
#include "hdmi_test/hik_camera.hpp"
#include "hdmi_test/latency_stats.hpp"
#include "hdmi_test/system_metrics.hpp"
#include "hdmi_test/yolo_detector.hpp"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <mutex>
#include <thread>
#include <vector>

namespace {
constexpr int kWidth = 800;
constexpr int kHeight = 480;

std::string read_file(const char* path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::uint64_t steady_time_ns() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

class CameraCapture {
 public:
  CameraCapture() : worker_([this](std::stop_token stop) { run(stop); }) {}
  ~CameraCapture() = default;

  std::shared_ptr<const hdmi_test::HikCameraFrame> latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
  }
  double fps() const { return fps_.load(); }
  std::string status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
  }

 private:
  void run(std::stop_token stop) {
    hdmi_test::HikCamera camera;
    auto last_sample = std::chrono::steady_clock::now();
    std::uint64_t count = 0;
    while (!stop.stop_requested()) {
      std::string error;
      if (!camera.is_open() && !camera.open(error)) {
        { std::lock_guard<std::mutex> lock(mutex_); status_ = "CAMERA OFFLINE"; }
        for (int i = 0; i < 20 && !stop.stop_requested(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      hdmi_test::HikCameraFrame frame;
      if (!camera.grab(frame, error)) {
        { std::lock_guard<std::mutex> lock(mutex_); status_ = "CAPTURE RETRY"; }
        camera.close();
        continue;
      }
      auto shared_frame = std::make_shared<hdmi_test::HikCameraFrame>(std::move(frame));
      {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_ = std::move(shared_frame);
        status_ = "HIKROBOT USB ONLINE";
      }
      ++count;
      const auto now = std::chrono::steady_clock::now();
      if (now - last_sample >= std::chrono::seconds(1)) {
        fps_.store(count / std::chrono::duration<double>(now - last_sample).count());
        count = 0;
        last_sample = now;
      }
    }
    camera.close();
  }

  mutable std::mutex mutex_;
  std::shared_ptr<const hdmi_test::HikCameraFrame> latest_;
  std::string status_ = "CAMERA STARTING";
  std::atomic<double> fps_{0.0};
  std::jthread worker_;
};

class YoloPipeline {
 public:
  explicit YoloPipeline(const CameraCapture& camera) : camera_(camera), worker_([this](std::stop_token stop) { run(stop); }) {}

  std::shared_ptr<const hdmi_test::YoloFrame> latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
  }
  double fps() const { return fps_.load(); }
  hdmi_test::YoloTimings timings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timings_;
  }
  std::string status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
  }

 private:
  void run(std::stop_token stop) {
    hdmi_test::YoloDetector detector("/home/p1ne4pp1e/screen_test/models/yolov8n.onnx",
                                     "/home/p1ne4pp1e/screen_test/models/yolov8n_fp16.engine");
    std::string error;
    if (!detector.initialize(error)) {
      std::lock_guard<std::mutex> lock(mutex_);
      status_ = "YOLO ENGINE ERROR";
      std::fprintf(stderr, "YOLO initialization: %s\n", error.c_str());
      return;
    }
    { std::lock_guard<std::mutex> lock(mutex_); status_ = "YOLOV8N TENSORRT ONLINE"; }
    auto last_sample = std::chrono::steady_clock::now();
    std::uint64_t count = 0;
    while (!stop.stop_requested()) {
      const auto input = camera_.latest();
      if (!input) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      hdmi_test::YoloFrame result;
      if (!detector.infer(input->bgr.data(), input->width, input->height, input->frame_number, result, error)) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = "YOLO INFERENCE ERROR";
        std::fprintf(stderr, "YOLO inference: %s\n", error.c_str());
        return;
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        result.capture_time_ns = input->capture_time_ns;
        timings_ = result.timings;
        latest_ = std::make_shared<hdmi_test::YoloFrame>(std::move(result));
        status_ = "YOLOV8N TENSORRT THROUGHPUT";
      }
      ++count;
      const auto now = std::chrono::steady_clock::now();
      if (now - last_sample >= std::chrono::seconds(1)) {
        fps_.store(count / std::chrono::duration<double>(now - last_sample).count());
        count = 0;
        last_sample = now;
      }
    }
  }

  const CameraCapture& camera_;
  mutable std::mutex mutex_;
  std::shared_ptr<const hdmi_test::YoloFrame> latest_;
  hdmi_test::YoloTimings timings_{};
  std::string status_ = "YOLO ENGINE BUILDING";
  std::atomic<double> fps_{0.0};
  std::jthread worker_;
};

const char* coco_label(int class_id) {
  static constexpr const char* kLabels[] = {
      "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
      "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
      "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
      "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
      "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli",
      "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet",
      "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator",
      "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush",
  };
  return class_id >= 0 && class_id < static_cast<int>(std::size(kLabels)) ? kLabels[class_id] : "object";
}

void draw_detection_labels(std::vector<std::uint32_t>& pixels, hdmi_test::FontRenderer& font,
                           const hdmi_test::YoloFrame* yolo_frame) {
  if (!yolo_frame || yolo_frame->width <= 0 || yolo_frame->height <= 0) return;
  constexpr int kPanelX = 424, kPanelY = 96, kPanelWidth = 352, kPanelHeight = 264;
  constexpr std::uint32_t kWhite = 0xF3F7FCU, kCyan = 0x24D6D0U;
  const std::size_t count = std::min<std::size_t>(yolo_frame->detections.size(), 6U);
  for (std::size_t index = 0; index < count; ++index) {
    const auto& detection = yolo_frame->detections[index];
    const int x = std::clamp(kPanelX + static_cast<int>(detection.left * kPanelWidth / yolo_frame->width),
                             kPanelX + 4, kPanelX + kPanelWidth - 110);
    const int y = std::clamp(kPanelY + static_cast<int>(detection.top * kPanelHeight / yolo_frame->height) - 4,
                             kPanelY + 16, kPanelY + kPanelHeight - 4);
    char text[48]{};
    std::snprintf(text, sizeof(text), "%s %.0f%%", coco_label(detection.class_id), detection.confidence * 100.0F);
    // A one-pixel dark shadow stays transparent outside glyphs, preserving
    // the video while making labels readable on bright industrial scenes.
    font.draw(pixels.data(), kWidth, kHeight, x + 1, y + 1, text, 13, 0x0B1420U);
    font.draw(pixels.data(), kWidth, kHeight, x, y, text, 13, index == 0 ? kWhite : kCyan);
  }
}

void draw_hud(std::vector<std::uint32_t>& pixels, hdmi_test::FontRenderer& font,
              double fps, double cpu, double gpu, hdmi_test::MemoryUsage memory,
              std::uint64_t frame, double camera_fps, const std::string& camera_status,
              const hdmi_test::HikCameraFrame* camera_frame, double yolo_fps,
              const hdmi_test::YoloTimings& yolo_timings, const std::string& yolo_status,
              const hdmi_test::YoloFrame* yolo_frame, double latency_p50,
              double latency_p95, double latency_p99, std::uint64_t uptime_seconds) {
  std::fill(pixels.begin(), pixels.end(), 0U);
  constexpr std::uint32_t kWhite = 0xF3F7FCU, kMuted = 0x91A5BCU, kCyan = 0x24D6D0U,
                          kAmber = 0xFFB454U;
  const auto draw_right = [&font, &pixels](int right, int baseline, const char* value,
                                           int pixel_size, std::uint32_t color) {
    font.draw(pixels.data(), kWidth, kHeight, right - font.text_width(value, pixel_size),
              baseline, value, pixel_size, color);
  };
  font.draw(pixels.data(), kWidth, kHeight, 24, 34, "VISION DISPLAY", 20, kWhite);
  font.draw(pixels.data(), kWidth, kHeight, 24, 56, "NVIDIA DRM  /  ASYNC CAMERA PIPELINE", 13, kMuted);
  draw_right(764, 38, "LIVE  60 Hz", 15, kCyan);
  char text[80]{};
  font.draw(pixels.data(), kWidth, kHeight, 32, 92, camera_status.c_str(), 13,
            camera_frame ? kCyan : kAmber);
  if (camera_frame) {
    std::snprintf(text, sizeof(text), "%dx%d  %.1f FPS", camera_frame->width, camera_frame->height, camera_fps);
    font.draw(pixels.data(), kWidth, kHeight, 32, 112, text, 13, kWhite);
  } else {
    const char* offline = "NO CAMERA SIGNAL";
    font.draw(pixels.data(), kWidth, kHeight, 200 - font.text_width(offline, 22) / 2, 222, offline, 22, kWhite);
    const char* hint = "USB capture thread is waiting for a device";
    font.draw(pixels.data(), kWidth, kHeight, 200 - font.text_width(hint, 13) / 2, 248, hint, 13, kMuted);
  }
  font.draw(pixels.data(), kWidth, kHeight, 432, 92, "YOLOV8N  /  TENSORRT", 13, kWhite);
  font.draw(pixels.data(), kWidth, kHeight, 432, 112, yolo_status.c_str(), 12,
            yolo_frame ? kCyan : kAmber);
  std::snprintf(text, sizeof(text), "%.1f FPS  |  PRE %.1f  INFER %.1f  POST %.1f ms", yolo_fps,
                yolo_timings.preprocess_ms, yolo_timings.inference_ms, yolo_timings.postprocess_ms);
  font.draw(pixels.data(), kWidth, kHeight, 432, 136, text, 12, kMuted);
  std::snprintf(text, sizeof(text), "E2E  P50 %.0f  P95 %.0f  P99 %.0f ms", latency_p50, latency_p95, latency_p99);
  font.draw(pixels.data(), kWidth, kHeight, 432, 158, text, 12, kWhite);
  const auto hours = uptime_seconds / 3600U;
  const auto minutes = (uptime_seconds / 60U) % 60U;
  const auto seconds = uptime_seconds % 60U;
  std::snprintf(text, sizeof(text), "DET %zu  |  UP %02llu:%02llu:%02llu",
                yolo_frame ? yolo_frame->detections.size() : 0U,
                static_cast<unsigned long long>(hours), static_cast<unsigned long long>(minutes),
                static_cast<unsigned long long>(seconds));
  font.draw(pixels.data(), kWidth, kHeight, 432, 180, text, 12, kMuted);
  draw_detection_labels(pixels, font, yolo_frame);
  const int metric_x[] = {24, 176, 326, 476, 626};
  font.draw(pixels.data(), kWidth, kHeight, metric_x[0], 402, "DISPLAY FPS", 12, kMuted);
  std::snprintf(text, sizeof(text), "%.1f", fps);
  font.draw(pixels.data(), kWidth, kHeight, metric_x[0], 434, text, 24, kWhite);
  const char* labels[] = {"CPU LOAD", "GPU LOAD", "MEMORY"};
  const double memory_used_gib = static_cast<double>(memory.used_kib) / (1024.0 * 1024.0);
  const double memory_total_gib = static_cast<double>(memory.total_kib) / (1024.0 * 1024.0);
  for (int index = 0; index < 3; ++index) {
    font.draw(pixels.data(), kWidth, kHeight, metric_x[index + 1], 402, labels[index], 12, kMuted);
    if (index == 0) std::snprintf(text, sizeof(text), "%.1f%%", cpu);
    if (index == 1) std::snprintf(text, sizeof(text), "%.1f%%", gpu);
    if (index == 2) std::snprintf(text, sizeof(text), "%.1f / %.1f GB", memory_used_gib, memory_total_gib);
    font.draw(pixels.data(), kWidth, kHeight, metric_x[index + 1], 434, text, index == 2 ? 15 : 22, kWhite);
  }
  font.draw(pixels.data(), kWidth, kHeight, metric_x[4], 402, "CAMERA FPS", 12, kMuted);
  std::snprintf(text, sizeof(text), "%.1f", camera_fps);
  font.draw(pixels.data(), kWidth, kHeight, metric_x[4], 434, text, 22, kWhite);
  std::snprintf(text, sizeof(text), "FRAME  %llu", static_cast<unsigned long long>(frame));
  font.draw(pixels.data(), kWidth, kHeight, 24, 466, text, 12, kMuted);
  draw_right(764, 466, "ASYNC CAPTURE  /  LATEST FRAME", 12, kMuted);
}

struct App {
  wl_display* display = nullptr;
  wl_compositor* compositor = nullptr;
  xdg_wm_base* wm_base = nullptr;
  wl_surface* surface = nullptr;
  xdg_surface* xdg_surface_handle = nullptr;
  xdg_toplevel* toplevel = nullptr;
  wl_egl_window* egl_window = nullptr;
  EGLDisplay egl_display = EGL_NO_DISPLAY;
  EGLSurface egl_surface = EGL_NO_SURFACE;
  EGLContext egl_context = EGL_NO_CONTEXT;
  wl_callback* frame_callback = nullptr;
  bool frame_presented = true;
};

void registry_global(void* data, wl_registry* registry, std::uint32_t name,
                     const char* interface, std::uint32_t version) {
  auto& app = *static_cast<App*>(data);
  if (std::strcmp(interface, "wl_compositor") == 0) {
    app.compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface,
                         std::min(version, static_cast<std::uint32_t>(4))));
  } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
    app.wm_base = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
  }
}
void registry_remove(void*, wl_registry*, std::uint32_t) {}
const wl_registry_listener kRegistryListener{registry_global, registry_remove};

void wm_ping(void*, xdg_wm_base* base, std::uint32_t serial) {
  xdg_wm_base_pong(base, serial);
}
const xdg_wm_base_listener kWmListener{wm_ping};
void surface_configure(void*, xdg_surface* surface, std::uint32_t serial) {
  xdg_surface_ack_configure(surface, serial);
}
const xdg_surface_listener kSurfaceListener{surface_configure};
void top_configure(void*, xdg_toplevel*, int, int, wl_array*) {}
void top_close(void*, xdg_toplevel*) {}
void top_bounds(void*, xdg_toplevel*, int, int) {}
const xdg_toplevel_listener kTopListener{top_configure, top_close, top_bounds};

void frame_done(void* data, wl_callback* callback, std::uint32_t) {
  auto& app = *static_cast<App*>(data);
  wl_callback_destroy(callback);
  app.frame_callback = nullptr;
  app.frame_presented = true;
}
const wl_callback_listener kFrameListener{frame_done};

bool wait_for_presented_frame(App& app) {
  app.frame_callback = wl_surface_frame(app.surface);
  if (!app.frame_callback) return false;
  app.frame_presented = false;
  wl_callback_add_listener(app.frame_callback, &kFrameListener, &app);
  return true;
}

GLuint compile_shader(GLenum type, const char* source) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    char log[512]{};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::fprintf(stderr, "shader compilation failed: %s\n", log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint create_program() {
  constexpr const char* vertex = R"(
    attribute vec2 position;
    void main() { gl_Position = vec4(position, 0.0, 1.0); }
  )";
  constexpr const char* fragment = R"(
    precision mediump float;
    uniform vec2 resolution;
    uniform float time;
    uniform sampler2D hud;
    uniform sampler2D camera;
    uniform vec2 cameraSize;
    uniform vec4 detectionBoxes[8];
    uniform int detectionCount;

    float rectangle(vec2 point, vec4 bounds) {
      vec2 lower = step(bounds.xy, point);
      vec2 upper = step(point, bounds.xy + bounds.zw);
      return lower.x * lower.y * upper.x * upper.y;
    }

    float roundedPanel(vec2 point, vec4 bounds, float radius) {
      vec2 center = bounds.xy + bounds.zw * 0.5;
      vec2 halfSize = bounds.zw * 0.5;
      vec2 distance = abs(point - center) - halfSize + radius;
      float signedDistance = length(max(distance, 0.0)) + min(max(distance.x, distance.y), 0.0) - radius;
      return 1.0 - smoothstep(0.0, 0.004, signedDistance);
    }

    void main() {
      vec2 uv = gl_FragCoord.xy / resolution;
      vec2 hudUv = vec2(uv.x, 1.0 - uv.y);
      // Use only continuous, low-frequency functions for the motion test.
      // Fine repeating `fract` grids alias on a 800×480 panel and can look as
      // though frames are missing even when presentation is stable.
      float wave = 0.5 + 0.5 * sin(uv.x * 8.0 + uv.y * 4.0 + time * 1.5707963);
      float diagonal = 0.5 + 0.5 * sin(uv.x * 5.0 - uv.y * 7.0 - time * 3.1415926);
      // A tiled sweep keeps constant velocity.  At the panel edges the next
      // sweep enters immediately, so there is no slow-down or turn-around.
      float scanPhase = fract(uv.x - time * 0.25);
      float scanDistance = min(scanPhase, 1.0 - scanPhase);
      float scan = 1.0 - smoothstep(0.0, 0.022, scanDistance);
      vec3 base = mix(vec3(0.02, 0.07, 0.13), vec3(0.03, 0.22, 0.36), wave);
      base += diagonal * vec3(0.01, 0.07, 0.11);
      base += scan * vec3(0.05, 0.65, 0.65);

      // The CPU texture deliberately contains glyph pixels only.  Additive
      // composition keeps its zero-valued background transparent and lets the
      // GPU own the dynamic test canvas on every frame.
      if (cameraSize.x > 0.0 && cameraSize.y > 0.0) {
        float cameraPanel = rectangle(hudUv, vec4(0.030, 0.200, 0.440, 0.550));
        vec2 local = (hudUv - vec2(0.030, 0.200)) / vec2(0.440, 0.550);
        float panelAspect = (0.440 * resolution.x) / (0.550 * resolution.y);
        float imageAspect = cameraSize.x / cameraSize.y;
        vec2 sampleUv = local;
        float visible = 1.0;
        if (imageAspect > panelAspect) {
          float contentHeight = panelAspect / imageAspect;
          visible = step(abs(local.y - 0.5), contentHeight * 0.5);
          sampleUv.y = (local.y - 0.5) / contentHeight + 0.5;
        } else {
          float contentWidth = imageAspect / panelAspect;
          visible = step(abs(local.x - 0.5), contentWidth * 0.5);
          sampleUv.x = (local.x - 0.5) / contentWidth + 0.5;
        }
        vec3 image = texture2D(camera, vec2(sampleUv.x, 1.0 - sampleUv.y)).bgr;
        base = mix(base, image, visible * cameraPanel);
        float cameraBorder = cameraPanel - rectangle(hudUv, vec4(0.033, 0.203, 0.434, 0.544));
        base += cameraBorder * vec3(0.12, 0.40, 0.48);
      }
      if (cameraSize.x > 0.0 && cameraSize.y > 0.0) {
        float overlayPanel = rectangle(hudUv, vec4(0.530, 0.200, 0.440, 0.550));
        vec2 local = (hudUv - vec2(0.530, 0.200)) / vec2(0.440, 0.550);
        float panelAspect = (0.440 * resolution.x) / (0.550 * resolution.y);
        float imageAspect = cameraSize.x / cameraSize.y;
        vec2 sampleUv = local;
        float visible = 1.0;
        if (imageAspect > panelAspect) {
          float contentHeight = panelAspect / imageAspect;
          visible = step(abs(local.y - 0.5), contentHeight * 0.5);
          sampleUv.y = (local.y - 0.5) / contentHeight + 0.5;
        } else {
          float contentWidth = imageAspect / panelAspect;
          visible = step(abs(local.x - 0.5), contentWidth * 0.5);
          sampleUv.x = (local.x - 0.5) / contentWidth + 0.5;
        }
        vec3 image = texture2D(camera, vec2(sampleUv.x, 1.0 - sampleUv.y)).bgr;
        base = mix(base, image, visible * overlayPanel);
        float detectionEdge = 0.0;
        for (int index = 0; index < 8; ++index) {
          if (index >= detectionCount) break;
          vec4 box = detectionBoxes[index];
          float horizontal = step(box.x, sampleUv.x) * step(sampleUv.x, box.z) *
                             (1.0 - smoothstep(0.0, 0.008, min(abs(sampleUv.y - box.y), abs(sampleUv.y - box.w))));
          float vertical = step(box.y, sampleUv.y) * step(sampleUv.y, box.w) *
                           (1.0 - smoothstep(0.0, 0.008, min(abs(sampleUv.x - box.x), abs(sampleUv.x - box.z))));
          detectionEdge = max(detectionEdge, max(horizontal, vertical));
        }
        base = mix(base, vec3(1.0, 0.86, 0.16), detectionEdge * visible * overlayPanel);
        float overlayBorder = overlayPanel - rectangle(hudUv, vec4(0.533, 0.203, 0.434, 0.544));
        base += overlayBorder * vec3(0.12, 0.40, 0.48);
      }
      // Original background structure: header, preview field, and bottom
      // metric strip. HUD coordinates are aligned to these fixed dividers.
      float outer = rectangle(hudUv, vec4(0.012, 0.012, 0.976, 0.976)) -
                    rectangle(hudUv, vec4(0.015, 0.015, 0.970, 0.970));
      float topLine = rectangle(hudUv, vec4(0.025, 0.145, 0.950, 0.003));
      float bottomLine = rectangle(hudUv, vec4(0.025, 0.770, 0.950, 0.003));
      float metricLines = rectangle(hudUv, vec4(0.198, 0.800, 0.002, 0.120)) +
                          rectangle(hudUv, vec4(0.385, 0.800, 0.002, 0.120)) +
                          rectangle(hudUv, vec4(0.572, 0.800, 0.002, 0.120)) +
                          rectangle(hudUv, vec4(0.760, 0.800, 0.002, 0.120));
      base += (outer + topLine + bottomLine + metricLines) * vec3(0.12, 0.40, 0.48);
      base += texture2D(hud, hudUv).rgb;
      gl_FragColor = vec4(min(base, vec3(1.0)), 1.0);
    }
  )";
  const GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex);
  const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment);
  if (vs == 0 || fs == 0) return 0;
  const GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glBindAttribLocation(program, 0, "position");
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint success = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (success == GL_FALSE) {
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

bool initialize_egl(App& app) {
  app.egl_window = wl_egl_window_create(app.surface, kWidth, kHeight);
  app.egl_display = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(app.display));
  if (app.egl_display == EGL_NO_DISPLAY || !eglInitialize(app.egl_display, nullptr, nullptr)) return false;
  if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;
  const EGLint config_attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
                                      EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                                      EGL_BLUE_SIZE, 8, EGL_NONE};
  EGLConfig config{};
  EGLint count = 0;
  if (!eglChooseConfig(app.egl_display, config_attributes, &config, 1, &count) || count == 0) return false;
  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  app.egl_context = eglCreateContext(app.egl_display, config, EGL_NO_CONTEXT, context_attributes);
  app.egl_surface = eglCreateWindowSurface(app.egl_display, config,
                                            reinterpret_cast<EGLNativeWindowType>(app.egl_window), nullptr);
  if (app.egl_context == EGL_NO_CONTEXT || app.egl_surface == EGL_NO_SURFACE) return false;
  if (!eglMakeCurrent(app.egl_display, app.egl_surface, app.egl_surface, app.egl_context)) return false;
  return eglSwapInterval(app.egl_display, 1) == EGL_TRUE;
}

}  // namespace

int main() {
  App app{};
  app.display = wl_display_connect(nullptr);
  if (!app.display) return 1;
  wl_registry* registry = wl_display_get_registry(app.display);
  wl_registry_add_listener(registry, &kRegistryListener, &app);
  wl_display_roundtrip(app.display);
  if (!app.compositor || !app.wm_base) return 2;
  xdg_wm_base_add_listener(app.wm_base, &kWmListener, &app);
  app.surface = wl_compositor_create_surface(app.compositor);
  app.xdg_surface_handle = xdg_wm_base_get_xdg_surface(app.wm_base, app.surface);
  xdg_surface_add_listener(app.xdg_surface_handle, &kSurfaceListener, &app);
  app.toplevel = xdg_surface_get_toplevel(app.xdg_surface_handle);
  xdg_toplevel_add_listener(app.toplevel, &kTopListener, &app);
  xdg_toplevel_set_fullscreen(app.toplevel, nullptr);
  wl_surface_commit(app.surface);
  wl_display_roundtrip(app.display);
  if (!initialize_egl(app)) return 3;
  const GLuint program = create_program();
  if (program == 0) return 4;
  constexpr GLfloat vertices[] = {-1.f,-1.f,0.f,1.f, 1.f,-1.f,1.f,1.f,
                                  -1.f,1.f,0.f,0.f, 1.f,1.f,1.f,0.f};
  const auto start = std::chrono::steady_clock::now();
  glViewport(0, 0, kWidth, kHeight);
  glUseProgram(program);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
  glEnableVertexAttribArray(0);
  const GLint resolution = glGetUniformLocation(program, "resolution");
  const GLint time = glGetUniformLocation(program, "time");
  const GLint hud_uniform = glGetUniformLocation(program, "hud");
  const GLint camera_uniform = glGetUniformLocation(program, "camera");
  const GLint camera_size = glGetUniformLocation(program, "cameraSize");
  const GLint detection_boxes = glGetUniformLocation(program, "detectionBoxes");
  const GLint detection_count = glGetUniformLocation(program, "detectionCount");
  GLuint hud_texture = 0;
  glGenTextures(1, &hud_texture);
  glBindTexture(GL_TEXTURE_2D, hud_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  GLuint camera_texture = 0;
  glGenTextures(1, &camera_texture);
  glBindTexture(GL_TEXTURE_2D, camera_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  std::vector<std::uint32_t> hud_pixels(kWidth * kHeight), hud_rgba(kWidth * kHeight);
  auto font = std::make_unique<hdmi_test::FontRenderer>();
  std::optional<hdmi_test::CpuTimes> previous_cpu;
  hdmi_test::MemoryUsage memory{};
  double cpu = 0.0, gpu = 0.0, fps = 0.0;
  CameraCapture camera_capture;
  YoloPipeline yolo_pipeline(camera_capture);
  std::uint64_t uploaded_camera_frame = 0;
  int camera_width = 0, camera_height = 0;
  std::uint64_t frame = 0, measured_frames = 0;
  std::uint64_t presented_yolo_frame = 0;
  hdmi_test::LatencyWindow end_to_end_latency(512);
  auto sample_time = start, fps_time = start, log_time = start;
  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    if (now - fps_time >= std::chrono::seconds(1)) {
      fps = measured_frames / std::chrono::duration<double>(now - fps_time).count();
      measured_frames = 0;
      fps_time = now;
    }
    if (now - log_time >= std::chrono::minutes(1)) {
      std::fprintf(stderr, "hdmi_egl_stress: %.1f FPS\n", fps);
      log_time = now;
    }
    if (now - sample_time >= std::chrono::seconds(1)) {
      const auto current_cpu = hdmi_test::parse_cpu_times(read_file("/proc/stat"));
      cpu = current_cpu && previous_cpu ? hdmi_test::compute_cpu_percent(*previous_cpu, *current_cpu) : 0.0;
      previous_cpu = current_cpu;
      memory = hdmi_test::parse_memory_usage(read_file("/proc/meminfo")).value_or(hdmi_test::MemoryUsage{});
      gpu = hdmi_test::parse_gpu_load_percent(read_file("/sys/devices/platform/bus@0/17000000.gpu/load")).value_or(0.0);
      const auto camera_frame = camera_capture.latest();
      const auto yolo_frame = yolo_pipeline.latest();
      draw_hud(hud_pixels, *font, fps, cpu, gpu, memory, frame, camera_capture.fps(),
               camera_capture.status(), camera_frame.get(), yolo_pipeline.fps(),
               yolo_pipeline.timings(), yolo_pipeline.status(), yolo_frame.get(),
               end_to_end_latency.percentile(0.50), end_to_end_latency.percentile(0.95),
               end_to_end_latency.percentile(0.99),
               static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now - start).count()));
      for (std::size_t index = 0; index < hud_pixels.size(); ++index) {
        const auto rgb = hud_pixels[index];
        hud_rgba[index] = ((rgb >> 16U) & 0xffU) | (rgb & 0xff00U) | ((rgb & 0xffU) << 16U) | 0xff000000U;
      }
      // Camera uploads use texture unit 1.  HUD must always update its own
      // texture on unit 0; otherwise the camera preview is overwritten with
      // glyph pixels and the two layers visibly overlap.
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, hud_texture);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight, GL_RGBA,
                      GL_UNSIGNED_BYTE, hud_rgba.data());
      sample_time = now;
    }
    // Fragment mediump floats lose sub-frame precision as an unbounded time
    // value grows.  All animation terms are periodic over four seconds, so a
    // bounded phase preserves seamless motion indefinitely.
    const float elapsed = std::fmod(std::chrono::duration<float>(now - start).count(), 4.0F);
    const auto camera_frame = camera_capture.latest();
    if (camera_frame && camera_frame->frame_number != uploaded_camera_frame) {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, camera_texture);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      if (camera_frame->width != camera_width || camera_frame->height != camera_height) {
        camera_width = camera_frame->width;
        camera_height = camera_frame->height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, camera_width, camera_height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, camera_frame->bgr.data());
      } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, camera_width, camera_height, GL_RGB,
                        GL_UNSIGNED_BYTE, camera_frame->bgr.data());
      }
      uploaded_camera_frame = camera_frame->frame_number;
    }
    const auto yolo_frame = yolo_pipeline.latest();
    if (yolo_frame && yolo_frame->source_frame_number != presented_yolo_frame &&
        yolo_frame->capture_time_ns > 0U) {
      const auto now_ns = steady_time_ns();
      if (now_ns >= yolo_frame->capture_time_ns) {
        end_to_end_latency.add(static_cast<double>(now_ns - yolo_frame->capture_time_ns) / 1'000'000.0);
      }
      presented_yolo_frame = yolo_frame->source_frame_number;
    }
    glUniform2f(resolution, static_cast<float>(kWidth), static_cast<float>(kHeight));
    glUniform1f(time, elapsed);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hud_texture);
    glUniform1i(hud_uniform, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, camera_texture);
    glUniform1i(camera_uniform, 1);
    glUniform2f(camera_size, static_cast<float>(camera_width), static_cast<float>(camera_height));
    GLfloat boxes[8 * 4]{};
    int count = 0;
    if (yolo_frame && yolo_frame->width > 0 && yolo_frame->height > 0) {
      count = std::min<int>(static_cast<int>(yolo_frame->detections.size()), 8);
      for (int index = 0; index < count; ++index) {
        const auto& detection = yolo_frame->detections[static_cast<std::size_t>(index)];
        const std::size_t offset = static_cast<std::size_t>(index) * 4U;
        boxes[offset] = detection.left / yolo_frame->width;
        // OpenGL samples the uploaded BGR texture with flipped T coordinates.
        // Convert detector image-space Y to that sampling space as well.
        boxes[offset + 1U] = 1.0F - detection.bottom / yolo_frame->height;
        boxes[offset + 2U] = detection.right / yolo_frame->width;
        boxes[offset + 3U] = 1.0F - detection.top / yolo_frame->height;
      }
    }
    glUniform4fv(detection_boxes, 8, boxes);
    glUniform1i(detection_count, count);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!wait_for_presented_frame(app)) return 6;
    if (!eglSwapBuffers(app.egl_display, app.egl_surface)) return 5;
    while (!app.frame_presented && wl_display_dispatch(app.display) != -1) {
    }
    if (!app.frame_presented) return 7;
    ++frame;
    ++measured_frames;
  }
}
