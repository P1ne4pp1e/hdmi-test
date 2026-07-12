#include "hdmi_test/system_metrics.hpp"

#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <poll.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace {
constexpr int kWidth = 800;
constexpr int kHeight = 480;
constexpr int kStride = kWidth * 4;
constexpr int kSize = kStride * kHeight;

struct Buffer { wl_buffer* handle = nullptr; std::uint32_t* pixels = nullptr; bool busy = false; };
struct App {
  wl_display* display = nullptr; wl_compositor* compositor = nullptr; wl_shell* shell = nullptr; wl_shm* shm = nullptr;
  wl_surface* surface = nullptr; wl_shell_surface* shell_surface = nullptr; Buffer buffer; std::optional<hdmi_test::CpuTimes> previous_cpu;
};

std::string read_file(const char* path) { std::ifstream file(path); return {std::istreambuf_iterator<char>(file), {}}; }
std::uint32_t color(unsigned red, unsigned green, unsigned blue) { return (red << 16U) | (green << 8U) | blue; }
void rect(Buffer& buffer, int x, int y, int width, int height, std::uint32_t value) {
  for (int row = std::max(0, y); row < std::min(kHeight, y + height); ++row)
    for (int column = std::max(0, x); column < std::min(kWidth, x + width); ++column)
      buffer.pixels[row * kWidth + column] = value;
}
std::array<std::uint8_t, 7> glyph(char value) {
  switch (value) {
    case 'A': return {14,17,17,31,17,17,17}; case 'C': return {14,17,16,16,16,17,14};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14}; case 'H': return {17,17,17,31,17,17,17};
    case 'M': return {17,27,21,21,17,17,17}; case 'P': return {30,17,17,30,16,16,16};
    case 'R': return {30,17,17,30,20,18,17}; case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4}; case 'U': return {17,17,17,17,17,17,14};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,30,1,1,17,14};
    case '6': return {6,8,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,2,12};
    case '.': return {0,0,0,0,0,6,6}; case ':': return {0,6,6,0,6,6,0}; case '%': return {17,2,4,8,17,0,0};
    case '/': return {1,2,4,8,16,0,0}; case '-': return {0,0,0,31,0,0,0}; default: return {0,0,0,0,0,0,0};
  }
}
void text(Buffer& buffer, int x, int y, const char* value, int scale, std::uint32_t ink) {
  for (; *value != '\0'; ++value, x += 6 * scale) { const auto bits = glyph(*value); for (int row = 0; row < 7; ++row) for (int column = 0; column < 5; ++column) if (bits[row] & (1U << (4 - column))) rect(buffer, x + column * scale, y + row * scale, scale, scale, ink); }
}
void draw(App& app, std::uint64_t frame, double cpu, double gpu, hdmi_test::MemoryUsage memory) {
  auto& b = app.buffer; rect(b, 0, 0, kWidth, kHeight, color(11,18,32)); rect(b, 0, 0, kWidth, 82, color(20,32,52));
  text(b, 30, 24, "HDMI DEBUG DISPLAY", 3, color(224,231,255)); text(b, 30, 57, "WESTON DRM  800X480  60HZ", 2, color(120,180,255));
  const std::uint32_t cards[] = {color(30,200,150), color(50,130,240), color(255,183,77)};
  const char* labels[] = {"CPU", "GPU", "RAM"}; char values[3][48]{};
  std::snprintf(values[0], sizeof(values[0]), "%.1f%%", cpu); std::snprintf(values[1], sizeof(values[1]), "%.1f%%", gpu); std::snprintf(values[2], sizeof(values[2]), "%llu/%llu", static_cast<unsigned long long>(memory.used_kib/1024), static_cast<unsigned long long>(memory.total_kib/1024));
  for (int i=0;i<3;++i) { int left=35+i*255; rect(b,left,125,220,180,cards[i]); text(b,left+20,155,labels[i],3,color(11,18,32)); text(b,left+20,210,values[i],4,color(11,18,32)); }
  char footer[80]{}; std::snprintf(footer,sizeof(footer),"FRAME %llu  FPS 60  GPU DIRECT",static_cast<unsigned long long>(frame)); text(b,30,415,footer,2,color(224,231,255)); text(b,30,445,"SAMPLE INTERVAL 1S  DOUBLE BUFFER READY",2,color(130,160,200));
}
void registry_global(void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version) {
  auto& app=*static_cast<App*>(data); if (std::strcmp(interface,"wl_compositor")==0) app.compositor=static_cast<wl_compositor*>(wl_registry_bind(registry,name,&wl_compositor_interface,std::min(version,static_cast<std::uint32_t>(4)))); else if (std::strcmp(interface,"wl_shell")==0) app.shell=static_cast<wl_shell*>(wl_registry_bind(registry,name,&wl_shell_interface,1)); else if (std::strcmp(interface,"wl_shm")==0) app.shm=static_cast<wl_shm*>(wl_registry_bind(registry,name,&wl_shm_interface,1)); }
void registry_remove(void*, wl_registry*, std::uint32_t) {}
const wl_registry_listener kRegistryListener{registry_global, registry_remove};
void shell_ping(void*, wl_shell_surface* surface, std::uint32_t serial) { wl_shell_surface_pong(surface,serial); }
void shell_configure(void*, wl_shell_surface*, std::uint32_t, int, int) {}
void shell_popup_done(void*, wl_shell_surface*) {}
const wl_shell_surface_listener kShellListener{shell_ping,shell_configure,shell_popup_done};
void buffer_release(void* data, wl_buffer*) { static_cast<Buffer*>(data)->busy=false; }
const wl_buffer_listener kBufferListener{buffer_release};
int create_file() { char path[]="/tmp/hdmi-wayland-XXXXXX"; int fd=mkstemp(path); if(fd>=0) unlink(path); return fd; }
}

int main() {
  App app{}; app.display=wl_display_connect(nullptr); if(!app.display) return 1;
  wl_registry* registry=wl_display_get_registry(app.display); wl_registry_add_listener(registry,&kRegistryListener,&app); wl_display_roundtrip(app.display);
  if(!app.compositor||!app.shell||!app.shm) return 2;
  int fd=create_file(); if(fd<0||ftruncate(fd,kSize)!=0) return 3; app.buffer.pixels=static_cast<std::uint32_t*>(mmap(nullptr,kSize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0)); if(app.buffer.pixels==MAP_FAILED) return 4;
  wl_shm_pool* pool=wl_shm_create_pool(app.shm,fd,kSize); app.buffer.handle=wl_shm_pool_create_buffer(pool,0,kWidth,kHeight,kStride,WL_SHM_FORMAT_XRGB8888); wl_buffer_add_listener(app.buffer.handle,&kBufferListener,&app.buffer); wl_shm_pool_destroy(pool); close(fd);
  app.surface=wl_compositor_create_surface(app.compositor); app.shell_surface=wl_shell_get_shell_surface(app.shell,app.surface); wl_shell_surface_add_listener(app.shell_surface,&kShellListener,&app); wl_shell_surface_set_fullscreen(app.shell_surface,WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,60,nullptr);
  std::uint64_t frame=0; for(;;) { auto cpu=hdmi_test::parse_cpu_times(read_file("/proc/stat")); double cpu_use=cpu&&app.previous_cpu?hdmi_test::compute_cpu_percent(*app.previous_cpu,*cpu):0.0; if(cpu) app.previous_cpu=cpu; auto memory=hdmi_test::parse_memory_usage(read_file("/proc/meminfo")).value_or(hdmi_test::MemoryUsage{}); auto gpu=hdmi_test::parse_gpu_load_percent(read_file("/sys/devices/platform/bus@0/17000000.gpu/load")).value_or(0.0); while(app.buffer.busy) wl_display_dispatch(app.display); draw(app,frame++,cpu_use,gpu,memory); app.buffer.busy=true; wl_surface_attach(app.surface,app.buffer.handle,0,0); wl_surface_damage(app.surface,0,0,kWidth,kHeight); wl_surface_commit(app.surface); wl_display_roundtrip(app.display); sleep(1); }
}
