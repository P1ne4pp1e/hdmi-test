CXX := g++
CPPFLAGS := -Iinclude $(shell pkg-config --cflags libdrm)
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS :=
LDLIBS := $(shell pkg-config --libs libdrm)
X11_CPPFLAGS := $(shell pkg-config --cflags x11)
X11_LDLIBS := $(shell pkg-config --libs x11)

BUILD_DIR := build
APP := $(BUILD_DIR)/hdmi_test
TEST := $(BUILD_DIR)/test_pattern
X11_KIOSK := $(BUILD_DIR)/hdmi_x11_kiosk

APP_SOURCES := src/main.cpp src/kms_display.cpp src/test_pattern.cpp
APP_OBJECTS := $(APP_SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

TEST_SOURCES := tests/test_pattern.cpp src/test_pattern.cpp

.PHONY: all test x11-kiosk clean

all: $(APP) $(X11_KIOSK)

$(APP): $(APP_OBJECTS) | $(BUILD_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST): $(TEST_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(X11_KIOSK): src/x11_kiosk.cpp | $(BUILD_DIR)
	$(CXX) $(X11_CPPFLAGS) $(CXXFLAGS) $< $(X11_LDLIBS) -o $@

$(BUILD_DIR):
	mkdir -p $@

test: $(TEST)
	$(TEST)

x11-kiosk: $(X11_KIOSK)

clean:
	rm -rf $(BUILD_DIR)
