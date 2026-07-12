#include "xdg-shell-client-protocol.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace {
constexpr int kWidth = 800;
constexpr int kHeight = 480;

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
    void main() {
      vec2 uv = gl_FragCoord.xy / resolution;
      float grid = step(0.985, fract(uv.x * 13.0 + time * 0.08)) +
                   step(0.985, fract(uv.y * 8.0 - time * 0.05));
      float wave = 0.5 + 0.5 * sin(uv.x * 22.0 + uv.y * 9.0 + time * 2.0);
      float scan = smoothstep(0.94, 1.0, fract(uv.x + time * 0.12));
      vec3 base = mix(vec3(0.02, 0.07, 0.13), vec3(0.03, 0.22, 0.36), wave);
      base += grid * vec3(0.03, 0.18, 0.28);
      base += scan * vec3(0.05, 0.65, 0.65);
      gl_FragColor = vec4(base, 1.0);
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
  constexpr GLfloat vertices[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
  const auto start = std::chrono::steady_clock::now();
  glViewport(0, 0, kWidth, kHeight);
  glUseProgram(program);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  glEnableVertexAttribArray(0);
  const GLint resolution = glGetUniformLocation(program, "resolution");
  const GLint time = glGetUniformLocation(program, "time");
  for (;;) {
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
    glUniform2f(resolution, static_cast<float>(kWidth), static_cast<float>(kHeight));
    glUniform1f(time, elapsed);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!eglSwapBuffers(app.egl_display, app.egl_surface)) return 5;
    wl_display_dispatch_pending(app.display);
  }
}
