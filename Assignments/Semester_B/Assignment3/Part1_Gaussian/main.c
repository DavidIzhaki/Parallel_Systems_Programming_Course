/*
 * Scalability + correctness harness for the Gaussian blur.
 *
 * Iterates over every image in ./images/ and for each one runs
 * createBlurredImage at THREAD_COUNTS = {1,2,4,8,16}. For each image
 * it prints a Threads | Time | Speedup | Efficiency | Correctness
 * table to stdout and appends the same table to scalability.log.
 *
 * The T=1 result is used as the reference; subsequent thread counts
 * are diffed against it (interior pixels only -- the algorithm leaves
 * the border uninitialized).
 *
 * The blurred output for each image is saved to images/output/ with
 * the input borders copied in so the PNG has no garbage frame.
 *
 * This file is provided so you can verify that your parallel
 * implementation of guassonFilter.c actually scales AND stays correct.
 * It is NOT part of the submission -- submit only guassonFilter.c.
 *
 * Build (on the BIU servers):
 *     gcc -O2 -fopenmp main.c guassonFilter.c -o gauss_bench -lm
 * Populate images (one-time):
 *     ./images/fetch_images.sh
 * Run:
 *     ./gauss_bench
 */

#include "guassonFilter.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

static const int THREAD_COUNTS[] = {1, 2, 4, 8, 16};
static const int NUM_THREAD_COUNTS = (int)(sizeof(THREAD_COUNTS) / sizeof(THREAD_COUNTS[0]));

static const int BLUR_RADIUS = 7;
static const int PIXEL_TOLERANCE = 1;

static const char *INPUT_DIR  = "images";
static const char *OUTPUT_DIR = "images/output";

/* ------------------------------------------------------------------ */
/* image helpers                                                      */
/* ------------------------------------------------------------------ */

static void freeImage(Image *image) {
    if (image == NULL) return;
    free(image->pixels);
    free(image);
}

/* createBlurredImage leaves a `border`-wide frame around the output
 * uninitialized. Copy those border pixels from the input before
 * saving so the PNG has no garbage frame. */
static void fillBorderFromInput(Image *out, const Image *in, int border) {
    int w = out->width;
    int h = out->height;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (y < border || y >= h - border || x < border || x >= w - border) {
                out->pixels[y * w + x] = in->pixels[y * w + x];
            }
        }
    }
}

/* Diff two outputs over the interior region only; returns the number
 * of pixels that differ by more than PIXEL_TOLERANCE on any channel. */
static long compareInterior(const Image *a, const Image *b, int border) {
    if (a->width != b->width || a->height != b->height) return -1;
    long diffs = 0;
    int w = a->width;
    for (int y = border; y < a->height - border; y++) {
        for (int x = border; x < w - border; x++) {
            int i = y * w + x;
            int dr = (int)a->pixels[i].r - (int)b->pixels[i].r;
            int dg = (int)a->pixels[i].g - (int)b->pixels[i].g;
            int db = (int)a->pixels[i].b - (int)b->pixels[i].b;
            if (abs(dr) > PIXEL_TOLERANCE ||
                abs(dg) > PIXEL_TOLERANCE ||
                abs(db) > PIXEL_TOLERANCE) {
                diffs++;
            }
        }
    }
    return diffs;
}

/* Sparse spot-check: did the blur actually change the image, or did
 * the student return the input unchanged? */
static bool blurActuallyHappened(const Image *input, const Image *blurred, int border) {
    int w = input->width;
    long changed = 0, sampled = 0;
    for (int y = border; y < input->height - border; y += 8) {
        for (int x = border; x < w - border; x += 8) {
            int i = y * w + x;
            sampled++;
            if (input->pixels[i].r != blurred->pixels[i].r ||
                input->pixels[i].g != blurred->pixels[i].g ||
                input->pixels[i].b != blurred->pixels[i].b) {
                changed++;
            }
        }
    }
    return sampled > 0 && changed * 4 > sampled;
}

/* ------------------------------------------------------------------ */
/* directory scanning                                                 */
/* ------------------------------------------------------------------ */

static bool hasImageExtension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return false;
    return strcasecmp(dot, ".jpg")  == 0 ||
           strcasecmp(dot, ".jpeg") == 0 ||
           strcasecmp(dot, ".png")  == 0 ||
           strcasecmp(dot, ".bmp")  == 0;
}

/* qsort comparator for stable, alphabetical processing order. */
static int strptrcmp(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Collect image filenames from INPUT_DIR. Caller owns the returned
 * array and each string. Returns the count via *out_count. */
static char **listImages(const char *dir, int *out_count) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        *out_count = 0;
        return NULL;
    }

    int cap = 16, n = 0;
    char **names = (char **)malloc((size_t)cap * sizeof(char *));

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;       /* skip ./, ../, hidden */
        if (!hasImageExtension(entry->d_name)) continue;

        if (n == cap) {
            cap *= 2;
            names = (char **)realloc(names, (size_t)cap * sizeof(char *));
        }
        names[n++] = strdup(entry->d_name);
    }
    closedir(d);

    qsort(names, n, sizeof(char *), strptrcmp);
    *out_count = n;
    return names;
}

/* ------------------------------------------------------------------ */
/* table printing                                                     */
/* ------------------------------------------------------------------ */

static void writeHeader(FILE *out, const char *name, int w, int h) {
    fprintf(out, "\n=== %s (%dx%d, radius=%d) ===\n", name, w, h, BLUR_RADIUS);
    fprintf(out, "%-8s | %-10s | %-8s | %-10s | %s\n",
            "Threads", "Time(s)", "Speedup", "Efficiency", "Correctness");
    fprintf(out, "---------+------------+----------+------------+----------------------\n");
}

static void writeRow(FILE *out, int threads, double seconds, double baseline,
                     const char *correctness) {
    double speedup = baseline / seconds;
    double efficiency = speedup / (double)threads;
    fprintf(out, "%-8d | %-10.4f | %-8.3f | %-10.3f | %s\n",
            threads, seconds, speedup, efficiency, correctness);
}

/* ------------------------------------------------------------------ */
/* per-image benchmark                                                */
/* ------------------------------------------------------------------ */

static void benchmarkImage(const char *filename, FILE *log) {
    char inPath[512];
    snprintf(inPath, sizeof(inPath), "%s/%s", INPUT_DIR, filename);

    Image *input = loadImage(inPath);
    if (input == NULL) {
        fprintf(stderr, "Could not load %s -- skipping\n", inPath);
        return;
    }

    writeHeader(stdout, filename, input->width, input->height);
    writeHeader(log,    filename, input->width, input->height);

    double baseline = 0.0;
    Image *reference = NULL;

    for (int t = 0; t < NUM_THREAD_COUNTS; t++) {
        int threads = THREAD_COUNTS[t];
        omp_set_num_threads(threads);

        double t0 = omp_get_wtime();
        Image *blurred = createBlurredImage(BLUR_RADIUS, input);
        double elapsed = omp_get_wtime() - t0;

        char correctness[64];
        if (t == 0) {
            baseline = elapsed;
            reference = blurred;
            snprintf(correctness, sizeof(correctness),
                     blurActuallyHappened(input, blurred, BLUR_RADIUS)
                     ? "reference (blur OK)"
                     : "reference (NO BLUR?)");
        } else {
            long diffs = compareInterior(reference, blurred, BLUR_RADIUS);
            if (diffs == 0)
                snprintf(correctness, sizeof(correctness), "OK (matches T=1)");
            else if (diffs < 0)
                snprintf(correctness, sizeof(correctness), "SIZE MISMATCH");
            else
                snprintf(correctness, sizeof(correctness), "FAIL (%ld px differ)", diffs);
        }

        writeRow(stdout, threads, elapsed, baseline, correctness);
        writeRow(log,    threads, elapsed, baseline, correctness);
        fflush(stdout);
        fflush(log);

        if (blurred != reference) {
            freeImage(blurred);
        }
    }

    /* Save blurred reference output (clean border) for visual check. */
    fillBorderFromInput(reference, input, BLUR_RADIUS);

    char outPath[512];
    /* Strip extension, append _blurred.png. */
    char base[256];
    strncpy(base, filename, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    snprintf(outPath, sizeof(outPath), "%s/%s_blurred.png", OUTPUT_DIR, base);
    saveImage(outPath, reference);

    freeImage(reference);
    freeImage(input);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    mkdir(OUTPUT_DIR, 0755);   /* harmless if it already exists */

    int imageCount = 0;
    char **images = listImages(INPUT_DIR, &imageCount);
    if (imageCount == 0) {
        fprintf(stderr,
                "No images found in '%s/'.\n"
                "Run ./images/fetch_images.sh to populate it, or drop your\n"
                "own .jpg / .png files in there.\n",
                INPUT_DIR);
        free(images);
        return 1;
    }

    FILE *log = fopen("scalability.log", "w");
    if (log == NULL) {
        fprintf(stderr, "Could not open scalability.log for writing\n");
        return 1;
    }

    fprintf(log, "Gaussian Blur scalability + correctness report\n");
    fprintf(log, "Blur radius: %d, pixel tolerance: %d\n", BLUR_RADIUS, PIXEL_TOLERANCE);
    fprintf(log, "Max OMP threads available: %d\n", omp_get_max_threads());
    fprintf(log, "Images scanned: %d in %s/\n", imageCount, INPUT_DIR);

    for (int i = 0; i < imageCount; i++) {
        benchmarkImage(images[i], log);
        free(images[i]);
    }
    free(images);

    fclose(log);
    printf("\nResults written to scalability.log\n");
    printf("Blurred outputs in %s/\n", OUTPUT_DIR);
    return 0;
}
