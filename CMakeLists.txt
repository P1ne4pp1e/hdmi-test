cmake_minimum_required(VERSION 3.16)

project(hdmi_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
find_package(X11 REQUIRED)

add_library(hdmi_test_pattern src/test_pattern.cpp)
target_include_directories(hdmi_test_pattern PUBLIC include)
target_compile_options(hdmi_test_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(hdmi_test src/main.cpp src/kms_display.cpp)
target_link_libraries(hdmi_test PRIVATE hdmi_test_pattern PkgConfig::LIBDRM)
target_compile_options(hdmi_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(hdmi_x11_kiosk src/x11_kiosk.cpp)
target_link_libraries(hdmi_x11_kiosk PRIVATE X11::X11)
target_compile_options(hdmi_x11_kiosk PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(test_pattern tests/test_pattern.cpp)
target_link_libraries(test_pattern PRIVATE hdmi_test_pattern)
target_compile_options(test_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME test_pattern COMMAND test_pattern)
