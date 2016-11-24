 /*
	QuickProbs version 2.0, October 2015
	Adam Gudys, Sebastian Deorowicz
	Silesian University of Technology, Gliwice, Poland
	adam.gudys@polsl.pl, sebastian.deorowicz@polsl.pl

	GPL version 3.0 applies.
*/
#include <exception>
#include "Common/Log.h"
#include "Alignment/Alignment.h"
#include "Common/MemoryTools.h"
#include "Common/rank.h"
#include "Common/deterministic_random.h"

using namespace std;

int main(int argc, char* argv[])
{
	std::shared_ptr<quickprobs::ExtendedMSA> msa;

	TIMER_CREATE(timer);
	
	TIMER_START(timer);
	try {
		TIMER_CREATE(configTimer);
		TIMER_START(configTimer);
		Log::getInstance(Log::LEVEL_NORMAL).enable();

		quickprobs::ExtendedMSA::printWelcome();

		auto config = std::shared_ptr<quickprobs::Configuration>(new quickprobs::Configuration());
		if (argc == 1 || !config->parse(argc, argv)) {
			LOG_NORMAL
				<< "Usage:" << endl
				<< "\t quickprobs [OPTION]... [infile]..." << endl << endl
				<< config->printOptions(false) << endl << endl;
		} else {

			if (config->io.enableVerbose) {
				Log::getInstance(Log::LEVEL_DEBUG).enable();
			}

			LOG_DEBUG << "Memory report:" << endl
				<< "System total physical = " << MemoryTools::systemTotalPhysical() << endl
			//	<< "System available physical = " MemoryTools::systemAvailablePhysical() << endl
				<< "Process current virtual = " << MemoryTools::processCurrentVirtual() << endl
				<< "Process peak virtual = " << MemoryTools::processPeakVirtual() << endl;

			LOG_NORMAL << "OpenCL device not specified - CPU variant will be used." << endl << endl;
			
			// alter parameters
			config->optimisation.useDoublePartition = true;
			
			LOG_DEBUG << "CONFIGURATION" << endl << config->toString() << endl << endl;

			TIMER_STOP(configTimer); 

			msa = shared_ptr<quickprobs::ExtendedMSA>(new quickprobs::ExtendedMSA(config));
			StatisticsProvider globalStats;

			for (int i = 0; i < config->io.inputFiles.size(); ++i) {
				(*msa)(config->io.inputFiles[i], config->io.outputFiles[i]);
				globalStats.addStats(*msa);
				msa->reset();
			}

			TIMER_STOP(timer);
			globalStats.writeStats("time.0.3-config", configTimer.seconds());
			globalStats.writeStats("time.whole", timer.seconds());
			cerr << "Elapsed time [seconds] = " << timer.seconds();

			cerr << "Saving stats..." << endl;
			globalStats.saveStats("execution.stats");	
			cerr << globalStats.printStats();
			cerr << endl;
		}
	}
	catch (std::runtime_error &ex) {
		std::cerr << ex.what() << endl;
		return -1;
	}

	return 0;
}
