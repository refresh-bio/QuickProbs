#include <iomanip>
#include <algorithm>

#include <omp.h>

#include "Common/Timer.h"
#include "Common/MemoryTools.h"
#include "Common/Log.h"

#include "DataStructures/Sequence.h"
#include "DataStructures/MultiSequence.h"
#include "ProbabilisticModel.h"
#include "PartitionFunction.h"
#include "ExtendedMSA.h"
#include "DataStructures/SparseHelper.h"
#include "PartitionFunction.h"
#include "Configuration.h"
#include "ClusterTree.h"
#include "RandomRefinement.h"

using namespace quickprobs;
using namespace std;

/// <summary>
/// See declaration for all the details.
/// </summary>
quickprobs::BasicMSA::BasicMSA()
{

}

/// <summary>
/// See declaration for all the details.
/// </summary>
BasicMSA::BasicMSA(std::shared_ptr<Configuration> config) : config(config)
{
	int numCores = omp_get_num_procs();
	LOG_NORMAL << "Detected " << numCores << " CPU cores" << endl;
	
	if (config->hardware.numThreads <= 0){
		config->hardware.numThreads = numCores;
		LOG_NORMAL << "Automatically set " << config->hardware.numThreads << " OpenMP threads." << endl;
	} else {
		LOG_NORMAL <<"Statically set " << config->hardware.numThreads<<" OpenMP threads." << endl << endl;
	}
	
	posteriorStage = std::shared_ptr<PosteriorStage>(new PosteriorStage(config));
	consistencyStage = std::shared_ptr<ConsistencyStage>(new ConsistencyStage(config));
	constructionStage = std::shared_ptr<ConstructionStage>(new ConstructionStage(config));
	refinementStage = std::shared_ptr<RefinementBase>(new RandomRefinement(config, constructionStage));
}



/// <summary>
/// See declaration for all the details.
/// </summary>
void BasicMSA::operator()(std::string inputFile, std::string outputFile)
{
	TIMER_CREATE(timer);
	TIMER_START(timer);
	
	if (inputFile.length() == 0) {
		LOG_NORMAL << "No input file specified!" << std::endl;
		this->printUsage();
		return;
	}
	
	std::shared_ptr<ofstream> file;
	std::ostream* alignOutFile;
	
	// open output files
	if (outputFile.length() == 0) {
		LOG_NORMAL << "The final alignment will be printed out to STDOUT" << std::endl;
		alignOutFile = &std::cout;
	} else {
		LOG_NORMAL << "The final alignment will be saved to " << outputFile << std::endl;
		file = std::shared_ptr<ofstream>(new ofstream(outputFile.c_str(), ios::binary | ios::out | ios::trunc));
		alignOutFile = file.get();
	}
	
	// read the input sequences
	MultiSequence *sequences = new MultiSequence();
	LOG_NORMAL << "Loading sequence file: " << inputFile << "...";
	sequences->LoadMFA(inputFile, true);
	LOG_NORMAL << "done!" << endl;
	TIMER_STOP(timer);
	STATS_WRITE("time.0.2-sequence loading", TIMER_SECONDS(timer));

	// now, we can perform the alignments and write them out
	LOG_NORMAL << "Performing alignment..." << endl;
	auto alignment = doAlign(sequences);

	//write the alignment results to standard output
	TIMER_START(timer);
	LOG_NORMAL << "Saving results...";
	if (config->io.enableClustalWOutput) {
		alignment->WriteALN(*alignOutFile);
	} else {
		alignment->WriteMFA(*alignOutFile);
	}
	LOG_NORMAL << "Alignment finished!" << endl;
	TIMER_STOP(timer);
	STATS_WRITE("time.6-result storing", TIMER_SECONDS(timer));

	//release resources
	delete sequences;
}

/// <summary>
/// See declaration for all the details.
/// </summary>
std::unique_ptr<quickprobs::MultiSequence> BasicMSA::doAlign(quickprobs::MultiSequence* sequences)
{
	assert (sequences);

	TIMER_CREATE(timer);
	TIMER_CREATE(totalTimer);
	TIMER_START(totalTimer);

	const int numSeqs = sequences->count();
	
	// create distance matrix
	Array<float> distances(numSeqs);
	Array<SparseMatrixType*> sparseMatrices(numSeqs);
	
	// all pairwise steps are encapsulated in a separate function now
	posteriorStage->operator()(*sequences, distances, sparseMatrices);

	// create the guide tree
	auto tree = std::shared_ptr<GuideTree>(new ClusterTree(distances));
	tree->operator()();
	
	// perform the consistency transformation desired number of times
	auto weights = tree->getWeights();
	consistencyStage->operator()(weights.data(), *sequences, distances, sparseMatrices);
/*	
	//compute the final multiple sequence alignment
	TIMER_START(timer);
	auto alignment = constructionStage->operator()(weights.data(), tree.get(), *sequences, sparseMatrices, *posteriorStage->getModel());		
	alignment = refinementStage->operator()(weights.data(), distances, sparseMatrices, *posteriorStage->getModel(), *alignment);
	TIMER_STOP_SAVE(timer, statistics["time.4-final alignment"]);

	// build annotation
	if (config->io.enableAnnotation) {
		WriteAnnotation(alignment.get(), sparseMatrices);
	}
*/	
	// delete sparse matrices
	for (int a = 0; a < numSeqs-1; a++) {
		for (int b = a+1; b < numSeqs; b++) {
			delete sparseMatrices[a][b];
			delete sparseMatrices[b][a];
		}
	}

	TIMER_STOP(totalTimer);
	STATS_WRITE("time.stages-1 to 5", totalTimer.seconds());
	STATS_WRITE("memory.peak allocated MB", (double)MemoryTools::processPeakVirtual() / 1e6);

	computeDatasetStatistics(*sequences, tree->getWeights().data());

	this->joinStats(*posteriorStage);
	this->joinStats(*consistencyStage);
	this->joinStats(*constructionStage);
	this->joinStats(*refinementStage);

	//return alignment;
	return nullptr;
}

/// <summary>
/// See declaration for all the details.
/// </summary>
void BasicMSA::WriteAnnotation (
	quickprobs::MultiSequence *alignment, 
	const Array<SparseMatrixType*> &sparseMatrices)
{
		ofstream outfile (config->io.annotationFilename.c_str());

		if (outfile.fail()){
			throw std::runtime_error("ERROR: Unable to write annotation file.");
		}

		const int alignLength = alignment->GetSequence(0)->GetLength();
		const int numSeqs = alignment->count();

		std::vector<int> position (numSeqs, 0);
		std::vector<std::vector<char>::iterator> seqs (numSeqs);
		for (int i = 0; i < numSeqs; i++) seqs[i] = alignment->GetSequence(i)->getIterator();
		std::vector<pair<int,int> > active;
		active.reserve (numSeqs);

		std::vector<int> lab;
		for (int i = 0; i < numSeqs; i++) lab.push_back(alignment->GetSequence(i)->GetSortLabel());

		// for every column
		for (int i = 1; i <= alignLength; i++){

			// find all aligned residues in this particular column
			active.clear();
			for (int j = 0; j < numSeqs; j++){
				if (seqs[j][i] != '-'){
					active.push_back (make_pair(lab[j], ++position[j]));
				}
			}

			stable_sort (active.begin(), active.end());
			outfile << std::setw(4) << ComputeScore (active, sparseMatrices) << endl;
		}

		outfile.close();
}

/// <summary>
/// See declaration for all the details.
/// </summary>
int BasicMSA::ComputeScore (const std::vector<pair<int, int> > &active, 
	const Array<SparseMatrixType*> &sparseMatrices)
{
	if (active.size() <= 1) return 0;

	// ALTERNATIVE #1: Compute the average alignment score.

	float val = 0;
	for (int i = 0; i < (int) active.size(); i++){
		for (int j = i+1; j < (int) active.size(); j++){
			val += sparseMatrices[active[i].first][active[j].first]->getValue(active[i].second, active[j].second);
		}
	}

	return (int) (200 * val / ((int) active.size() * ((int) active.size() - 1)));

}

void BasicMSA::computeDatasetStatistics(const quickprobs::MultiSequence& sequences, const float* weights)
{
	int numSeqs = sequences.count();
	std::vector<int> lengths(numSeqs);
	
	int idx = 0;
	std::generate(lengths.begin(), lengths.end(), [&sequences, &idx]()->int {
		return sequences.GetSequence(idx++)->GetLength();
	});

	auto minmaxLength = std::minmax_element(lengths.begin(), lengths.end());
	double meanLength = (double)std::accumulate(lengths.begin(), lengths.end(), 0) / numSeqs;
	
	auto minmaxWeight = std::minmax_element(weights, weights + numSeqs);
	double meanWeight = (double)std::accumulate(weights, weights + numSeqs, 0) / numSeqs;

	STATS_WRITE("dataset.count", numSeqs);
	STATS_WRITE("dataset.length min", *(minmaxLength.first) );
	STATS_WRITE("dataset.length max", *(minmaxLength.second) );
	STATS_WRITE("dataset.length avg", meanLength);

	STATS_WRITE("dataset.weight min", *(minmaxWeight.first) );
	STATS_WRITE("dataset.weight max", *(minmaxWeight.second) );
	STATS_WRITE("dataset.weight avg", meanWeight);
}
