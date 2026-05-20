/*
 * Scalability + correctness harness for K-means.
 *
 * Iterates over every .csv file in ./inputs/ and for each one runs
 * runKMeans at THREAD_COUNTS = {1,2,4,8,16}. For each input it prints
 * a Threads | Time | Iters | Speedup | Efficiency | Correctness table
 * to stdout and to scalability.log.
 *
 * Correctness check: the T=1 result is captured as the reference
 * (final centroid positions); subsequent thread counts must converge
 * to the same centroids within a small tolerance. Reduction order
 * differences between thread counts can perturb FP sums slightly, so
 * we compare centroid coordinates (after a stable sort by x then y)
 * with a tolerance, rather than requiring bit-exact output.
 *
 * This file is for your own scalability testing -- DO NOT submit it.
 * Submit only kmeans.c (and kmeans.h if you modified it).
 *
 * Build (on the BIU servers):
 *     gcc -O2 -fopenmp main.c kmeans.c dataset.c -o kmeans_bench -lm
 * Populate inputs (one-time):
 *     python3 inputs/generate_inputs.py
 * Run:
 *     ./kmeans_bench
 */

#include "kmeans.h"
#include "dataset.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

static const int THREAD_COUNTS[] = {1, 2, 4, 8, 16};
static const int NUM_THREAD_COUNTS = (int)(sizeof(THREAD_COUNTS) / sizeof(THREAD_COUNTS[0]));

/* Bumped from (50, 1e-4) so the heavy CSV tiers do enough iterations
 * to make per-thread timing meaningful. Small/medium/large still
 * converge well before MAX_ITERS, so this is a no-op for them. */
static const int    MAX_ITERS = 100;
static const double TOLERANCE = 1e-8;
static const double CENTROID_MATCH_TOL = 1.0;   /* in input-coord units */

static const char *INPUT_DIR = "inputs";

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static bool hasCsvExtension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot != NULL && strcasecmp(dot, ".csv") == 0;
}

/* Sort by file size ascending so the run goes small -> medium ->
 * large -> heavy. File size is a perfect proxy for dataset size in
 * our CSV format. */
static int filenameCmpBySize(const void *a, const void *b) {
    struct stat sa, sb;
    char pa[512], pb[512];
    snprintf(pa, sizeof(pa), "%s/%s", INPUT_DIR, *(const char **)a);
    snprintf(pb, sizeof(pb), "%s/%s", INPUT_DIR, *(const char **)b);
    if (stat(pa, &sa) != 0 || stat(pb, &sb) != 0) return 0;
    return (sa.st_size > sb.st_size) - (sa.st_size < sb.st_size);
}

static char **listInputs(const char *dir, int *out_count) {
    DIR *d = opendir(dir);
    if (d == NULL) { *out_count = 0; return NULL; }

    int cap = 16, n = 0;
    char **names = (char **)malloc((size_t)cap * sizeof(char *));
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (!hasCsvExtension(e->d_name)) continue;
        if (n == cap) { cap *= 2; names = realloc(names, (size_t)cap * sizeof(char *)); }
        names[n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(char *), filenameCmpBySize);
    *out_count = n;
    return names;
}

/* qsort comparator for sorting centroids by (x, y) so that two runs
 * that produced the same clusters in different orderings can be
 * compared element-wise. */
static int pointCmp(const void *pa, const void *pb) {
    const Point *a = (const Point *)pa;
    const Point *b = (const Point *)pb;
    if (a->x < b->x) return -1;
    if (a->x > b->x) return  1;
    if (a->y < b->y) return -1;
    if (a->y > b->y) return  1;
    return 0;
}

/* Returns the number of centroids that don't match within tolerance. */
static int compareCentroids(const Centroids *a, const Centroids *b, double tol) {
    if (a->k != b->k) return -1;
    int k = a->k;
    Point *sa = malloc((size_t)k * sizeof(Point));
    Point *sb = malloc((size_t)k * sizeof(Point));
    memcpy(sa, a->centroids, (size_t)k * sizeof(Point));
    memcpy(sb, b->centroids, (size_t)k * sizeof(Point));
    qsort(sa, k, sizeof(Point), pointCmp);
    qsort(sb, k, sizeof(Point), pointCmp);
    int mismatches = 0;
    for (int i = 0; i < k; i++) {
        double dx = sa[i].x - sb[i].x;
        double dy = sa[i].y - sb[i].y;
        if (sqrt(dx * dx + dy * dy) > tol) mismatches++;
    }
    free(sa); free(sb);
    return mismatches;
}

/* ------------------------------------------------------------------ */
/* table printing                                                     */
/* ------------------------------------------------------------------ */

static void writeHeader(FILE *out, const char *name, int n, int k) {
    fprintf(out, "\n=== %s (n=%d, k=%d, maxIters=%d) ===\n", name, n, k, MAX_ITERS);
    fprintf(out, "%-8s | %-10s | %-6s | %-8s | %-10s | %s\n",
            "Threads", "Time(s)", "Iters", "Speedup", "Efficiency", "Correctness");
    fprintf(out, "---------+------------+--------+----------+------------+----------------------\n");
}

static void writeRow(FILE *out, int threads, double seconds, int iters,
                     double baseline, const char *correctness) {
    double speedup = baseline / seconds;
    double efficiency = speedup / (double)threads;
    fprintf(out, "%-8d | %-10.4f | %-6d | %-8.3f | %-10.3f | %s\n",
            threads, seconds, iters, speedup, efficiency, correctness);
}

/* ------------------------------------------------------------------ */
/* per-input benchmark                                                */
/* ------------------------------------------------------------------ */

static void benchmarkInput(const char *filename, FILE *log) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, filename);

    int k = 0;
    PointSet *data = loadDataset(path, &k);
    if (data == NULL) return;

    writeHeader(stdout, filename, data->numPoints, k);
    writeHeader(log,    filename, data->numPoints, k);

    double baseline = 0.0;
    Centroids *reference = NULL;

    for (int t = 0; t < NUM_THREAD_COUNTS; t++) {
        int threads = THREAD_COUNTS[t];
        omp_set_num_threads(threads);

        /* Fresh initial centroids and assignments per run -- same
         * starting point for every thread count keeps the comparison
         * apples-to-apples. */
        Centroids *centroids = initCentroidsEvenly(data, k);
        for (int i = 0; i < data->numPoints; i++) data->assignments[i] = 0;

        double t0 = omp_get_wtime();
        int iters = runKMeans(data, centroids, MAX_ITERS, TOLERANCE);
        double elapsed = omp_get_wtime() - t0;

        char correctness[64];
        if (t == 0) {
            baseline = elapsed;
            reference = centroids;
            snprintf(correctness, sizeof(correctness), "reference");
        } else {
            int diffs = compareCentroids(reference, centroids, CENTROID_MATCH_TOL);
            if (diffs == 0)
                snprintf(correctness, sizeof(correctness), "OK (matches T=1)");
            else if (diffs < 0)
                snprintf(correctness, sizeof(correctness), "K MISMATCH");
            else
                snprintf(correctness, sizeof(correctness),
                         "FAIL (%d centroid%s off)", diffs, diffs == 1 ? "" : "s");
            freeCentroids(centroids);
        }

        writeRow(stdout, threads, elapsed, iters, baseline, correctness);
        writeRow(log,    threads, elapsed, iters, baseline, correctness);
        fflush(stdout);
        fflush(log);
    }

    freeCentroids(reference);
    freePointSet(data);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    int count = 0;
    char **inputs = listInputs(INPUT_DIR, &count);
    if (count == 0) {
        fprintf(stderr,
                "No .csv inputs found in '%s/'.\n"
                "Run python3 inputs/generate_inputs.py to populate it.\n",
                INPUT_DIR);
        free(inputs);
        return 1;
    }

    FILE *log = fopen("scalability.log", "w");
    if (log == NULL) {
        fprintf(stderr, "Could not open scalability.log for writing\n");
        return 1;
    }

    fprintf(log, "K-means scalability + correctness report\n");
    fprintf(log, "maxIters=%d, tolerance=%g, centroid-match tolerance=%g\n",
            MAX_ITERS, TOLERANCE, CENTROID_MATCH_TOL);
    fprintf(log, "Max OMP threads available: %d\n", omp_get_max_threads());
    fprintf(log, "Inputs scanned: %d in %s/\n", count, INPUT_DIR);

    for (int i = 0; i < count; i++) {
        benchmarkInput(inputs[i], log);
        free(inputs[i]);
    }
    free(inputs);

    fclose(log);
    printf("\nResults written to scalability.log\n");
    return 0;
}
