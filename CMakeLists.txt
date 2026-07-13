cmake_minimum_required(VERSION 3.16)

project(hdmi_test LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(FONT REQUIRED IMPORTED_TARGET freetype2 fontconfig)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl glesv2 wayland-egl)
find_package(X11 REQUIRED)
find_path(HIKSDK_INCLUDE_DIR MvCameraControl.h PATHS /opt/MVS/include REQUIRED)
find_library(HIKSDK_CAMERA_CONTROL NAMES MvCameraControl PATHS /opt/MVS/lib/aarch64 REQUIRED)
find_path(TENSORRT_INCLUDE_DIR NvInfer.h PATHS /usr/include/aarch64-linux-gnu REQUIRED)
find_library(TENSORRT_LIBRARY NAMES nvinfer REQUIRED)
find_library(TENSORRT_ONNX_LIBRARY NAMES nvonnxparser REQUIRED)
find_library(CUDART_LIBRARY NAMES cudart PATHS /usr/local/cuda/targets/aarch64-linux/lib REQUIRED)

add_library(hdmi_test_pattern src/test_pattern.cpp)
target_include_directories(hdmi_test_pattern PUBLIC include)
target_compile_options(hdmi_test_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(hdmi_test_metrics src/system_metrics.cpp)
target_include_directories(hdmi_test_metrics PUBLIC include)
target_compile_options(hdmi_test_metrics PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(hdmi_test src/main.cpp src/kms_display.cpp)
target_link_libraries(hdmi_test PRIVATE hdmi_test_pattern PkgConfig::LIBDRM)
target_compile_options(hdmi_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(hdmi_x11_kiosk src/x11_kiosk.cpp)
target_link_libraries(hdmi_x11_kiosk PRIVATE X11::X11 hdmi_test_metrics)
target_compile_options(hdmi_x11_kiosk PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(hdmi_wayland_kiosk
  src/wayland_kiosk.cpp
  src/font_renderer.cpp
  generated/xdg-shell-protocol.cpp
)
target_include_directories(hdmi_wayland_kiosk PRIVATE generated)
target_link_libraries(hdmi_wayland_kiosk PRIVATE hdmi_test_metrics PkgConfig::WAYLAND PkgConfig::FONT)
target_compile_options(hdmi_wayland_kiosk PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=attributes)

add_library(hdmi_test_yolo src/yolo_detector.cpp src/yolo_preprocess.cu)
target_include_directories(hdmi_test_yolo PUBLIC include ${TENSORRT_INCLUDE_DIR} /usr/local/cuda/targets/aarch64-linux/include)
target_link_libraries(hdmi_test_yolo PUBLIC ${TENSORRT_LIBRARY} ${TENSORRT_ONNX_LIBRARY} ${CUDART_LIBRARY})
target_compile_options(hdmi_test_yolo PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Werror>
)
set_target_properties(hdmi_test_yolo PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON CUDA_ARCHITECTURES 87)

add_executable(hdmi_egl_stress src/egl_stress.cpp src/hik_camera.cpp src/font_renderer.cpp generated/xdg-shell-protocol.cpp)
target_include_directories(hdmi_egl_stress PRIVATE generated ${HIKSDK_INCLUDE_DIR})
target_link_libraries(hdmi_egl_stress PRIVATE hdmi_test_metrics hdmi_test_yolo PkgConfig::WAYLAND PkgConfig::EGL PkgConfig::FONT ${HIKSDK_CAMERA_CONTROL})
target_compile_options(hdmi_egl_stress PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=attributes)

add_executable(hdmi_hik_camera_probe src/hik_camera_probe.cpp src/hik_camera.cpp)
target_include_directories(hdmi_hik_camera_probe PRIVATE include ${HIKSDK_INCLUDE_DIR})
target_link_libraries(hdmi_hik_camera_probe PRIVATE ${HIKSDK_CAMERA_CONTROL})
target_compile_options(hdmi_hik_camera_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(test_pattern tests/test_pattern.cpp)
target_link_libraries(test_pattern PRIVATE hdmi_test_pattern)
target_compile_options(test_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME test_pattern COMMAND test_pattern)
add_executable(test_system_metrics tests/test_system_metrics.cpp)
target_link_libraries(test_system_metrics PRIVATE hdmi_test_metrics)
target_compile_options(test_system_metrics PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME test_system_metrics COMMAND test_system_metrics)
add_executable(test_yolo_postprocess tests/test_yolo_postprocess.cpp)
target_link_libraries(test_yolo_postprocess PRIVATE hdmi_test_yolo)
target_compile_options(test_yolo_postprocess PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME test_yolo_postprocess COMMAND test_yolo_postprocess)
add_executable(test_yolo_preprocess tests/test_yolo_preprocess.cpp)
target_link_libraries(test_yolo_preprocess PRIVATE hdmi_test_yolo)
target_compile_options(test_yolo_preprocess PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME test_yolo_preprocess COMMAND test_yolo_preprocess)
add_executable(test_latency_stats tests/test_latency_stats.cpp)
target_include_directories(test_latency_stats PRIVATE include)
target_compile_options(test_latency_stats PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME test_latency_stats COMMAND test_latency_stats)
