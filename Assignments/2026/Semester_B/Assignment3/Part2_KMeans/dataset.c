#include "dataset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int parseHeader(const char *line, const char *key) {
    /* Match "# <key>=<int>" with arbitrary whitespace. Returns -1 on miss. */
    const char *p = line;
    while (*p == '#' || isspace((unsigned char)*p)) p++;
    size_t klen = strlen(key);
    if (strncmp(p, key, klen) != 0) return -1;
    p += klen;
    while (isspace((unsigned char)*p)) p++;
    if (*p != '=') return -1;
    p++;
    while (isspace((unsigned char)*p)) p++;
    return atoi(p);
}

PointSet *loadDataset(const char *filename, int *out_k) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "loadDataset: cannot open %s\n", filename);
        return NULL;
    }

    int k = -1, n = -1;
    char line[256];

    /* First scan headers (we need n before allocating). */
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') {
            int v;
            if ((v = parseHeader(line, "k")) >= 0) k = v;
            if ((v = parseHeader(line, "n")) >= 0) n = v;
        }
    }

    if (k <= 0 || n <= 0) {
        fprintf(stderr, "loadDataset: %s missing # k= or # n= header\n", filename);
        fclose(f);
        return NULL;
    }

    PointSet *data = createPointSet(n);

    /* Re-read for data lines. */
    rewind(f);
    int read = 0;
    while (fgets(line, sizeof(line), f) && read < n) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        double x, y;
        if (sscanf(line, "%lf %lf", &x, &y) == 2) {
            data->points[read].x = x;
            data->points[read].y = y;
            read++;
        }
    }
    fclose(f);

    if (read != n) {
        fprintf(stderr, "loadDataset: %s -- expected %d points, got %d\n",
                filename, n, read);
        freePointSet(data);
        return NULL;
    }

    *out_k = k;
    return data;
}

Centroids *initCentroidsEvenly(const PointSet *data, int k) {
    Centroids *c = createCentroids(k);
    int n = data->numPoints;
    for (int i = 0; i < k; i++) {
        int idx = (int)((long long)i * n / k);
        if (idx >= n) idx = n - 1;
        c->centroids[i] = data->points[idx];
    }
    return c;
}

