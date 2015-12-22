cmake_minimum_required (VERSION 2.6)
project (Console-OpenCl)

set(SRC_FILES main-opencl.cpp)

set (EXE_NAME "quickprobs-${QUICKPROBS_VERSION}")
if (WIN32)
	set (EXE_NAME "${EXE_NAME}-win")
else()
	set (EXE_NAME "${EXE_NAME}-linux")
endif()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenCL_INCLUDE_DIRS})

source_group("" FILES ${SRC_FILES})
add_executable(Console-OpenCl ${SRC_FILES})
target_link_libraries(Console-OpenCl 
	debug ${TCMalloc_LIBRARIES_DEBUG} 
	optimized ${TCMalloc_LIBRARIES_RELEASE}
	general ${Extra_LIBRARIES} ${OpenCL_LIBRARIES} ${Boost_LIBRARIES} Alignment KernelRepository Common Hardware)

set_target_properties(Console-OpenCl PROPERTIES OUTPUT_NAME ${EXE_NAME} ) 
set(EXECUTABLE_OUTPUT_PATH ${OUT_PATH})

