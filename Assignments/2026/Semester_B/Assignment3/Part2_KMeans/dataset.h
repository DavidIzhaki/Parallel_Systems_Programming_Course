/*
 *  dataset.h
 *  CSV I/O and deterministic initialization helpers for K-means.
 *  Used by the benchmark harness (main.c). Not part of the algorithm
 *  and not part of the student submission.
 *
 *  CSV format:
 *      # k=<int>
 *      # n=<int>
 *      <x> <y>
 *      <x> <y>
 *      ...
 *  Header lines may appear in any order; lines starting with '#' are
 *  comments and may also appear elsewhere.
 */

#ifndef DATASET_H
#define DATASET_H

#include "kmeans.h"

/* Load a CSV file. Returns a fresh PointSet and writes the dataset's
 * declared K to *out_k. Returns NULL on failure. */
PointSet *loadDataset(const char *filename, int *out_k);

/* Deterministic centroid initialization: pick K points spaced evenly
 * through the dataset (indices 0, n/k, 2n/k, ...). For our blob
 * datasets, sorted-by-cluster point order means these picks come from
 * different clusters, which is a reasonable starting point. */
Centroids *initCentroidsEvenly(const PointSet *data, int k);

#endif /* DATASET_H */
