/***********************************************
* # Copyright 2009-2010. Liu Yongchao
* # Contact: Liu Yongchao, School of Computer Engineering,
* #			 Nanyang Technological University.
* # Emails:	 liuy0039@ntu.edu.sg; nkcslyc@hotmail.com
* #
* # GPL version 3.0 applies.
* #
* ************************************************/
#pragma once

#include "GuideTree.h"

#include "Common/Array.h"

namespace quickprobs 
{

class ClusterTree : public GuideTree
{
public:
	ClusterTree(Array<float>& distances);
	~ClusterTree();

protected:
	
	Array<float>& distances;
	
	//construct the cluster tree
	virtual void build();
};

};