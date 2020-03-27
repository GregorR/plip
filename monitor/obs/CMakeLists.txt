# Since obs-plip-monitor demands private headers of obs, it's not particularly
# easy to compile. If you want to compile it yourself, set up the complete obs,
# copy this directory to obs/plugins/obs-plip-monitor, and add it to
# obs/plugins/CMakeLists.txt, then build obs.

project(obs-plip-monitor)

set(obs-plip-monitor_SOURCES
      obs-plip-monitor.cxx)

find_package(Qt5Widgets REQUIRED)

add_library(obs-plip-monitor MODULE
      ${obs-plip-monitor_SOURCES})
target_link_libraries(obs-plip-monitor
      libobs
      Qt5::Widgets
      obs-frontend-api)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")

install_obs_plugin_with_data(obs-plip-monitor data)
