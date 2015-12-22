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

class KernelMSA : public ExtendedMSA, public IRefinementObserver
{
public:
	static void printWelcome();

	/// <summary>
	/// </summary>
	/// <param name=cl></param>
	/// <returns></returns>
	KernelMSA(
		std::shared_ptr<clex::OpenCL> cl,  
		std::shared_ptr<Configuration> config);

	virtual ~KernelMSA() {}

	virtual std::unique_ptr<MultiSequence> doAlign(MultiSequence *sequences);

	virtual void iterationDone(const MultiSequence& alignment, int iteration);

protected:
	std::string version;	
	
	std::shared_ptr<clex::OpenCL> cl;


protected:	
	
	virtual void degenerateDistances(Array<float> &distances);

	virtual void buildDistancesHistogram(const Array<float>& distances);
};

};