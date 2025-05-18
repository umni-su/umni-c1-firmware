# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "..\\..\\esp-idf-lib\\components")

set (FW_VER "1.0.0")

# Specify the configuration file from which the header file will be generated
configure_file (config.h.in config.h @ONLY)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Umni-FW)
