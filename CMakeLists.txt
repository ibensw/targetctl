cmake_minimum_required (VERSION 3.11)

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(tuilight
  GIT_REPOSITORY https://github.com/ibensw/tuilight
  GIT_TAG master
)
FetchContent_MakeAvailable(tuilight)

# FetchContent_GetProperties(ftxui)
# if(NOT ftxui_POPULATED)
#   FetchContent_Populate(ftxui)
#   add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
# endif()

include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.0
)
FetchContent_MakeAvailable(argparse)


project(targetctl
LANGUAGES CXX
VERSION 1.0.0
)

set (CMAKE_CXX_STANDARD 17)

add_executable(targetctl src/main.cpp src/systemctl.cpp src/servicetree.cpp src/ui.cpp src/journal.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# if(CLANG_TIDY)
  find_program(CLANG_TIDY_BIN NAMES "clang-tidy")
  set(CLANG_TIDY_COMMAND ${CLANG_TIDY_BIN} --extra-arg=-Wno-invalid-constexpr --extra-arg=-Wno-unknown-warning-option --extra-arg=-Wno-ignored-optimization-argument --extra-arg=-Wno-sign-conversion --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy)
  set_target_properties(targetctl PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
# endif()

find_package(PkgConfig)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
pkg_check_modules(FMT REQUIRED fmt)

target_include_directories(${PROJECT_NAME} PUBLIC ${SYSTEMD_INCLUDE_DIRS} PUBLIC ${FMT_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
  PRIVATE tuilight
  PRIVATE argparse
  ${SYSTEMD_LIBRARIES}
  ${FMT_LIBRARIES}
)

install(TARGETS targetctl RUNTIME)
