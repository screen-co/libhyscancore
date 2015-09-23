cmake_minimum_required( VERSION 2.6 )

project( libhyscancore )

include( FindPkgConfig )

#
# Default build type.
if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif()

#
# HyScan libraries default paths
if( NOT HYSCAN_DB_SOURCE_DIR )
  set( HYSCAN_DB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscandb )
endif()
if( NOT HYSCAN_CACHE_SOURCE_DIR )
  set( HYSCAN_CACHE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libhyscancache )
endif()

#
# OpenMP support.
if( HYSCAN_CORE_USE_OPENMP )
  add_definitions( -DHYSCAN_CORE_USE_OPENMP )
endif()

#
# Architecture check.
if( CMAKE_SIZEOF_VOID_P EQUAL 4 )
  set( CPU_ARCH x32 )
elseif( CMAKE_SIZEOF_VOID_P EQUAL 8 )
  set( CPU_ARCH x64 )
else()
  message( FATAL_ERROR "Unsupported architecture." )
endif()

#
# Compilers options.
if( ${CMAKE_C_COMPILER_ID} STREQUAL GNU  )
  if( ${CPU_ARCH} STREQUAL x32 )
    set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=pentium4 -mfpmath=sse" )
    set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -march=pentium4 -mfpmath=sse" )
  endif()
  set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast -Wall" )
  set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall" )
  if( HYSCAN_CORE_USE_OPENMP )
    set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fopenmp" )
    set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fopenmp" )
  endif()
  add_definitions( -D_GCC_COMPILER )
elseif( ${CMAKE_C_COMPILER_ID} STREQUAL MSVC )
  add_definitions( -D_MSVC_COMPILER )
else()
  message( FATAL_ERROR "Unsupported compiler" )
endif()

# Additional tuning for GCC on UNIX platform
if( ${CMAKE_C_COMPILER_ID} STREQUAL GNU AND UNIX )
  set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -pthread" )
  set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pthread" )
endif()

# Additional tuning for GCC on Windows platform
if( ${CMAKE_C_COMPILER_ID} STREQUAL GNU AND WIN32 )
  set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mthreads" )
  set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -mthreads" )
  add_definitions( -D_WIN32_WINNT=0x0600 )
endif()

# Additional tuning for MSVC on Windows platform
if( ${CMAKE_C_COMPILER_ID} STREQUAL MSVC AND WIN32 )
  add_definitions( -D_WIN32_WINNT=0x0600 )
  add_definitions( -D_CRT_SECURE_NO_WARNINGS )
endif()

#
# Output directories.
if( NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin )
endif()

if( NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin )
endif()

#
# Dependencies
pkg_check_modules( GLIB2 REQUIRED glib-2.0 gobject-2.0 gio-2.0 )
add_definitions( ${GLIB2_CFLAGS} )
link_directories( ${GLIB2_LIBRARY_DIRS} )

#
# Include directories.
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/hyscancore )

#
# HyScan dependencies.
pkg_check_modules( HYSCAN QUIET hyscandb hyscancache )
if( ${HYSCAN_FOUND} )
  add_definitions( ${HYSCAN_CFLAGS} )
  link_directories( ${HYSCAN_LIBRARY_DIRS} )
else()
  include_directories( "${HYSCAN_DB_SOURCE_DIR}/hyscandb" )
  include_directories( "${HYSCAN_CACHE_SOURCE_DIR}/hyscancache" )
  link_directories( "${HYSCAN_DB_SOURCE_DIR}/bin" )
  link_directories( "${HYSCAN_CACHE_SOURCE_DIR}/bin" )
  set( HYSCAN_LIBRARIES hyscandb hyscancache )
endif()

#
# Target.
add_definitions( -DG_LOG_DOMAIN="HyScanCore" )
add_subdirectory( hyscancore )
add_subdirectory( tests )

#
# Pkgconfig support.
#configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/urpc/urpc.pc.in ${CMAKE_CURRENT_SOURCE_DIR}/bin/urpc.pc @ONLY )
#install( FILES bin/urpc.pc DESTINATION lib/pkgconfig PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE WORLD_READ )
