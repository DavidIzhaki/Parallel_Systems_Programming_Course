/*
 *  kmeans.h
 *  Serial K-means clustering on 2D points. You are required to
 *  parallelize the implementation in kmeans.c using OpenMP.
 */

#ifndef KMEANS_H
#define KMEANS_H

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    int numPoints;
    Point *points;
    int *assignments;   /* cluster index for each point, length numPoints */
} PointSet;

typedef struct {
    int k;
    Point *centroids;   /* length k */
} Centroids;

/* Allocation helpers. */
PointSet *createPointSet(int numPoints);
void freePointSet(PointSet *p);

Centroids *createCentroids(int k);
void freeCentroids(Centroids *c);

/*
 * Run Lloyd's K-means algorithm in place: updates data->assignments
 * and centroids->centroids until either maxIters is reached or the
 * largest centroid movement (Euclidean distance) between two
 * consecutive iterations falls below `tolerance`.
 *
 * Returns the number of iterations actually executed.
 */
int runKMeans(PointSet *data, Centroids *centroids, int maxIters, double tolerance);

#endif /* KMEANS_H */
