cmake_minimum_required(VERSION 3.10.2)
project(hello-decode)
find_package(OpenCV REQUIRED)

set(TARGET antispoofing)
set(SOURCES src/antispoofing.cpp)
set(VPL_NAME mfx)
set(CMAKE_MSVCIDE_RUN_PATH $ENV{PATH})
set(RUNARGS ${CMAKE_CURRENT_SOURCE_DIR}/content/fraud-video.h265 y)

# Set default build type to RelWithDebInfo if not specified
if(NOT CMAKE_BUILD_TYPE)
  message(
    STATUS "Default CMAKE_BUILD_TYPE not set using Release with Debug Info")
  set(CMAKE_BUILD_TYPE
      "RelWithDebInfo"
      CACHE
        STRING
        "Choose build type from: None Debug Release RelWithDebInfo MinSizeRel"
        FORCE)
endif()

add_executable(${TARGET} ${SOURCES})

get_directory_property(has_parent PARENT_DIRECTORY)
if(NOT has_parent)
  # only make run target available for stand-alone build
  add_custom_target(run ${TARGET} ${RUNARGS})
endif()

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  if(NOT DEFINED ENV{VSCMD_VER})
    set(CMAKE_MSVCIDE_RUN_PATH $ENV{PATH})
  endif()
endif()

find_package(VPL REQUIRED)
target_link_libraries(${TARGET} VPL::dispatcher)

if(UNIX)
  set(LIBVA_SUPPORT
      ON
      CACHE BOOL "Enable hardware support.")
  if(LIBVA_SUPPORT)
    find_package(PkgConfig REQUIRED)
    # note: pkg-config version for libva is *API* version
    pkg_check_modules(PKG_LIBVA IMPORTED_TARGET libva>=1.2)
    pkg_check_modules(PKG_LIBVA_DRM IMPORTED_TARGET libva-drm>=1.2)
    if(PKG_LIBVA_FOUND)
      target_compile_definitions(${TARGET} PUBLIC -DLIBVA_SUPPORT)
      set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
      set(THREADS_PREFER_PTHREAD_FLAG TRUE)
      find_package(Threads REQUIRED)
      target_link_libraries(${TARGET} PkgConfig::PKG_LIBVA
                            PkgConfig::PKG_LIBVA_DRM Threads::Threads)
      target_include_directories(${TARGET} PUBLIC ${PKG_LIBVA_INCLUDE_DIRS})
    else()
      message(
        SEND_ERROR
          "libva not found: set LIBVA_SUPPORT=OFF to build ${TARGET} without libva support"
      )
    endif()
  else()
    message(STATUS "Building ${TARGET} without hardware support")
  endif()
  message(STATUS "OpenCV library status:")
  message(STATUS "    config: ${OpenCV_DIR}")
  message(STATUS "    version: ${OpenCV_VERSION}")
  message(STATUS "    libraries: ${OpenCV_LIBS}")
  message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

  include_directories( ${OpenCV_INCLUDE_DIRS} )
  # Link your application with OpenCV libraries
  target_include_directories(${TARGET} PRIVATE $ENV{VPL_INCLUDE})
  target_link_libraries(${TARGET} ${VPL_LIB} ${OpenCV_LIBS})
endif()

install(TARGETS ${TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                                  COMPONENT dev)



