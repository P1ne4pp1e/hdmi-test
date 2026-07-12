CXX := g++
CPPFLAGS := -Iinclude $(shell pkg-config --cflags libdrm)
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS :=
LDLIBS := $(shell pkg-config --libs libdrm)
X11_CPPFLAGS := $(shell pkg-config --cflags x11)
X11_LDLIBS := $(shell pkg-config --libs x11)
WAYLAND_CPPFLAGS := $(shell pkg-config --cflags wayland-client)
WAYLAND_LDLIBS := $(shell pkg-config --libs wayland-client)
EGL_CPPFLAGS := $(shell pkg-config --cflags egl glesv2 wayland-egl)
EGL_LDLIBS := $(shell pkg-config --libs egl glesv2 wayland-egl)
FONT_CPPFLAGS := $(shell pkg-config --cflags freetype2 fontconfig)
FONT_LDLIBS := $(shell pkg-config --libs freetype2 fontconfig)

BUILD_DIR := build
APP := $(BUILD_DIR)/hdmi_test
TEST := $(BUILD_DIR)/test_pattern
METRICS_TEST := $(BUILD_DIR)/test_system_metrics
X11_KIOSK := $(BUILD_DIR)/hdmi_x11_kiosk
X11_KIOSK_SOURCES := src/x11_kiosk.cpp src/system_metrics.cpp
WAYLAND_KIOSK := $(BUILD_DIR)/hdmi_wayland_kiosk
WAYLAND_KIOSK_SOURCES := src/wayland_kiosk.cpp src/system_metrics.cpp src/font_renderer.cpp generated/xdg-shell-protocol.cpp
EGL_STRESS := $(BUILD_DIR)/hdmi_egl_stress
EGL_STRESS_SOURCES := src/egl_stress.cpp generated/xdg-shell-protocol.cpp

APP_SOURCES := src/main.cpp src/kms_display.cpp src/test_pattern.cpp
APP_OBJECTS := $(APP_SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

TEST_SOURCES := tests/test_pattern.cpp src/test_pattern.cpp
METRICS_TEST_SOURCES := tests/test_system_metrics.cpp src/system_metrics.cpp

.PHONY: all test x11-kiosk clean

all: $(APP) $(X11_KIOSK) $(WAYLAND_KIOSK) $(EGL_STRESS)

$(APP): $(APP_OBJECTS) | $(BUILD_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST): $(TEST_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(METRICS_TEST): $(METRICS_TEST_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(X11_KIOSK): $(X11_KIOSK_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(X11_CPPFLAGS) $(CXXFLAGS) $^ $(X11_LDLIBS) -o $@

$(WAYLAND_KIOSK): $(WAYLAND_KIOSK_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -Igenerated $(WAYLAND_CPPFLAGS) $(FONT_CPPFLAGS) $(CXXFLAGS) -Wno-error=attributes $^ $(WAYLAND_LDLIBS) $(FONT_LDLIBS) -o $@

$(EGL_STRESS): $(EGL_STRESS_SOURCES) | $(BUILD_DIR)
	$(CXX) -Igenerated $(WAYLAND_CPPFLAGS) $(EGL_CPPFLAGS) $(CXXFLAGS) -Wno-error=attributes $^ $(WAYLAND_LDLIBS) $(EGL_LDLIBS) -o $@

$(BUILD_DIR):
	mkdir -p $@

test: $(TEST) $(METRICS_TEST)
	$(TEST)
	$(METRICS_TEST)

x11-kiosk: $(X11_KIOSK)

clean:
	rm -rf $(BUILD_DIR)
