CXX := g++
CPPFLAGS := -Iinclude $(shell pkg-config --cflags libdrm)
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS :=
LDLIBS := $(shell pkg-config --libs libdrm)

BUILD_DIR := build
APP := $(BUILD_DIR)/hdmi_test
TEST := $(BUILD_DIR)/test_pattern

APP_SOURCES := src/main.cpp src/kms_display.cpp src/test_pattern.cpp
APP_OBJECTS := $(APP_SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

TEST_SOURCES := tests/test_pattern.cpp src/test_pattern.cpp

.PHONY: all test clean

all: $(APP)

$(APP): $(APP_OBJECTS) | $(BUILD_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST): $(TEST_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR):
	mkdir -p $@

test: $(TEST)
	$(TEST)

clean:
	rm -rf $(BUILD_DIR)
