cmake_minimum_required (VERSION 2.6)
project (Console)

set(SRC_FILES main.cpp)

set (EXE_NAME "quickprobs-${QUICKPROBS_VERSION}")
if (WIN32)
	set (EXE_NAME "${EXE_NAME}")
else()
	set (EXE_NAME "${EXE_NAME}")
endif()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenCL_INCLUDE_DIRS})

source_group("" FILES ${SRC_FILES})
add_executable(Console ${SRC_FILES})
target_link_libraries(Console 
	debug ${TCMalloc_LIBRARIES_DEBUG} 
	optimized ${TCMalloc_LIBRARIES_RELEASE}
	general ${Extra_LIBRARIES} Alignment Common Hardware)

set_target_properties(Console PROPERTIES OUTPUT_NAME ${EXE_NAME} ) 
set(EXECUTABLE_OUTPUT_PATH ${OUT_PATH})

