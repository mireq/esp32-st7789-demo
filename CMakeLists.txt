# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ram)

set(project_elf ${CMAKE_PROJECT_NAME}.elf)
set(project_bin ${CMAKE_PROJECT_NAME}.bin)
add_custom_command(
	OUTPUT ${project_bin}
	COMMAND $ENV{IDF_PATH}/components/esptool_py/esptool/esptool.py --chip esp32 elf2image --flash_freq 40m --flash_mode dio --flash_size 4MB --spi-connection SPI ${project_elf}
	DEPENDS ${project_elf}
)

add_custom_target(_project_binary ALL DEPENDS ${project_bin})
add_dependencies(_project_binary ${project_elf})
