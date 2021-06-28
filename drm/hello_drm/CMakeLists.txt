# refer: https://github.com/ardera/flutter-pi/blob/master/CMakeLists.txt
cmake_minimum_required(VERSION 3.10.2)

project(hello_drm LANGUAGES C VERSION "1.0.0")

message(STATUS "Generator .............. ${CMAKE_GENERATOR}")
message(STATUS "Build Type ............. ${CMAKE_BUILD_TYPE}")

include(FindPkgConfig)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(GBM REQUIRED gbm)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLESV2 REQUIRED glesv2)
pkg_check_modules(LIBSYSTEMD REQUIRED libsystemd)
pkg_check_modules(LIBINPUT REQUIRED libinput)
pkg_check_modules(LIBXKBCOMMON REQUIRED xkbcommon)
pkg_check_modules(LIBUDEV REQUIRED libudev)

set(HELLO_DRM_SRC
  main.c
)

add_executable(hello_drm ${HELLO_DRM_SRC})

target_link_libraries(hello_drm
  ${DRM_LDFLAGS}
  ${GBM_LDFLAGS}
  ${EGL_LDFLAGS}
  ${GLESV2_LDFLAGS}
  ${LIBSYSTEMD_LDFLAGS}
  #${LIBINPUT_LDFLAGS}
  #${LIBUDEV_LDFLAGS}
  #${LIBXKBCOMMON_LDFLAGS}
  pthread dl rt m
)

target_include_directories(hello_drm PRIVATE
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${DRM_INCLUDE_DIRS}
  ${GBM_INCLUDE_DIRS}
  ${EGL_INCLUDE_DIRS}
  ${GLESV2_INCLUDE_DIRS}
  ${LIBSYSTEMD_INCLUDE_DIRS}
  #${LIBINPUT_INCLUDE_DIRS}
  #${LIBUDEV_INCLUDE_DIRS}
  #${LIBXKBCOMMON_INCLUDE_DIRS}
)

target_compile_options(hello_drm PRIVATE
  ${DRM_CFLAGS}  
  ${GBM_CFLAGS}
  ${EGL_CFLAGS}
  ${GLESV2_CFLAGS}
  ${LIBSYSTEMD_CFLAGS}
  #${LIBINPUT_CFLAGS}
  #${LIBUDEV_CFLAGS}
  ${LIBXKBCOMMON_CFLAGS}
  $<$<CONFIG:DEBUG>:-O0 -ggdb -DDEBUG>
)

target_link_options(hello_drm PRIVATE
  -rdynamic
)

install(TARGETS hello_drm RUNTIME DESTINATION bin)

