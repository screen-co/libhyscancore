cmake_minimum_required (VERSION 2.6)

project (libhyscancore)

include (FindPkgConfig)

#
# Default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif ()

#
# Default CPU architecture.
if (NOT CPU_ARCH)
  set (CPU_ARCH "x86")
endif ()

#
# CPU architecture.
if (CPU_ARCH STREQUAL "x86")
  add_definitions (-DCPU_ARCH_X86)
else ()
  message (FATAL_ERROR "${CPU_ARCH} is unsupported CPU architecture")
endif ()

#
# CPU bitness.
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  set (CPU32 1)
  add_definitions (-DCPU32)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (CPU64 1)
  add_definitions (-DCPU64)
else ()
  MATH (EXPR CPU_BITNESS 8*${CMAKE_SIZEOF_VOID_P})
  message (FATAL_ERROR "${CPU_BITNESS}bit is unsupported CPU bitness")
endif ()

#
# OpenMP support.
if (HYSCAN_CORE_USE_OPENMP)
  add_definitions (-DHYSCAN_CORE_USE_OPENMP)
endif ()

#
# Compilers options.
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  if (CPU_ARCH STREQUAL "x86" AND CPU32)
    set (COMPILER_FLAGS "-march=pentium4 -mtune=generic -mfpmath=sse -Wall")
  else ()
    set (COMPILER_FLAGS "-Wall")
  endif ()
  if (HYSCAN_CORE_USE_OPENMP)
    set (OMP_FLAGS "-fopenmp")
  endif ()
  set (COMPILER_FLAGS_RELEASE "${COMPILER_FLAGS} -O2 ${OMP_FLAGS}")
  set (COMPILER_FLAGS_DEBUG "${COMPILER_FLAGS} -Og ${OMP_FLAGS}")
  set (ADDON_LIBRARIES m)
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  if (HYSCAN_CORE_USE_OPENMP)
    set (OMP_FLAGS "/openmp")
  endif ()
  set (COMPILER_FLAGS_RELEASE "/wd4244 /wd4305 ${OMP_FLAGS}")
  set (COMPILER_FLAGS_DEBUG "/wd4244 /wd4305 ${OMP_FLAGS}")
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${COMPILER_FLAGS_RELEASE}")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${COMPILER_FLAGS_DEBUG}")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMPILER_FLAGS_RELEASE}")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMPILER_FLAGS_DEBUG}")

#
# Output directories.
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

#
# System dependencies.
pkg_check_modules (GLIB2 REQUIRED "glib-2.0 gobject-2.0 gthread-2.0 gio-2.0")
add_definitions (${GLIB2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS})

#
# HyScan libraries default paths.
if (NOT HYSCAN_DB_SOURCE_DIR)
  set (HYSCAN_DB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb)
endif ()
if (NOT HYSCAN_CACHE_SOURCE_DIR)
  set (HYSCAN_CACHE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache)
endif ()
if (NOT HYSCAN_TYPES_SOURCE_DIR)
  set (HYSCAN_TYPES_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes)
endif ()

#
# HyScan dependencies.
pkg_check_modules (HYSCAN QUIET hyscandb hyscancache hyscantypes)
if (${HYSCAN_FOUND})
  add_definitions (${HYSCAN_CFLAGS})
  link_directories (${HYSCAN_LIBRARY_DIRS})
else ()
  include_directories ("${HYSCAN_DB_SOURCE_DIR}/hyscandb")
  include_directories ("${HYSCAN_CACHE_SOURCE_DIR}/hyscancache")
  include_directories ("${HYSCAN_TYPES_SOURCE_DIR}/hyscantypes")
  link_directories ("${HYSCAN_DB_SOURCE_DIR}/bin")
  link_directories ("${HYSCAN_CACHE_SOURCE_DIR}/bin")
  link_directories ("${HYSCAN_TYPES_SOURCE_DIR}/bin")
  set (HYSCAN_LIBRARIES hyscandb hyscancache hyscantypes)
endif ()

#
# Include directories.
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscancore")

#
# Target.
add_definitions (-DG_LOG_DOMAIN="HyScanCore")
add_subdirectory ("hyscancore")
add_subdirectory ("tests")
