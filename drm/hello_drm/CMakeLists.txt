# refer: https://github.com/ardera/flutter-pi/blob/master/CMakeLists.txt
cmake_minimum_required(VERSION 3.10.2)
project(hello_drm LANGUAGES C VERSION "1.0.0")

set(HELLO_DRM_SRC main.c)
add_executable(hello_drm ${HELLO_DRM_SRC})

include(FindPkgConfig)
pkg_check_modules(DRM REQUIRED libdrm)

target_link_libraries(hello_drm
  ${DRM_LDFLAGS}
)

target_compile_options(hello_drm PRIVATE
  ${DRM_CFLAGS}  
)

