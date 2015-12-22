cmake_minimum_required (VERSION 2.6)
project (KernelRepository)

set(MAIN_FILES
	KernelFactory.cpp
	KernelFactory.h
	KernelRepository.cpp
	KernelRepository.h
	process_kernel_files.py
)

set(KERNEL_FILES 
	AuxiliaryStructures.cl
	BinarySearch.cl
	Finalization.cl
	Finalization_long.cl
	LogarithmUtils.cl
	MemoryTransfers.cl
	Partition.cl
	PartitionLogarithm.cl
	Partition_long.cl
	PartitionLogarithm_long.cl
	Probabilistic.cl
	Probabilistic_long.cl
	Random.cl
	ScoreType.cl
	SparseMatrix.cl
	SparseMatrix_long.cl
	Utils.cl

	MultipleConsistency_old.cl
	MultipleConsistencyUtils.cl
	MultiplePartition.cl
	MultipleProbabilistic.cl
	MultipleFinalization.cl
	MultipleSparse.cl
	
)
	
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenCL_INCLUDE_DIRS})

source_group("" FILES ${MAIN_FILES})
source_group("Kernels" FILES ${KERNEL_FILES})

add_library(KernelRepository STATIC ${MAIN_FILES} ${KERNEL_FILES})
target_link_libraries(KernelRepository ${OpenCL_LIBRARIES} ${Boost_LIBRARIES} Common Hardware)

#if (WIN32)
#	add_custom_command(TARGET KernelRepository 
#	    PRE_BUILD
#	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}
#	    COMMAND python process_kernel_files.py
#	)
#endif()
