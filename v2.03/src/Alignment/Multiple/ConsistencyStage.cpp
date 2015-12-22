#include "ConsistencyStage.h"
#include "GuideTree.h"
#include "DataStructures/Sequence.h"
#include "DataStructures/MultiSequence.h"
#include "DataStructures/SparseMatrixType.h"
#include "DataStructures/SparseHelper.h"

#include "KernelRepository/Random.cl"
#include "Common/Log.h"

#include "Configuration.h"

#include <random>
#include <omp.h>

#undef min
#define filter(a,b,x)			(((x) <= (a)) ? 2.0f : 0)

using namespace std;
using namespace quickprobs;

ConsistencyStage::ConsistencyStage(std::shared_ptr<Configuration> config) : IAlgorithmStage(config)
{

}

/// <summary>
/// See declaration for all the details.
/// </summary>
void ConsistencyStage::operator()(
	const float* seqsWeights,
	quickprobs::ISequenceSet& set,
	Array<float>& distances, 
	Array<SparseMatrixType*>& sparseMatrices)
{
	TIMER_CREATE(timer);
	TIMER_START(timer);

	this->iterations = config->algorithm.consistency.itertions > 0 ? config->algorithm.consistency.itertions :
		(set.count() > config->algorithm.consistency.iterationsThreshold ? 
		config->algorithm.consistency.largeIterations : 
			config->algorithm.consistency.smallIterations);

	float x = (float)set.count(); 

	this->selfweight = config->algorithm.consistency.selfweight > 0 ? config->algorithm.consistency.selfweight :
		(set.count() > config->algorithm.consistency.selfweightThreshold ? 
		config->algorithm.consistency.largeSelfweight : 
	//	(2.5 / 200.0) * x + 0.5);		

	config->algorithm.consistency.smallSelfweight);



	//calculate sequence pairs for openmp model
	LOG_NORMAL << iterations << " consistency iterations with OpenMP using " << config->hardware.numThreads << " threads (selfweight=" 
		<< selfweight << ")..." << endl;


	run(seqsWeights, set, distances, sparseMatrices);
	TIMER_STOP_SAVE(timer, statistics["time.3-consistency transformation"]);

	size_t elems = config->io.enableVerbose ? SparseHelper::totalElements(sparseMatrices) : 0;
	STATS_WRITE("memory.sparse consistency cells", (double)elems);
}

/// <summary>
/// See declaration for all the details.
/// </summary>
void ConsistencyStage::run(
	const float* seqsWeights,
	quickprobs::ISequenceSet& set, 
	Array<float>& distances, 
	Array<SparseMatrixType*>& sparseMatrices)
{
	TIMER_CREATE(timerDelete);
	TIMER_CREATE(timerCalculation);

	const int numSeqs = set.count();
	
	for (int iter = 0; iter < iterations; ++iter) {
		bool filterFlag = true; 

		// turn of filtering for last iteration if num filterings below 0
		if (config->algorithm.consistency.numFilterings < 0) {
			if (iter == iterations - 1) {
				filterFlag = false;
			}
		} else {
			filterFlag = iter < config->algorithm.consistency.numFilterings;
		}

		TIMER_MOVEON(timerCalculation);
		Array<SparseMatrixType*> newSparseMatrices = doRelaxation(seqsWeights, &set, distances, sparseMatrices, filterFlag);
		TIMER_STOP(timerCalculation);

		// now replace the old posterior matrices
		TIMER_MOVEON(timerDelete);
		for (int i = 0; i < numSeqs; i++) {
			for (int j = 0; j < numSeqs; j++) {
				delete sparseMatrices[i][j];
				sparseMatrices[i][j] = newSparseMatrices[i][j];
			}
		}
		TIMER_STOP(timerDelete);
	}
}

Array<SparseMatrixType*> ConsistencyStage::doRelaxation(
	const float* seqsWeights, 
	const quickprobs::ISequenceSet *sequences, 
	const Array<float>& distances, 
	const Array<SparseMatrixType *> &sparseMatrices,
	bool filterFlag)
{
	const int numSeqs = sequences->count();

	Array<SparseMatrixType *> newSparseMatrices (numSeqs);

	omp_set_num_threads(config->hardware.numThreads);
	int pairIdx = 0;
	int numPairs = (numSeqs - 1) * numSeqs / 2;
	std::vector<std::pair<int,int>> seqsPairs(numPairs);
	for(int a = 0; a < numSeqs; a++){
		for(int b = a + 1; b < numSeqs; b++){
			seqsPairs[pairIdx].first = a;
			seqsPairs[pairIdx].second = b;
			pairIdx++;
		}
	}
	
	std::vector<int> seeds(numSeqs * numSeqs);
	std::default_random_engine eng;
	std::uniform_int_distribution<int> dist(0, 65536);
	std::generate(seeds.begin(), seeds.end(), [&eng, &dist]()->int {
		return dist(eng);
	});

	#pragma omp parallel for private(pairIdx) default(shared) schedule(dynamic)
	for(pairIdx = 0; pairIdx < numPairs; pairIdx++) {
		int i = seqsPairs[pairIdx].first;
		int j = seqsPairs[pairIdx].second;
		float wi = seqsWeights[i];
		float wj = seqsWeights[j];

		const Sequence *seq1 = sequences->GetSequence(i);
		const Sequence *seq2 = sequences->GetSequence(j);

		// get the original posterior matrix
		std::vector<float> *posteriorPtr = sparseMatrices[i][j]->getPosterior();
		std::vector<float> &posterior = *posteriorPtr;

		const int seq1Length = seq1->GetLength();
		const int seq2Length = seq2->GetLength();

		// contribution from the summation where z = x and z = y
		float wi_wj =  (wi + wj) * selfweight;
		float sumW = 1.0f;

		// contribution from all other sequences
		int seed = seeds[i * numSeqs + j];
		
		for (int k = 0; k < numSeqs; k++){
			if (k != i && k != j) {	
				float x = std::max(distances[i][k], distances[j][k]);
				seed = parkmiller(seed); // get next random number
				x = filter(config->algorithm.consistency.selectivity, 0, x);
				float w = ((float)seed) * RND_MAX_INV - x;  
				
				if  (w < 0) {				
					float wk = seqsWeights[k] / wi_wj;
					sumW += wk;
					relax (wk, sparseMatrices[i][k], sparseMatrices[k][j], posterior);
				} else {
				//	cout << "o-oh!";
				}
			}
		}
	
		for (int k = 0; k < (seq1Length+1) * (seq2Length+1); k++){
			posterior[k] /= sumW;
		}
		// mask out positions not originally in the posterior matrix
		SparseMatrixType *matXY = sparseMatrices[i][j];
		for (int y = 0; y <= seq2Length; y++) posterior[y] = 0;
		for (int x = 1; x <= seq1Length; x++){
			auto XYptr = matXY->getRowPtr(x);
			auto XYend = XYptr + matXY->getRowSize(x);
			auto base = posterior.begin() + x * (seq2Length + 1);
			int curr = 0;
			while (XYptr != XYend){

				// zero out all cells until the first filled column
				while (curr < XYptr->getColumn()){
					base[curr] = 0;
					++curr;
				}

				// now, skip over this column
				++curr;
				++XYptr;
			}

			// zero out cells after last column
			while (curr <= seq2Length){
				base[curr] = 0;
				curr++;
			}
		}
		
		// save the new posterior matrix
		auto m = new SparseMatrixType(seq1->GetLength(), seq2->GetLength(), posterior.data(), 
			filterFlag ? config->algorithm.posteriorCutoff : 1e-5);
		newSparseMatrices[i][j] = m;
		newSparseMatrices[j][i] = m->computeTranspose();

		delete posteriorPtr;
	}

	return newSparseMatrices;
}


void ConsistencyStage::relax (
	float weight, 
	const SparseMatrixType *matXZ, 
	const SparseMatrixType *matZY, 
	std::vector<float> &posterior)
{
	int lengthX = matXZ->getSeq1Length();
	int lengthY = matZY->getSeq2Length();
	assert (matXZ->getSeq2Length() == matZY->getSeq1Length());

	// for every x[i]
	for (int i = 1; i <= lengthX; i++){
		auto XZptr = matXZ->getRowPtr(i);
		auto XZend = XZptr + matXZ->getRowSize(i);

		std::vector<float>::iterator base = posterior.begin() + i * (lengthY + 1);

		// iterate through all x[i]-z[k]
		while (XZptr != XZend){
			auto ZYptr = matZY->getRowPtr(XZptr->getColumn());
			auto ZYend = ZYptr + matZY->getRowSize(XZptr->getColumn());
			const float XZval = XZptr->getValue();

			// iterate through all z[k]-y[j]
			while (ZYptr != ZYend){
				base[ZYptr->getColumn()] += weight * XZval * ZYptr->getValue();
				ZYptr++;
			}
			XZptr++;
		}
	}
}



void ConsistencyStage::relaxTransposed(
	float weight, 
	const SparseMatrixType *matZX, 
	const SparseMatrixType *matZY, 
	std::vector<float>& posterior)
{
	int lengthZ = matZX->getSeq1Length();
	int lengthY = matZY->getSeq2Length();

	// for every z[k]
	for (int k = 1; k <= lengthZ; k++){
		auto ZXptr = matZX->getRowPtr(k);
		auto ZXend = ZXptr + matZX->getRowSize(k);

		// iterate through all z[k]-x[i]
		while (ZXptr != ZXend){
			auto ZYptr = matZY->getRowPtr(k);
			auto ZYend = ZYptr + matZY->getRowSize(k);
			const float ZXval = ZXptr->getValue();
			auto base = posterior.begin() + ZXptr->getColumn() * (lengthY + 1);

			// iterate through all z[k]-y[j]
			while (ZYptr != ZYend){
				base[ZYptr->getColumn()] += weight * ZXval * ZYptr->getValue();
				ZYptr++;
			}
			ZXptr++;
		}
	}
}

