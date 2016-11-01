#pragma once
#include <memory>
#include "Common/ProgramOptions.h"
#include "Hardware/OpenCl.h"
#include "Hardware/Kernel.h"
#include "Alignment/DataStructures/ContiguousMultiSequence.h"
#include "ExtendedMSA.h"
#include "PosteriorTasksWave.h"
#include "RelaxationSector.h"
#include "IRefinementObserver.h"

namespace quickprobs 
{

class KernelMSA : public ExtendedMSA
{
public:
	/// <summary>
	/// </summary>
	/// <param name=cl></param>
	/// <returns></returns>
	KernelMSA(
		std::shared_ptr<clex::OpenCL> cl,  
		std::shared_ptr<Configuration> config);

	virtual ~KernelMSA() {}


protected:
	std::shared_ptr<clex::OpenCL> cl;
};

};