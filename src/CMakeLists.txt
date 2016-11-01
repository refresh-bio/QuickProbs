cmake_minimum_required (VERSION 2.6)
project(QuickProbs)

# The version number.
set(QUICKPROBS_VERSION "2.04" CACHE STRING "QuickProbs version")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BITNESS 64)
  set(OUT_PATH "${CMAKE_SOURCE_DIR}/../bin64")
else()
	set(BITNESS 32)
	message(SEND_ERROR "Only 64 bit platforms are supported!")
endif()

set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
set(Extra_LIBRARIES "")

set(Enable_DevMode OFF)


if (WIN32)
	# configure following option to make boost working
	set(Boost_PARENT_DIR "E:/SDKs/boost_1_61_0" CACHE STRING "Boost parent directory")
	
	set(Boost_LIB_DIR "${Boost_PARENT_DIR}/lib64-msvc-14.0" CACHE STRING "Boost library directory")
	set(Boost_INCLUDE_DIR "${Boost_PARENT_DIR}")
	set(BOOST_LIBRARYDIR "${Boost_LIB_DIR}")
	
	set(OpenCL_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/../libs/opencl/include" CACHE STRING "OpenCL include directory")
	set(OpenCL_LIBRARYDIR "${CMAKE_SOURCE_DIR}/../libs/opencl/x86_64" CACHE STRING "OpenCL library directory")
	set(TCMalloc_LIBRARYDIR "${CMAKE_SOURCE_DIR}/../libs/tcmalloc/lib64" CACHE STRING "TCMalloc library directory")

	set(EXTRA_LINKER_FLAGS "/INCLUDE:__tcmalloc")
	set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /openmp" )
	set(Extra_LIBRARIES "psapi.lib")
	
	find_library(TCMalloc_LIBRARIES_DEBUG tcmalloc_minimal_debug PATHS ${TCMalloc_LIBRARYDIR})
	find_library(TCMalloc_LIBRARIES_RELEASE tcmalloc_minimal PATHS ${TCMalloc_LIBRARYDIR})
	
else()
	
	set(Boost_INCLUDE_DIR "/usr/local/include")
	set(BOOST_LIBRARYDIR "/usr/local/lib")
	set(OpenCL_INCLUDE_DIRS "/usr/include" CACHE STRING "OpenCL include directory")
	
	set(OpenCL_LIBRARYDIR "/opt/AMDAPP/lib/x86_64" CACHE STRING "OpenCL library directory")

	set(TCMalloc_LIBRARYDIR "/usr/lib" CACHE STRING "TCMalloc library directory")
	set(EXTRA_LINKER_FLAGS "-ltcmalloc")	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fopenmp -std=c++0x")
	
	find_library(TCMalloc_LIBRARIES_DEBUG libtcmalloc_minimal_debug.a PATHS ${TCMalloc_LIBRARYDIR})
	find_library(TCMalloc_LIBRARIES_RELEASE libtcmalloc_minimal.a PATHS ${TCMalloc_LIBRARYDIR})
	
endif()

add_definitions(-DQUICKPROBS_VERSION=${QUICKPROBS_VERSION})

find_library(OpenCL_LIBRARIES OpenCL PATHS ${OpenCL_LIBRARYDIR})
find_package(Boost COMPONENTS filesystem program_options system)
	


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EXTRA_LINKER_FLAGS}" )	
add_subdirectory(Common)
add_subdirectory(Hardware)
add_subdirectory(Alignment)
add_subdirectory(KernelAlignment)
add_subdirectory(Console)
add_subdirectory(Console-OpenCL)

