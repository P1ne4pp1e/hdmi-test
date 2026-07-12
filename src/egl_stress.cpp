#include "xdg-shell-client-protocol.h"
#include "hdmi_test/font_renderer.hpp"
#include "hdmi_test/system_metrics.hpp"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

namespace {
constexpr int kWidth = 800;
constexpr int kHeight = 480;

std::string read_file(const char* path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void draw_hud(std::vector<std::uint32_t>& pixels, hdmi_test::FontRenderer& font,
              double fps, double cpu, double gpu, hdmi_test::MemoryUsage memory,
              std::uint64_t frame) {
  std::fill(pixels.begin(), pixels.end(), 0U);
  constexpr std::uint32_t kWhite = 0xF3F7FCU, kMuted = 0x91A5BCU, kCyan = 0x24D6D0U;
  const auto draw_right = [&font, &pixels](int right, int baseline, const char* value,
                                           int pixel_size, std::uint32_t color) {
    font.draw(pixels.data(), kWidth, kHeight,
              right - font.text_width(value, pixel_size), baseline, value, pixel_size, color);
  };
  font.draw(pixels.data(), kWidth, kHeight, 24, 31, "HDMI PERFORMANCE LAB", 22, kWhite);
  font.draw(pixels.data(), kWidth, kHeight, 24, 52, "Native Wayland / NVIDIA DRM", 13, kMuted);
  draw_right(764, 38, "LIVE  60 Hz", 15, kCyan);
  char text[80]{};
  font.draw(pixels.data(), kWidth, kHeight, 40, 112, "FRAME RATE", 13, kMuted);
  std::snprintf(text, sizeof(text), "%.1f FPS", fps);
  font.draw(pixels.data(), kWidth, kHeight, 40, 163, text, 34, kWhite);
  const char* labels[] = {"CPU LOAD", "GPU LOAD", "MEMORY"};
  const int baselines[] = {113, 173, 233};
  const double memory_used_gib = static_cast<double>(memory.used_kib) / (1024.0 * 1024.0);
  const double memory_total_gib = static_cast<double>(memory.total_kib) / (1024.0 * 1024.0);
  for (int index = 0; index < 3; ++index) {
    font.draw(pixels.data(), kWidth, kHeight, 340, baselines[index], labels[index], 13, kMuted);
    if (index == 0) std::snprintf(text, sizeof(text), "%.1f%%", cpu);
    if (index == 1) std::snprintf(text, sizeof(text), "%.1f%%", gpu);
    if (index == 2) std::snprintf(text, sizeof(text), "%.1f / %.1f GB", memory_used_gib, memory_total_gib);
    draw_right(760, baselines[index], text, 20, kWhite);
  }
  std::snprintf(text, sizeof(text), "FRAME  %llu", static_cast<unsigned long long>(frame));
  font.draw(pixels.data(), kWidth, kHeight, 40, 312, text, 13, kMuted);
  font.draw(pixels.data(), kWidth, kHeight, 40, 340, "GPU SHADER STRESS BACKGROUND", 16, kWhite);
  font.draw(pixels.data(), kWidth, kHeight, 40, 365, "V-SYNC ENABLED  /  HUD UPDATE 1 Hz", 13, kMuted);
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

    float rectangle(vec2 point, vec4 bounds) {
      vec2 lower = step(bounds.xy, point);
      vec2 upper = step(point, bounds.xy + bounds.zw);
      return lower.x * lower.y * upper.x * upper.y;
    }

    void main() {
      vec2 uv = gl_FragCoord.xy / resolution;
      vec2 hudUv = vec2(uv.x, 1.0 - uv.y);
      // Use only continuous, low-frequency functions for the motion test.
      // Fine repeating `fract` grids alias on a 800×480 panel and can look as
      // though frames are missing even when presentation is stable.
      float wave = 0.5 + 0.5 * sin(uv.x * 8.0 + uv.y * 4.0 + time * 2.8);
      float diagonal = 0.5 + 0.5 * sin(uv.x * 5.0 - uv.y * 7.0 - time * 2.1);
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
      float header = rectangle(hudUv, vec4(0.025, 0.025, 0.95, 0.115));
      float fpsCard = rectangle(hudUv, vec4(0.025, 0.175, 0.35, 0.365));
      float cpuCard = rectangle(hudUv, vec4(0.400, 0.175, 0.575, 0.108));
      float gpuCard = rectangle(hudUv, vec4(0.400, 0.300, 0.575, 0.108));
      float ramCard = rectangle(hudUv, vec4(0.400, 0.425, 0.575, 0.108));
      float footer = rectangle(hudUv, vec4(0.025, 0.575, 0.95, 0.375));
      float panel = max(max(header, fpsCard), max(max(cpuCard, gpuCard), max(ramCard, footer)));
      float inset = max(max(rectangle(hudUv, vec4(0.028, 0.028, 0.944, 0.109)),
                            rectangle(hudUv, vec4(0.028, 0.178, 0.344, 0.359))),
                        max(max(rectangle(hudUv, vec4(0.403, 0.178, 0.569, 0.102)),
                                rectangle(hudUv, vec4(0.403, 0.303, 0.569, 0.102))),
                            max(rectangle(hudUv, vec4(0.403, 0.428, 0.569, 0.102)),
                                rectangle(hudUv, vec4(0.028, 0.578, 0.944, 0.369)))));
      base = mix(base, vec3(0.025, 0.070, 0.115), panel * 0.82);
      base += (panel - inset) * vec3(0.10, 0.26, 0.36);
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
  GLuint hud_texture = 0;
  glGenTextures(1, &hud_texture);
  glBindTexture(GL_TEXTURE_2D, hud_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  std::vector<std::uint32_t> hud_pixels(kWidth * kHeight), hud_rgba(kWidth * kHeight);
  auto font = std::make_unique<hdmi_test::FontRenderer>();
  std::optional<hdmi_test::CpuTimes> previous_cpu;
  hdmi_test::MemoryUsage memory{};
  double cpu = 0.0, gpu = 0.0, fps = 0.0;
  std::uint64_t frame = 0, measured_frames = 0;
  auto sample_time = start, fps_time = start, log_time = start;
  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    if (now - fps_time >= std::chrono::seconds(1)) {
      fps = measured_frames / std::chrono::duration<double>(now - fps_time).count();
      measured_frames = 0;
      fps_time = now;
    }
    if (now - log_time >= std::chrono::seconds(5)) {
      std::fprintf(stderr, "hdmi_egl_stress: %.1f FPS\n", fps);
      log_time = now;
    }
    if (now - sample_time >= std::chrono::seconds(1)) {
      const auto current_cpu = hdmi_test::parse_cpu_times(read_file("/proc/stat"));
      cpu = current_cpu && previous_cpu ? hdmi_test::compute_cpu_percent(*previous_cpu, *current_cpu) : 0.0;
      previous_cpu = current_cpu;
      memory = hdmi_test::parse_memory_usage(read_file("/proc/meminfo")).value_or(hdmi_test::MemoryUsage{});
      gpu = hdmi_test::parse_gpu_load_percent(read_file("/sys/devices/platform/bus@0/17000000.gpu/load")).value_or(0.0);
      draw_hud(hud_pixels, *font, fps, cpu, gpu, memory, frame);
      for (std::size_t index = 0; index < hud_pixels.size(); ++index) {
        const auto rgb = hud_pixels[index];
        hud_rgba[index] = ((rgb >> 16U) & 0xffU) | (rgb & 0xff00U) | ((rgb & 0xffU) << 16U) | 0xff000000U;
      }
      glBindTexture(GL_TEXTURE_2D, hud_texture);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight, GL_RGBA,
                      GL_UNSIGNED_BYTE, hud_rgba.data());
      sample_time = now;
    }
    const float elapsed = std::chrono::duration<float>(now - start).count();
    glUniform2f(resolution, static_cast<float>(kWidth), static_cast<float>(kHeight));
    glUniform1f(time, elapsed);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hud_texture);
    glUniform1i(hud_uniform, 0);
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
