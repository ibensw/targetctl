cmake_minimum_required (VERSION 3.11)

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v4.0.0
)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
  FetchContent_Populate(ftxui)
  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

project(targetctl
  LANGUAGES CXX
  VERSION 1.0.0
)

set (CMAKE_CXX_STANDARD 17)

add_executable(targetctl src/main.cpp src/systemctl.cpp src/servicetree.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)

find_package(PkgConfig)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
pkg_check_modules(FMT REQUIRED fmt)

target_include_directories(${PROJECT_NAME} PUBLIC ${SYSTEMD_INCLUDE_DIRS} PUBLIC ${FMT_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  ${SYSTEMD_LIBRARIES}
  ${FMT_LIBRARIES}
)
