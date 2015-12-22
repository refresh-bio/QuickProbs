#pragma once
#include <vector>
#include <string>
#include <cmath>

#include "Common/mathex.h"
#include "Hardware/Kernel.h"
#include "Hardware/OpenCl.h"
#include "KernelRepository/KernelFactory.h"

#include <boost/lexical_cast.hpp>

namespace quickprobs
{


class RelaxationKernelWrapper 
{
public:
	const int stripeCount;

	const int stripeLength;

	const int sparseWidth;
	
	std::shared_ptr<clex::Kernel> obj;

	RelaxationKernelWrapper(
		std::shared_ptr<clex::OpenCL> cl,
		std::string name,
		const std::vector<int>& files, 
		std::vector<std::string> defines,
		int stripeCount,
		int stripeLength,
		int sparseWidth) : stripeCount(stripeCount), stripeLength(stripeLength), sparseWidth(sparseWidth)
	{

		defines.push_back("STRIPE_COUNT=" + boost::lexical_cast<std::string>(stripeCount));
		defines.push_back("STRIPE_LENGTH=" + boost::lexical_cast<std::string>(stripeLength));
		defines.push_back("STRIPE_LENGTH_LOG2=" + boost::lexical_cast<std::string>((int)mathex::log2(stripeLength)));

		obj = KernelFactory::instance(cl).create(files, name, defines);
	}
};

};