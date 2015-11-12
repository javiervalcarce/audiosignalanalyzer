#
# CMakefile para compilar la librería rest y los programas de ejemplo asociados.
#
# Para compilar hay que instalar previamente la herramienta cmake 2.8+, disponible para Linux, Windows y Mac,
# una vez instalada, hacemos:
#
# mkdir -p build
# cd build
# cmake ..
# make
#

PROJECT(libthdanalyzer)
CMAKE_MINIMUM_REQUIRED (VERSION 2.8.6)
set (CMAKE_LEGACY_CYGWIN_WIN32 0)

set (LIBTHDANALYZER_VERSION_MAJOR 0)
set (LIBTHDANALYZER_VERSION_MINOR 1)
set (LIBTHDANALYZER_VERSION_MICRO 0)
set (LIBTHDANALYZER_VERSION_STRING ${LIBTHDANALYZER_VERSION_MAJOR}.${LIBTHDANALYZER_VERSION_MINOR}.${LIBTHDANALYZER_VERSION_MICRO})

set (libthdanalyzer_SRCS src/thd_analyzer.cpp)
set (test_thd_analyzer_SRCS src/test_thd_analyzer.cpp)

set (CMAKE_VERBOSE_MAKEFILE on)

# Directorios de ficheros cabecera y de bibliotecas (opciones -I y -L respectivamente)
include_directories (src/include)

# opciones de compilación (CFLAGS)
add_definitions ("-Wall -g -D_FILE_OFFSET_BITS=64")

add_library(thdanalyzer SHARED ${libthdanalyzer_SRCS})
set_target_properties(thdanalyzer PROPERTIES VERSION ${LIBTHDANALYZER_VERSION_STRING})

add_executable(test_thd_analyzer ${test_thd_analyzer_SRCS})
target_link_libraries(test_thd_analyzer thdanalyzer asound m pthread)



