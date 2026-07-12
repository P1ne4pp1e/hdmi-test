#include "hdmi_test/system_metrics.hpp"
#include "hdmi_test/font_renderer.hpp"
#include "xdg-shell-client-protocol.h"

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
#include <memory>
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
  wl_display* display = nullptr; wl_compositor* compositor = nullptr; xdg_wm_base* wm = nullptr; wl_shm* shm = nullptr;
  wl_surface* surface = nullptr; xdg_surface* xdg_surface_handle = nullptr; xdg_toplevel* toplevel = nullptr; Buffer buffer; std::optional<hdmi_test::CpuTimes> previous_cpu; std::unique_ptr<hdmi_test::FontRenderer> font;
};

std::string read_file(const char* path) { std::ifstream file(path); return {std::istreambuf_iterator<char>(file), {}}; }
std::uint32_t color(unsigned red, unsigned green, unsigned blue) { return (red << 16U) | (green << 8U) | blue; }
void rect(Buffer& buffer, int x, int y, int width, int height, std::uint32_t value) {
  for (int row = std::max(0, y); row < std::min(kHeight, y + height); ++row)
    for (int column = std::max(0, x); column < std::min(kWidth, x + width); ++column)
      buffer.pixels[row * kWidth + column] = value;
}
void glass(Buffer& buffer, int x, int y, int width, int height) {
  for (int row=y; row<y+height; ++row) for (int column=x; column<x+width; ++column) {
    std::uint32_t& pixel=buffer.pixels[row*kWidth+column];
    const int r=(pixel>>16)&255,g=(pixel>>8)&255,b=pixel&255;
    pixel=color((r*30+17*70)/100,(g*30+30*70)/100,(b*30+46*70)/100);
  }
  rect(buffer,x,y,width,1,color(41,65,93)); rect(buffer,x,y+height-1,width,1,color(41,65,93)); rect(buffer,x,y,1,height,color(41,65,93)); rect(buffer,x+width-1,y,1,height,color(41,65,93));
}
void draw(App& app, std::uint64_t frame, double fps, double cpu, double gpu, hdmi_test::MemoryUsage memory) {
  auto& b=app.buffer;
  for(int y=0;y<kHeight;++y) for(int x=0;x<kWidth;++x) { const int pulse=((x+static_cast<int>(frame)*3)+(y*2))%180; b.pixels[y*kWidth+x]=color(7+pulse/30,17+pulse/12,31+pulse/6); }
  const int offset=static_cast<int>((frame*3)%64); for(int x=-64+offset;x<kWidth;x+=64) rect(b,x,64,1,kHeight-64,color(18,58,99)); for(int y=80+offset/2;y<kHeight;y+=64) rect(b,0,y,kWidth,1,color(18,58,99));
  for(int i=0;i<7;++i) { int x=static_cast<int>((frame*(2+i)+i*137)%900)-50; int y=90+i*48; rect(b,x,y,80+i*12,3,color(36,214,208)); }
  rect(b,0,0,kWidth,64,color(7,17,31)); glass(b,20,84,280,174); glass(b,312,84,468,174); glass(b,20,270,760,190);
  auto& font=*app.font; font.draw(b.pixels,kWidth,kHeight,24,30,"HDMI PERFORMANCE LAB",22,color(243,247,252)); font.draw(b.pixels,kWidth,kHeight,24,52,"Native Wayland / NVIDIA DRM",13,color(145,165,188)); font.draw(b.pixels,kWidth,kHeight,646,38,"LIVE  60 Hz",15,color(36,214,208));
  char fps_text[24]{}; std::snprintf(fps_text,sizeof(fps_text),"%.1f",fps); font.draw(b.pixels,kWidth,kHeight,40,112,"FRAME RATE",13,color(145,165,188)); font.draw(b.pixels,kWidth,kHeight,40,163,fps_text,38,color(243,247,252)); font.draw(b.pixels,kWidth,kHeight,163,162,"FPS",16,color(36,214,208)); font.draw(b.pixels,kWidth,kHeight,40,205,"16.67 ms target",14,color(145,165,188));
  const char* labels[]={"CPU LOAD","GPU LOAD","MEMORY"}; char values[3][48]{}; std::snprintf(values[0],48,"%.1f%%",cpu); std::snprintf(values[1],48,"%.1f%%",gpu); std::snprintf(values[2],48,"%llu / %llu MB",static_cast<unsigned long long>(memory.used_kib/1024),static_cast<unsigned long long>(memory.total_kib/1024));
  const std::uint32_t accents[]={color(36,214,208),color(77,163,255),color(255,180,84)}; for(int i=0;i<3;++i){int x=332+i*148; font.draw(b.pixels,kWidth,kHeight,x,112,labels[i],12,color(145,165,188)); font.draw(b.pixels,kWidth,kHeight,x,153,values[i],i==2?18:24,color(243,247,252)); rect(b,x,180,120,6,color(41,65,93)); double value=i==0?cpu:(i==1?gpu:(memory.total_kib?100.0*memory.used_kib/memory.total_kib:0)); rect(b,x,180,static_cast<int>(std::clamp(value,0.0,100.0)*1.2),6,accents[i]); }
  font.draw(b.pixels,kWidth,kHeight,40,300,"LIVE MOTION STRESS FIELD",13,color(145,165,188)); for(int i=0;i<120;++i){int x=40+i*6; int wave=static_cast<int>((frame+i*7)%76); rect(b,x,420-wave,3,wave, i>95?color(36,214,208):color(77,163,255));}
  char footer[96]{}; std::snprintf(footer,sizeof(footer),"Frame %llu   Sample 1s   Weston direct scanout test",static_cast<unsigned long long>(frame)); font.draw(b.pixels,kWidth,kHeight,40,447,footer,13,color(145,165,188));
}
void registry_global(void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version) {
  (void)version;
  auto& app=*static_cast<App*>(data); if (std::strcmp(interface,"wl_compositor")==0) app.compositor=static_cast<wl_compositor*>(wl_registry_bind(registry,name,&wl_compositor_interface,4)); else if (std::strcmp(interface,"xdg_wm_base")==0) app.wm=static_cast<xdg_wm_base*>(wl_registry_bind(registry,name,&xdg_wm_base_interface,1)); else if (std::strcmp(interface,"wl_shm")==0) app.shm=static_cast<wl_shm*>(wl_registry_bind(registry,name,&wl_shm_interface,1)); }
void registry_remove(void*, wl_registry*, std::uint32_t) {}
const wl_registry_listener kRegistryListener{registry_global, registry_remove};
void wm_ping(void*, xdg_wm_base* wm, std::uint32_t serial) { xdg_wm_base_pong(wm,serial); }
const xdg_wm_base_listener kWmListener{wm_ping};
void surface_configure(void*, xdg_surface* surface, std::uint32_t serial) { xdg_surface_ack_configure(surface,serial); }
const xdg_surface_listener kSurfaceListener{surface_configure};
void top_configure(void*, xdg_toplevel*, int, int, wl_array*) {} void top_close(void*, xdg_toplevel*) {} void top_bounds(void*, xdg_toplevel*, int, int) {}
const xdg_toplevel_listener kTopListener{top_configure,top_close,top_bounds};
void buffer_release(void* data, wl_buffer*) { static_cast<Buffer*>(data)->busy=false; }
const wl_buffer_listener kBufferListener{buffer_release};
int create_file() { char path[]="/tmp/hdmi-wayland-XXXXXX"; int fd=mkstemp(path); if(fd>=0) unlink(path); return fd; }
}

int main() {
  App app{}; app.font=std::make_unique<hdmi_test::FontRenderer>(); if(!app.font->ready()) return 5; app.display=wl_display_connect(nullptr); if(!app.display) return 1;
  wl_registry* registry=wl_display_get_registry(app.display); wl_registry_add_listener(registry,&kRegistryListener,&app); wl_display_roundtrip(app.display);
  if(!app.compositor||!app.wm||!app.shm) return 2;
  int fd=create_file(); if(fd<0||ftruncate(fd,kSize)!=0) return 3; app.buffer.pixels=static_cast<std::uint32_t*>(mmap(nullptr,kSize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0)); if(app.buffer.pixels==MAP_FAILED) return 4;
  wl_shm_pool* pool=wl_shm_create_pool(app.shm,fd,kSize); app.buffer.handle=wl_shm_pool_create_buffer(pool,0,kWidth,kHeight,kStride,WL_SHM_FORMAT_XRGB8888); wl_buffer_add_listener(app.buffer.handle,&kBufferListener,&app.buffer); wl_shm_pool_destroy(pool); close(fd);
  xdg_wm_base_add_listener(app.wm,&kWmListener,&app); app.surface=wl_compositor_create_surface(app.compositor); app.xdg_surface_handle=xdg_wm_base_get_xdg_surface(app.wm,app.surface); xdg_surface_add_listener(app.xdg_surface_handle,&kSurfaceListener,&app); app.toplevel=xdg_surface_get_toplevel(app.xdg_surface_handle); xdg_toplevel_add_listener(app.toplevel,&kTopListener,&app); xdg_toplevel_set_fullscreen(app.toplevel,nullptr); wl_surface_commit(app.surface); wl_display_roundtrip(app.display);
  std::uint64_t frame=0,sample_frames=0; double fps=0.0,cpu_use=0.0,gpu=0.0; hdmi_test::MemoryUsage memory{}; auto next_sample=std::chrono::steady_clock::now(); auto fps_start=next_sample;
  for(;;) { const auto now=std::chrono::steady_clock::now(); if(now-fps_start>=std::chrono::seconds(1)){fps=sample_frames/std::chrono::duration<double>(now-fps_start).count(); sample_frames=0; fps_start=now;} if(now>=next_sample){auto cpu=hdmi_test::parse_cpu_times(read_file("/proc/stat")); cpu_use=cpu&&app.previous_cpu?hdmi_test::compute_cpu_percent(*app.previous_cpu,*cpu):0.0; if(cpu)app.previous_cpu=cpu; memory=hdmi_test::parse_memory_usage(read_file("/proc/meminfo")).value_or(hdmi_test::MemoryUsage{}); gpu=hdmi_test::parse_gpu_load_percent(read_file("/sys/devices/platform/bus@0/17000000.gpu/load")).value_or(0.0); next_sample=now+std::chrono::seconds(1);} while(app.buffer.busy) wl_display_dispatch(app.display); draw(app,frame++,fps,cpu_use,gpu,memory); ++sample_frames; app.buffer.busy=true; wl_surface_attach(app.surface,app.buffer.handle,0,0); wl_surface_damage(app.surface,0,0,kWidth,kHeight); wl_surface_commit(app.surface); wl_display_roundtrip(app.display); }
}
