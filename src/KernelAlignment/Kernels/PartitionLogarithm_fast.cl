#define GAP_OPEN		params->gapOpen
#define GAP_EXTEND		params->gapExt

#define TERM_GAP_OPEN	LOG_ONE
#define TERM_GAP_EXTEND LOG_ONE
#define PROB_LO 0.001f
#define PROB_HI 1.0f
typedef float partition_t;

	
struct PartitionFunctionParams
{
	float termGapOpen; 
	float termGapExtend; 
	float gapOpen; 
	float gapExt;
	float subMatrix[26 * 26];
};


/// <summary>
/// </summary>
/// <param name=seq1></param>
/// <param name=seq2></param>
/// <param name=seq1Length></param>
/// <param name=seq2Length></param>
/// <param name=scheduling></param>
/// <param name=params></param>
/// <param name=aux></param>
/// <param name=forward></param>
/// <returns></returns>
__kernel 
void Partition_ComputeForward(
	global		const	char* seq1,
	global		const	char* seq2,
						int seq1Length,
						int seq2Length,
						struct PosteriorSchedule scheduling,
	PART_MEM	const	struct PartitionFunctionParams* params MAX_SIZE(sizeof(struct PartitionFunctionParams)),
	local				float* aux,
	global				float* forward)
{
	
	int j = get_local_id(0);
	#define width (seq2Length + 1)
	#define height (seq1Length + 1)

	#define Zm forward
	#define Ze aux
	local float* Ze_Zf_Zm_prev = aux + width;

	// calculate indices
	int ij_cur = jaggedIndex(j, -j, height, JAG_FLOAT);			// 0 - current
	int ij_left = jaggedIndex(j - 1, -j, height, JAG_FLOAT);	// 1 - left
	
	// initializations
	if (j <= seq2Length) { 
		Ze[j] = (j > 0) ? LOG_ONE : LOG_ZERO;
	} 

	barrier(CLK_LOCAL_MEM_FENCE);

	float Zm_up = (j == 0) ? LOG_ONE : LOG_ZERO;
	float Zf_up = LOG_ZERO;
	float Ze_Zf_Zm_diag = LOG_ZERO;

	for (int iter = 0, iterCount = seq1Length + seq2Length + 1; iter < iterCount; ++iter) {
		int i = iter - j;	// calculate i coordinate starting from row 0

		float Ze_left;
		bool legal = i >= 0 && i <= seq1Length && j <= seq2Length;

		// initializations for first column
		if (j == 0 && i == 1) { 
			Zm_up = LOG_ZERO;
			Zf_up = LOG_ONE;  
		}

		// utilise elements from Ze array and update Ze_Zf_Zm_prev array
		if (legal && j > 0) {
			Ze_left = Ze[j - 1];
		}

		barrier(CLK_LOCAL_MEM_FENCE);
		
		if (legal) { // if indices are legal
			// initialization of first row
			if (i > 0 && j > 0) {				
				int id = fast_mad(seq1[i], 26, seq2[j]);
					
				// update Zf_up 
				float t = Zm_up + ((j == seq2Length) ? TERM_GAP_OPEN : GAP_OPEN);
				logOfSum_private(&t, Zf_up + ((j == seq2Length) ? TERM_GAP_EXTEND : GAP_EXTEND));
				Zf_up = t; 
				
				// update Zm
				Zm_up = Ze_Zf_Zm_diag + params->subMatrix[id]; // now Zm_cur, future Zm_up
						
				// update Ze 
				t = Zm[ij_left] + ((i == seq1Length) ? TERM_GAP_OPEN : GAP_OPEN);
				logOfSum_private(&t, Ze_left + ((i == seq1Length) ? TERM_GAP_EXTEND: GAP_EXTEND));
				Ze[j] = t;
			}
			
			Zm[ij_cur] = Zm_up;
			Ze_Zf_Zm_diag = Ze_Zf_Zm_prev[j - 1];
		}
			
		barrier(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE);
		
		if (legal) { 
			float t = Ze[j];
			logOfSum_private(&t, Zf_up);
			logOfSum_private(&t, Zm_up);
			Ze_Zf_Zm_prev[j] = t;
		}
		
		ij_cur += JAG_FLOAT; // move to next row
		ij_left += JAG_FLOAT;
	}	

	//store the sum of zm Zf_up ze (m,n)s in zm's 0,0 th position
	if (j == seq2Length) {
		Zm[0] = Ze_Zf_Zm_prev[j];
	}
	
	barrier(CLK_GLOBAL_MEM_FENCE);

	#undef height
	#undef width
	#undef Zm
}

/// <summary>
/// </summary>
/// <param name=seq1></param>
/// <param name=seq2></param>
/// <param name=seq1Length></param>
/// <param name=seq2Length></param>
/// <param name=scheduling></param>
/// <param name=params></param>
/// <param name=aux></param>
/// <param name=forward></param>
/// <param name=posterior></param>
/// <returns></returns>
__kernel 
void Partition_ComputeReverse(
	global		const	char* seq1,
	global		const	char* seq2,
						int seq1Length,
						int seq2Length,
						struct PosteriorSchedule scheduling,
	PART_MEM	const	struct PartitionFunctionParams* params MAX_SIZE(sizeof(struct PartitionFunctionParams)),
	local				float* aux,
	global	const		float* forward,
	global				float* posterior)
{
	int j = get_local_id(0);
	#define width (seq2Length + 1)
	#define height (seq1Length + 1)
	
	int j_rev = seq2Length - j;

	++seq1; // + 1 because of '@' character at the beginning of seq.
	++seq2;

	#define Ze aux
	local float* Zm = aux + 1 * width;
	local float* Ze_Zf_Zm_prev = aux + 2 * width;
	float forward0 = forward[0];
	float Zf = LOG_ZERO;
	float Ze_Zf_Zm = LOG_ZERO;

	// calculate indices
	int ij_cur = jaggedIndex(j, seq1Length + j_rev, height, JAG_FLOAT);						// current	
	int ij_diag = jaggedIndex(j + 1, seq1Length + 1 + j_rev, height, JAG_FLOAT);			// diagonal
	int ij_forward_diag = jaggedIndex(j + 1, seq1Length + 1 + j_rev, height, JAG_FLOAT);	// diagonal

	if (j < width) {
		Zm[j] = (j_rev == 0) ? LOG_ONE : LOG_ZERO; // 0 ... 0 1
		Ze[j] = (j_rev > 0)  ? LOG_ONE : LOG_ZERO; // 1 ... 1 0
	}

	if (j_rev == 0) {
		Ze_Zf_Zm_prev[seq2Length] = LOG_ONE;
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	for (int iter = 0, iterCount = seq1Length + seq2Length + 1; iter < iterCount; ++iter) {
		int i = seq1Length + j_rev - iter;

		float Zm_diag = LOG_ZERO;
		float Ze_diag = LOG_ZERO;

		if (j_rev > 0) {
			Zm_diag = Zm[j + 1];
			Ze_diag = Ze[j + 1];	
		} else if (i == seq1Length - 1)  { // initialisation of last row and column
			Zf = LOG_ONE; 
			Zm[j] = LOG_ZERO;
		}
		
		barrier(CLK_LOCAL_MEM_FENCE);

		if (i >= 0 && i <= seq1Length && j <= seq2Length) {

			#ifdef EXACT_REVERSE_PARTITION 
				if (i == 0 || j == 0) { posterior[ij_cur] = 0; } // initialization of first posterior row and column
			#endif

			if (i < seq1Length && j < seq2Length) // for all elements but last row and column
			{
				int id = fast_mad(seq1[i], 26, seq2[j]);

				// terminal gaps may be ignored here as last row and column are not used for posterior calculation
				float t = Zm[j] + GAP_OPEN;
				logOfSum_private(&t, Zf + GAP_EXTEND); 
				Zf = t;

				t = Zm_diag + GAP_OPEN;
				logOfSum_private(&t, Ze_diag + GAP_EXTEND);
				Ze[j] = t; 
				
				// use t to store probability
				t = (forward0 == LOG_ZERO) ? 0 : fast_exp(forward[ij_forward_diag] + Ze_Zf_Zm - forward0);
				Zm[j] = Ze_Zf_Zm + params->subMatrix[id]; // multiply by score
				posterior[ij_diag] = (t <= PROB_HI && t >= PROB_LO) ? t : 0;
			}
			
			if (j_rev > 0) { Ze_Zf_Zm = Ze_Zf_Zm_prev[j + 1]; }	
		}

		barrier(CLK_LOCAL_MEM_FENCE);

		// initialize last column
		if (j_rev > 0) {
			float t = Ze[j];
			logOfSum_private(&t, Zf);
			logOfSum_private(&t, Zm[j]);
			Ze_Zf_Zm_prev[j] = t;
		}

		barrier(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE);

		ij_cur -= JAG_FLOAT;
		ij_diag -= JAG_FLOAT;
		ij_forward_diag -= JAG_FLOAT;
	}			

	barrier(CLK_GLOBAL_MEM_FENCE);

	#undef height
	#undef width
}


/// <summary>
/// </summary>
/// <param name=seq1></param>
/// <param name=seq2></param>
/// <param name=seq1Length></param>
/// <param name=seq2Length></param>
/// <param name=scheduling></param>
/// <param name=params></param>
/// <param name=aux></param>
/// <param name=posterior></param>
/// <returns></returns>
__kernel 
void Partition_ComputeAll(
	global		const	char* seq1,
	global		const	char* seq2,
						int seq1Length,
						int seq2Length,
						struct PosteriorSchedule scheduling,
	PART_MEM	const	struct PartitionFunctionParams* params MAX_SIZE(sizeof(struct PartitionFunctionParams)),
	local				float* auxiliaryRows, 
	global				float* auxiliaryLayer,
	global				float* posterior)
{
	Partition_ComputeForward(seq1, seq2, seq1Length, seq2Length, scheduling, params, auxiliaryRows, auxiliaryLayer);
	Partition_ComputeReverse(seq1, seq2, seq1Length, seq2Length, scheduling, params, auxiliaryRows, auxiliaryLayer, posterior);
}
