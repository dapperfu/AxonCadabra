cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS src)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(axoncadabra_esp32)
