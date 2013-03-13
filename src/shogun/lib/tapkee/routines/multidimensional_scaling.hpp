/* This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Copyright (c) 2012-2013 Sergey Lisitsyn
 */

#ifndef TAPKEE_MDS_H_
#define TAPKEE_MDS_H_

/* Tapkee includes */
#include <shogun/lib/tapkee/tapkee_defines.hpp>
#include <shogun/lib/tapkee/utils/time.hpp>
/* End of Tapkee includes */

#include <algorithm>

using std::random_shuffle;
using std::fill;

namespace tapkee
{
namespace tapkee_internal
{

template <class RandomAccessIterator>
Landmarks select_landmarks_random(RandomAccessIterator begin, RandomAccessIterator end, ScalarType ratio)
{
	Landmarks landmarks;
	landmarks.reserve(end-begin);
	for (RandomAccessIterator iter=begin; iter!=end; ++iter)
		landmarks.push_back(iter-begin);
	random_shuffle(landmarks.begin(),landmarks.end());
	landmarks.erase(landmarks.begin() + static_cast<IndexType>(landmarks.size()*ratio),landmarks.end());
	return landmarks;
}

template <class RandomAccessIterator, class PairwiseCallback>
DenseSymmetricMatrix compute_distance_matrix(RandomAccessIterator begin, RandomAccessIterator /*end*/,
                                             const Landmarks& landmarks, PairwiseCallback callback)
{
	timed_context context("Multidimensional scaling distance matrix computation");

	const IndexType n_landmarks = landmarks.size();
	DenseSymmetricMatrix distance_matrix(n_landmarks,n_landmarks);

#pragma omp parallel shared(begin,landmarks,distance_matrix,callback) default(none)
	{
		IndexType i_index_iter,j_index_iter;
#pragma omp for nowait
		for (i_index_iter=0; i_index_iter<n_landmarks; ++i_index_iter)
		{
			for (j_index_iter=i_index_iter; j_index_iter<n_landmarks; ++j_index_iter)
			{
				ScalarType d = callback(begin[landmarks[i_index_iter]],begin[landmarks[j_index_iter]]);
				d *= d;
				distance_matrix(i_index_iter,j_index_iter) = d;
				distance_matrix(j_index_iter,i_index_iter) = d;
			}
		}
	}
	return distance_matrix;
};

template <class RandomAccessIterator, class PairwiseCallback>
EmbeddingResult triangulate(RandomAccessIterator begin, RandomAccessIterator end, PairwiseCallback distance_callback,
                            const Landmarks& landmarks, const DenseVector& landmark_distances_squared, 
                            EmbeddingResult& landmarks_embedding, IndexType target_dimension)
{
	timed_context context("Landmark triangulation");
	
	bool* to_process = new bool[end-begin];
	fill(to_process,to_process+(end-begin),true);
	
	DenseMatrix embedding((end-begin),target_dimension);

	for (Landmarks::const_iterator iter=landmarks.begin(); 
			iter!=landmarks.end(); ++iter)
	{
		to_process[*iter] = false;
		embedding.row(*iter).noalias() = landmarks_embedding.first.row(iter-landmarks.begin());
	}

	for (IndexType i=0; i<target_dimension; ++i)
		landmarks_embedding.first.col(i).array() /= landmarks_embedding.second(i);

#pragma omp parallel shared(begin,end,to_process,distance_callback,landmarks, \
		landmarks_embedding,landmark_distances_squared,embedding) default(none)
	{
		DenseVector distances_to_landmarks = DenseVector(landmarks.size());
		IndexType index_iter;
#pragma omp for nowait
		for (index_iter=0; index_iter<IndexType(end-begin); ++index_iter)
		{
			if (!to_process[index_iter])
				continue;

			for (IndexType i=0; i<distances_to_landmarks.size(); ++i)
			{
				ScalarType d = distance_callback(begin[index_iter],begin[landmarks[i]]);
				distances_to_landmarks(i) = d*d;
			}
			//distances_to_landmarks.array().square();

			distances_to_landmarks -= landmark_distances_squared;
			embedding.row(index_iter).noalias() = -0.5*landmarks_embedding.first.transpose()*distances_to_landmarks;
		}
	}

	delete[] to_process;

	return EmbeddingResult(embedding,DenseVector());
}

template <class RandomAccessIterator, class PairwiseCallback>
DenseSymmetricMatrix compute_distance_matrix(RandomAccessIterator begin, RandomAccessIterator end, 
                                             PairwiseCallback callback)
{
	timed_context context("Multidimensional scaling distance matrix computation");

	const IndexType n_vectors = end-begin;
	DenseSymmetricMatrix distance_matrix(n_vectors,n_vectors);

#pragma omp parallel shared(begin,distance_matrix,callback) default(none)
	{
		IndexType i_index_iter,j_index_iter;
#pragma omp for nowait
		for (i_index_iter=0; i_index_iter<n_vectors; ++i_index_iter)
		{
			for (j_index_iter=i_index_iter; j_index_iter<n_vectors; ++j_index_iter)
			{
				ScalarType d = callback(begin[i_index_iter],begin[j_index_iter]);
				d *= d;
				distance_matrix(i_index_iter,j_index_iter) = d;
				distance_matrix(j_index_iter,i_index_iter) = d;
			}
		}
	}
	return distance_matrix;
};

}
}

#endif
