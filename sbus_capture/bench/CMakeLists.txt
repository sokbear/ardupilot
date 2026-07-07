cmake_minimum_required(VERSION 3.16)
project(electronic_pilot_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED core imgproc video tracking)

pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(LIBDRM REQUIRED libdrm)

add_executable(electronic_pilot_bench
    main.cpp
    camera_capture.cpp
    coord_map.cpp
    display_hub.cpp
    marker_tracker.cpp
    osd_renderer.cpp
    composite_out.cpp
    pca9685.cpp
    servo_gimbal.cpp
    gimbal_tracker.cpp
    mouse_input.cpp
    roi_selector.cpp
)

target_include_directories(electronic_pilot_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCAMERA_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
)

target_link_libraries(electronic_pilot_bench PRIVATE
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    pthread
)

target_compile_options(electronic_pilot_bench PRIVATE
    -Wall -Wextra
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_options(electronic_pilot_bench PRIVATE -O3 -mcpu=cortex-a76)
endif()

install(TARGETS electronic_pilot_bench RUNTIME DESTINATION bin)
