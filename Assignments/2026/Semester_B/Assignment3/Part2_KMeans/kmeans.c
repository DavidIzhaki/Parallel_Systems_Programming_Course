/*
 *  kmeans.c
 *  Serial reference implementation of K-means on 2D points. Your job
 *  is to parallelize this file with OpenMP (see Assignment3.pdf).
 *
 *  Think about:
 *    - which loops are embarrassingly parallel
 *    - how to reduce per-cluster sums without serializing the workers
 *    - false sharing on the per-cluster accumulators
 *    - which scheduling clause fits each loop
 */

#include "kmeans.h"

#include <stdlib.h>
#include <math.h>

PointSet *createPointSet(int numPoints) {
    PointSet *p = (PointSet *)malloc(sizeof(PointSet));
    p->numPoints = numPoints;
    p->points = (Point *)malloc((size_t)numPoints * sizeof(Point));
    p->assignments = (int *)malloc((size_t)numPoints * sizeof(int));
    for (int i = 0; i < numPoints; i++) {
        p->assignments[i] = 0;
    }
    return p;
}

void freePointSet(PointSet *p) {
    if (p == NULL) return;
    free(p->points);
    free(p->assignments);
    free(p);
}

Centroids *createCentroids(int k) {
    Centroids *c = (Centroids *)malloc(sizeof(Centroids));
    c->k = k;
    c->centroids = (Point *)malloc((size_t)k * sizeof(Point));
    return c;
}

void freeCentroids(Centroids *c) {
    if (c == NULL) return;
    free(c->centroids);
    free(c);
}

static inline double squaredDistance(Point a, Point b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

int runKMeans(PointSet *data, Centroids *centroids, int maxIters, double tolerance) {
    int n = data->numPoints;
    int k = centroids->k;

    double *sumX = (double *)malloc((size_t)k * sizeof(double));
    double *sumY = (double *)malloc((size_t)k * sizeof(double));
    int *counts = (int *)malloc((size_t)k * sizeof(int));

    double tolSquared = tolerance * tolerance;
    int iter = 0;

    for (iter = 0; iter < maxIters; iter++) {
        /* Assignment step: nearest centroid for every point. */
        for (int i = 0; i < n; i++) {
            double bestDist = squaredDistance(data->points[i], centroids->centroids[0]);
            int bestCluster = 0;
            for (int c = 1; c < k; c++) {
                double d = squaredDistance(data->points[i], centroids->centroids[c]);
                if (d < bestDist) {
                    bestDist = d;
                    bestCluster = c;
                }
            }
            data->assignments[i] = bestCluster;
        }

        /* Reset per-cluster accumulators. */
        for (int c = 0; c < k; c++) {
            sumX[c] = 0.0;
            sumY[c] = 0.0;
            counts[c] = 0;
        }

        /* Accumulate into each cluster. */
        for (int i = 0; i < n; i++) {
            int c = data->assignments[i];
            sumX[c] += data->points[i].x;
            sumY[c] += data->points[i].y;
            counts[c]++;
        }

        /* Recompute centroids; track largest movement for convergence. */
        double maxMovement = 0.0;
        for (int c = 0; c < k; c++) {
            if (counts[c] == 0) continue;
            Point updated;
            updated.x = sumX[c] / counts[c];
            updated.y = sumY[c] / counts[c];
            double mv = squaredDistance(centroids->centroids[c], updated);
            if (mv > maxMovement) maxMovement = mv;
            centroids->centroids[c] = updated;
        }

        if (maxMovement < tolSquared) {
            iter++;
            break;
        }
    }

    free(sumX);
    free(sumY);
    free(counts);
    return iter;
}
