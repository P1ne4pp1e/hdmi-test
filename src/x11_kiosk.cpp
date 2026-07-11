#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>

namespace {

std::atomic_bool keep_running{true};

void handle_signal(int) { keep_running = false; }

unsigned long rgb(unsigned char red, unsigned char green, unsigned char blue) {
  return (static_cast<unsigned long>(red) << 16U) |
         (static_cast<unsigned long>(green) << 8U) |
         static_cast<unsigned long>(blue);
}

void draw(Display* display, Window window, GC graphics, int width, int height) {
  XSetForeground(display, graphics, rgb(11, 18, 32));
  XFillRectangle(display, window, graphics, 0, 0, static_cast<unsigned>(width),
                 static_cast<unsigned>(height));

  const int header_height = height / 6;
  XSetForeground(display, graphics, rgb(20, 32, 52));
  XFillRectangle(display, window, graphics, 0, 0, static_cast<unsigned>(width),
                 static_cast<unsigned>(header_height));

  XSetForeground(display, graphics, rgb(224, 231, 255));
  XDrawString(display, window, graphics, 32, header_height / 2 + 6,
              "HDMI DISPLAY TEST", 17);

  XSetForeground(display, graphics, rgb(31, 47, 72));
  for (int x = 0; x < width; x += 80) {
    XFillRectangle(display, window, graphics, x, header_height, 1,
                   static_cast<unsigned>(height - header_height));
  }
  for (int y = header_height; y < height; y += 80) {
    XFillRectangle(display, window, graphics, 0, y, static_cast<unsigned>(width), 1);
  }

  const int card_top = header_height + 64;
  const int card_height = height / 3;
  const int card_width = (width - 160) / 3;
  const unsigned long colors[] = {rgb(30, 200, 150), rgb(239, 83, 80),
                                  rgb(255, 183, 77)};
  const char* labels[] = {"DISPLAY LINK", "FRAME LOOP", "INPUT READY"};
  for (int index = 0; index < 3; ++index) {
    const int left = 40 + index * (card_width + 40);
    XSetForeground(display, graphics, colors[index]);
    XFillRectangle(display, window, graphics, left, card_top,
                   static_cast<unsigned>(card_width),
                   static_cast<unsigned>(card_height));
    XSetForeground(display, graphics, rgb(11, 18, 32));
    XDrawString(display, window, graphics, left + 18, card_top + 32,
                labels[index], static_cast<int>(std::strlen(labels[index])));
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
  localtime_r(&seconds, &local_time);
  char time_label[48]{};
  std::strftime(time_label, sizeof(time_label), "RUNNING  %Y-%m-%d  %H:%M:%S",
                &local_time);
  XSetForeground(display, graphics, rgb(224, 231, 255));
  XDrawString(display, window, graphics, 32, height - 32, time_label,
              static_cast<int>(std::strlen(time_label)));
  XFlush(display);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc > 1 && (std::strcmp(argv[1], "--help") == 0 ||
                   std::strcmp(argv[1], "-h") == 0)) {
    std::cout << "Usage: hdmi_x11_kiosk\n"
                 "Starts a fullscreen C++ test screen on the active X display. "
                 "Press Escape to exit.\n";
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

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  while (keep_running) {
    while (XPending(display) > 0) {
      XEvent event{};
      XNextEvent(display, &event);
    }
    draw(display, window, graphics, width, height);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  XFreeGC(display, graphics);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  return 0;
}
