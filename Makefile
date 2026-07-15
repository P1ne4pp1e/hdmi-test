CXX := g++
NVCC := /usr/local/cuda/bin/nvcc
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS :=
WAYLAND_CPPFLAGS := $(shell pkg-config --cflags wayland-client)
WAYLAND_LDLIBS := $(shell pkg-config --libs wayland-client)
EGL_CPPFLAGS := $(shell pkg-config --cflags egl glesv2 wayland-egl)
EGL_LDLIBS := $(shell pkg-config --libs egl glesv2 wayland-egl)
FONT_CPPFLAGS := $(shell pkg-config --cflags freetype2 fontconfig)
FONT_LDLIBS := $(shell pkg-config --libs freetype2 fontconfig)
HIK_CPPFLAGS := -I/opt/MVS/include
HIK_LDLIBS := -L/opt/MVS/lib/aarch64 -Wl,-rpath,/opt/MVS/lib/aarch64 -lMvCameraControl -lpthread
TENSORRT_CPPFLAGS := -I/usr/include/aarch64-linux-gnu -I/usr/local/cuda/targets/aarch64-linux/include
TENSORRT_LDLIBS := -lnvinfer -lnvonnxparser -L/usr/local/cuda/targets/aarch64-linux/lib -lcudart

BUILD_DIR := build
METRICS_TEST := $(BUILD_DIR)/test_system_metrics
EGL_STRESS := $(BUILD_DIR)/hdmi_egl_stress
EGL_STRESS_SOURCES := src/egl_stress.cpp src/hik_camera.cpp src/system_metrics.cpp src/font_renderer.cpp generated/xdg-shell-protocol.cpp
HIK_CAMERA_PROBE := $(BUILD_DIR)/hdmi_hik_camera_probe
HIK_CAMERA_PROBE_SOURCES := src/hik_camera_probe.cpp src/hik_camera.cpp
YOLO_TEST := $(BUILD_DIR)/test_yolo_postprocess
YOLO_PREPROCESS_TEST := $(BUILD_DIR)/test_yolo_preprocess
LATENCY_TEST := $(BUILD_DIR)/test_latency_stats
TOUCH_TEST := $(BUILD_DIR)/test_touch_state
YOLO_SOURCES := src/yolo_detector.cpp
YOLO_CUDA_OBJECT := $(BUILD_DIR)/yolo_preprocess.o

METRICS_TEST_SOURCES := tests/test_system_metrics.cpp src/system_metrics.cpp

.PHONY: all test clean

all: $(EGL_STRESS) $(HIK_CAMERA_PROBE) $(METRICS_TEST) $(YOLO_TEST) $(YOLO_PREPROCESS_TEST) $(LATENCY_TEST) $(TOUCH_TEST)

$(METRICS_TEST): $(METRICS_TEST_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(EGL_STRESS): $(EGL_STRESS_SOURCES) $(YOLO_SOURCES) $(YOLO_CUDA_OBJECT) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -Igenerated $(HIK_CPPFLAGS) $(TENSORRT_CPPFLAGS) $(WAYLAND_CPPFLAGS) $(EGL_CPPFLAGS) $(FONT_CPPFLAGS) $(CXXFLAGS) -Wno-error=attributes $^ $(WAYLAND_LDLIBS) $(EGL_LDLIBS) $(FONT_LDLIBS) $(HIK_LDLIBS) $(TENSORRT_LDLIBS) -o $@

$(HIK_CAMERA_PROBE): $(HIK_CAMERA_PROBE_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(HIK_CPPFLAGS) $(CXXFLAGS) $^ $(HIK_LDLIBS) -o $@

$(YOLO_TEST): tests/test_yolo_postprocess.cpp $(YOLO_SOURCES) $(YOLO_CUDA_OBJECT) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(TENSORRT_CPPFLAGS) $(CXXFLAGS) $^ $(TENSORRT_LDLIBS) -o $@

$(YOLO_PREPROCESS_TEST): tests/test_yolo_preprocess.cpp $(YOLO_CUDA_OBJECT) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(TENSORRT_CPPFLAGS) $(CXXFLAGS) $^ $(TENSORRT_LDLIBS) -o $@

$(LATENCY_TEST): tests/test_latency_stats.cpp | $(BUILD_DIR)
	$(CXX) -Iinclude $(CXXFLAGS) $< -o $@

$(TOUCH_TEST): tests/test_touch_state.cpp | $(BUILD_DIR)
	$(CXX) -Iinclude $(CXXFLAGS) $< -o $@

$(YOLO_CUDA_OBJECT): src/yolo_preprocess.cu | $(BUILD_DIR)
	$(NVCC) -std=c++17 -O2 -Iinclude $(TENSORRT_CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

test: $(METRICS_TEST) $(YOLO_TEST) $(YOLO_PREPROCESS_TEST) $(LATENCY_TEST) $(TOUCH_TEST)
	$(METRICS_TEST)
	$(YOLO_TEST)
	$(YOLO_PREPROCESS_TEST)
	$(LATENCY_TEST)
	$(TOUCH_TEST)

clean:
	rm -rf $(BUILD_DIR)
