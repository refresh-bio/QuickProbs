#include "RefinementBase.h"
#include "EntropyEvaluator.h"

#include "Common/Log.h"

#include <list>

#undef min
#undef max

using namespace quickprobs;

std::unique_ptr<MultiSequence> quickprobs::RefinementBase::operator()(
	const GuideTree& tree,
	const float *seqsWeights, 
	const Array<float>& distances, 
	const Array<SparseMatrixType*> &sparseMatrices, 
	const ProbabilisticModel &model, 
	std::unique_ptr<MultiSequence> alignment)
{
	TIMER_CREATE(timer);
	TIMER_START(timer);

	// two timers for experimental purposes
	constructor->timeMatrixAddition = 0;
	constructor->timeDynamicProgramming = 0;
	model.timePreparation = 0;
	model.timeCalculaton = 0;
	model.timeReduction = 0;
	
	// get repetitions count
	int iterations = config->algorithm.refinement.iterations> 0 ? config->algorithm.refinement.iterations :
		(alignment->count() > config->algorithm.refinement.smallLargeThreshold ? 
			config->algorithm.refinement.largeIterations : 
			config->algorithm.refinement.smallIterations);

	LOG_NORMAL <<  iterations << " refinements with OpenMP using " << model.getNumThreads() << " threads..." << std::endl;


	if (iterations > 0) {
		LOG_NORMAL << "Initialisation..." << std::endl;
		bool prepared = initialise(seqsWeights, distances, sparseMatrices, model, *alignment);
		
		LOG_NORMAL << "Iteration started..." << std::endl;
		notifyObservers(*alignment, 0);

		for (currentIter = 0; currentIter < iterations; currentIter++) {
			if (prepared) {
				alignment = refine(tree, seqsWeights, distances, sparseMatrices, model, std::move(alignment), 0);
			}
			
			notifyObservers(*alignment, currentIter + 1);
		}

		finalise();
	}

	LOG_DEBUG << std::endl;

	STATS_WRITE("time.4-2-1-1-preparation", model.timePreparation);
	STATS_WRITE("time.4-2-1-2-calculation", model.timeCalculaton);
	STATS_WRITE("time.4-2-1-3-reduction", model.timeReduction);

	STATS_WRITE("time.4-2-1-building posterior matrices", constructor->timeMatrixAddition);
	STATS_WRITE("time.4-2-2-dynamic programming", constructor->timeDynamicProgramming);

	TIMER_STOP(timer);
	STATS_WRITE("time.4-2-refinement", timer.seconds());

	return std::move(alignment);
}

void quickprobs::RefinementBase::notifyObservers(const MultiSequence& alignment, int iteration)
{
	for (auto o : observers) {
		if (o != nullptr) {
			o->iterationDone(alignment, iteration);
		}
	}
}

std::unique_ptr<MultiSequence> quickprobs::RefinementBase::refine(
	const GuideTree& tree,
	const float *seqsWeights, 
	const Array<float>& distances, 
	const Array<SparseMatrixType*> &sparseMatrices, 
	const ProbabilisticModel &model, 
	std::unique_ptr<MultiSequence> alignment,
	int depth)
{
	std::set<int> groupOne, groupTwo;
	split(tree, *alignment, groupOne, groupTwo);

	if (groupOne.size() > 0 && groupTwo.size() > 0) {
		auto profileOne = alignment->extractSubset(groupOne); 
		auto profileTwo = alignment->extractSubset(groupTwo);

		auto candidate = constructor->alignAlignments(seqsWeights, distances, *profileOne, *profileTwo, sparseMatrices, model, nullptr);

		if (checkAcceptance(*alignment, *candidate)) {
			alignment = std::move(candidate);
		} 
	} 

	return std::move(alignment);
}


bool quickprobs::RefinementBase::checkAcceptance(
	const MultiSequence& reference, 
	const MultiSequence& candidate)
{
	bool ok = true;
	
	if (config->algorithm.refinement.acceptanceLength) {
		ok &= reference.GetSequence(0)->GetLength() >= candidate.GetSequence(0)->GetLength();
	}

	if (config->algorithm.refinement.acceptanceEntropy) {
		EntropyEvaluator eval;
		auto referenceScore = eval(reference);
		auto candidateScore = eval(candidate);
		ok &= (candidateScore >= referenceScore); 
	}

	return ok;
}

