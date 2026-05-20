/*
 * Scalability + correctness harness for the concurrent BST.
 *
 * Scans inputs/, runs each .txt operations file twice:
 *   - serially (no OMP tasks)   -> reference final tree
 *   - in parallel via #pragma omp task at 1/2/4/8/16 threads
 *
 * Correctness check: in-order traversal of the parallel tree must
 * equal the in-order traversal of the serial tree. The inputs are
 * generated so this equality always holds when the student's
 * synchronization is correct (see Assignment3.pdf for the
 * commutativity argument).
 *
 * This file is for testing only -- DO NOT submit it. Submit only
 * binary_tree.c (and binary_tree.h if you modified it).
 *
 * Build (on the BIU servers):
 *     gcc -O2 -fopenmp main.c binary_tree.c -o bst_bench -lm
 * Populate inputs (one-time):
 *     python3 inputs/generate_inputs.py
 * Run:
 *     ./bst_bench
 */

#include "binary_tree.h"

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

static const char *INPUT_DIR = "inputs";

/* ------------------------------------------------------------------ */
/* operation representation                                           */
/* ------------------------------------------------------------------ */

typedef enum { OP_INS, OP_DEL, OP_FIND, OP_MIN, OP_INIT_INS } OpKind;

typedef struct {
    OpKind kind;
    int    key;        /* unused for OP_MIN */
} Op;

typedef struct {
    char *name;            /* basename of input file */
    char *mode;            /* "fromempty" or "buildthenattack" */
    int   nInit;           /* count of INIT_INS ops (front of array) */
    int   nAttack;         /* count of attack ops (the rest) */
    Op   *ops;             /* nInit + nAttack entries */
} Workload;

static void freeWorkload(Workload *w) {
    if (!w) return;
    free(w->name);
    free(w->mode);
    free(w->ops);
    free(w);
}

/* ------------------------------------------------------------------ */
/* file parsing                                                       */
/* ------------------------------------------------------------------ */

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    memcpy(p, s, n);
    return p;
}

static Workload *parseWorkload(const char *path, const char *basename) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "open %s failed\n", path); return NULL; }

    char  *mode = NULL;
    int    nDeclared = 0;
    int    cap = 1024, count = 0;
    Op    *ops = (Op *)malloc((size_t)cap * sizeof(Op));
    int    nInit = 0;
    bool   seenAttack = false;     /* once we see a non-INIT op, all subsequent are attack */

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* trim trailing newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (line[0] == '\0') continue;
        if (line[0] == '#') {
            const char *p = line + 1;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "mode=", 5) == 0) {
                free(mode);
                mode = xstrdup(p + 5);
            } else if (strncmp(p, "n=", 2) == 0) {
                nDeclared = atoi(p + 2);
            }
            continue;
        }

        Op op;
        if (strncmp(line, "INIT_INS", 8) == 0) {
            op.kind = OP_INIT_INS;
            op.key  = atoi(line + 9);
            if (seenAttack) {
                fprintf(stderr, "%s: INIT_INS after attack op (line dropped)\n", path);
                continue;
            }
            nInit++;
        } else if (strncmp(line, "INS", 3) == 0) {
            op.kind = OP_INS;  op.key = atoi(line + 4); seenAttack = true;
        } else if (strncmp(line, "DEL", 3) == 0) {
            op.kind = OP_DEL;  op.key = atoi(line + 4); seenAttack = true;
        } else if (strncmp(line, "FIND", 4) == 0) {
            op.kind = OP_FIND; op.key = atoi(line + 5); seenAttack = true;
        } else if (strncmp(line, "MIN", 3) == 0) {
            op.kind = OP_MIN;  op.key = 0;               seenAttack = true;
        } else {
            continue;
        }

        if (count == cap) {
            cap *= 2;
            ops = (Op *)realloc(ops, (size_t)cap * sizeof(Op));
        }
        ops[count++] = op;
    }
    fclose(f);

    if (!mode) mode = xstrdup("fromempty");

    Workload *w = (Workload *)malloc(sizeof(Workload));
    w->name    = xstrdup(basename);
    w->mode    = mode;
    w->nInit   = nInit;
    w->nAttack = count - nInit;
    w->ops     = ops;
    (void)nDeclared;
    return w;
}

/* ------------------------------------------------------------------ */
/* runners                                                            */
/* ------------------------------------------------------------------ */

/* Apply one op to the tree. Returns updated root (or current root for
 * read-only ops). Always called single-threaded for setup. */
static TreeNode *applyOp(TreeNode *root, Op op) {
    switch (op.kind) {
        case OP_INIT_INS:
        case OP_INS:  return insertNode(root, op.key);
        case OP_DEL:  return deleteNode(root, op.key);
        case OP_FIND: (void)searchNode(root, op.key); return root;
        case OP_MIN:  (void)findMin(root);            return root;
    }
    return root;
}

/* Build the init tree serially (uses INIT_INS ops if any). */
static TreeNode *buildInit(const Workload *w) {
    TreeNode *root = NULL;
    for (int i = 0; i < w->nInit; i++) {
        root = applyOp(root, w->ops[i]);
    }
    return root;
}

/* Serial reference run: apply attack ops in order. */
static TreeNode *runSerial(const Workload *w, double *out_seconds) {
    TreeNode *root = buildInit(w);
    /* For fromempty mode we still need a root before parallel can be
     * useful. The generator puts a single "root-anchor" INS as the
     * first attack op so root identity is stable; we apply that one
     * serially so the parallel version sees the same starting point. */
    int parallelStart = w->nInit;
    if (strcmp(w->mode, "fromempty") == 0 && w->nAttack > 0) {
        root = applyOp(root, w->ops[parallelStart]);
        parallelStart++;
    }
    double t0 = omp_get_wtime();
    for (int i = parallelStart; i < w->nInit + w->nAttack; i++) {
        root = applyOp(root, w->ops[i]);
    }
    *out_seconds = omp_get_wtime() - t0;
    return root;
}

/* Parallel run: dispatch each attack op as an omp task. The init
 * section (and the root-anchor op for fromempty) runs serially first
 * so that root identity is stable across the parallel section. */
static TreeNode *runParallel(const Workload *w, int threads, double *out_seconds) {
    omp_set_num_threads(threads);
    TreeNode *root = buildInit(w);

    int parallelStart = w->nInit;
    if (strcmp(w->mode, "fromempty") == 0 && w->nAttack > 0) {
        root = applyOp(root, w->ops[parallelStart]);
        parallelStart++;
    }

    /* All concurrent tasks read the same `root` pointer. The
     * generator guarantees the root key is never DELed in the attack
     * section, so root identity is preserved by all ops. */
    int total = w->nInit + w->nAttack;
    const Op *ops = w->ops;

    /* Distribute the op array across the thread team with
     * schedule(static). Equivalent semantics to "fire each op as its
     * own concurrent call" -- multiple threads hammer the tree at
     * once -- but with no per-op task dispatch overhead. Each op is
     * ~270 ns of work, far less than OMP's ~1 us task creation cost,
     * so we MUST avoid one-task-per-op or we benchmark the runtime,
     * not the student's locking. */
    double t0 = omp_get_wtime();
    #pragma omp parallel for schedule(static)
    for (int i = parallelStart; i < total; i++) {
        Op op = ops[i];
        switch (op.kind) {
            case OP_INS:  insertNode(root, op.key); break;
            case OP_DEL:  deleteNode(root, op.key); break;
            case OP_FIND: searchNode(root, op.key); break;
            case OP_MIN:  findMin(root);            break;
            default: break;
        }
    }
    *out_seconds = omp_get_wtime() - t0;
    return root;
}

/* ------------------------------------------------------------------ */
/* in-order capture and comparison                                    */
/* ------------------------------------------------------------------ */

/* In-order walk into a growable int array (no I/O, no printf). */
static void collectInorder(TreeNode *root, int **buf, size_t *n, size_t *cap) {
    if (!root) return;
    collectInorder(root->left, buf, n, cap);
    if (*n == *cap) {
        *cap = (*cap == 0) ? 1024 : *cap * 2;
        *buf = (int *)realloc(*buf, *cap * sizeof(int));
    }
    (*buf)[(*n)++] = root->data;
    collectInorder(root->right, buf, n, cap);
}

/* Returns 0 if in-orders match, otherwise the number of differing
 * positions (or -1 if sizes differ). */
static long compareInorder(TreeNode *a, TreeNode *b, size_t *out_size) {
    int *ba = NULL, *bb = NULL;
    size_t na = 0, nb = 0, ca = 0, cb = 0;
    collectInorder(a, &ba, &na, &ca);
    collectInorder(b, &bb, &nb, &cb);
    *out_size = na;
    if (na != nb) {
        free(ba); free(bb);
        return -1;
    }
    long diffs = 0;
    for (size_t i = 0; i < na; i++) if (ba[i] != bb[i]) diffs++;
    free(ba); free(bb);
    return diffs;
}

/* ------------------------------------------------------------------ */
/* directory scan                                                     */
/* ------------------------------------------------------------------ */

static bool hasTxtExtension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ".txt") == 0;
}

static int cmpBySize(const void *a, const void *b) {
    struct stat sa, sb;
    char pa[512], pb[512];
    snprintf(pa, sizeof(pa), "%s/%s", INPUT_DIR, *(const char **)a);
    snprintf(pb, sizeof(pb), "%s/%s", INPUT_DIR, *(const char **)b);
    if (stat(pa, &sa) != 0 || stat(pb, &sb) != 0) return 0;
    return (sa.st_size > sb.st_size) - (sa.st_size < sb.st_size);
}

static char **listInputs(int *out_count) {
    DIR *d = opendir(INPUT_DIR);
    if (!d) { *out_count = 0; return NULL; }
    int cap = 16, n = 0;
    char **names = (char **)malloc((size_t)cap * sizeof(char *));
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (!hasTxtExtension(e->d_name)) continue;
        if (n == cap) { cap *= 2; names = realloc(names, (size_t)cap * sizeof(char *)); }
        names[n++] = xstrdup(e->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(char *), cmpBySize);
    *out_count = n;
    return names;
}

/* ------------------------------------------------------------------ */
/* per-input benchmark                                                */
/* ------------------------------------------------------------------ */

static void benchmarkInput(const char *basename, FILE *log) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, basename);
    Workload *w = parseWorkload(path, basename);
    if (!w) return;

    fprintf(stdout, "\n=== %s (mode=%s, init=%d, attack=%d) ===\n",
            w->name, w->mode, w->nInit, w->nAttack);
    fprintf(log,    "\n=== %s (mode=%s, init=%d, attack=%d) ===\n",
            w->name, w->mode, w->nInit, w->nAttack);
    fprintf(stdout, "%-8s | %-10s | %-8s | %-10s | %s\n",
            "Threads", "Time(s)", "Speedup", "Efficiency", "Correctness");
    fprintf(log,    "%-8s | %-10s | %-8s | %-10s | %s\n",
            "Threads", "Time(s)", "Speedup", "Efficiency", "Correctness");
    fprintf(stdout, "---------+------------+----------+------------+----------------------\n");
    fprintf(log,    "---------+------------+----------+------------+----------------------\n");
    fflush(stdout); fflush(log);

    /* Reference: serial run. */
    double serialSec;
    TreeNode *refTree = runSerial(w, &serialSec);

    fprintf(stdout, "%-8s | %-10.4f | %-8s | %-10s | reference\n",
            "serial", serialSec, "1.000", "--");
    fprintf(log,    "%-8s | %-10.4f | %-8s | %-10s | reference\n",
            "serial", serialSec, "1.000", "--");
    fflush(stdout); fflush(log);

    /* Parallel runs at each thread count. */
    for (int t = 0; t < NUM_THREAD_COUNTS; t++) {
        int threads = THREAD_COUNTS[t];

        double parSec;
        TreeNode *parTree = runParallel(w, threads, &parSec);

        size_t sz = 0;
        long diffs = compareInorder(refTree, parTree, &sz);
        char correctness[64];
        if (diffs == 0)      snprintf(correctness, sizeof(correctness),
                                      "OK (inorder match, %zu nodes)", sz);
        else if (diffs < 0)  snprintf(correctness, sizeof(correctness), "SIZE MISMATCH");
        else                 snprintf(correctness, sizeof(correctness),
                                      "FAIL (%ld positions differ)", diffs);

        double speedup    = serialSec / parSec;
        double efficiency = speedup / (double)threads;

        fprintf(stdout, "%-8d | %-10.4f | %-8.3f | %-10.3f | %s\n",
                threads, parSec, speedup, efficiency, correctness);
        fprintf(log,    "%-8d | %-10.4f | %-8.3f | %-10.3f | %s\n",
                threads, parSec, speedup, efficiency, correctness);
        fflush(stdout); fflush(log);

        freeTree(parTree);
    }

    freeTree(refTree);
    freeWorkload(w);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    int n = 0;
    char **inputs = listInputs(&n);
    if (n == 0) {
        fprintf(stderr,
                "No .txt inputs found in '%s/'.\n"
                "Run python3 inputs/generate_inputs.py to populate it.\n",
                INPUT_DIR);
        free(inputs);
        return 1;
    }

    FILE *log = fopen("scalability.log", "w");
    if (!log) { perror("scalability.log"); return 1; }
    fprintf(log, "BST scalability + correctness report\n");
    fprintf(log, "Max OMP threads available: %d\n", omp_get_max_threads());
    fprintf(log, "Inputs scanned: %d in %s/\n", n, INPUT_DIR);

    for (int i = 0; i < n; i++) {
        benchmarkInput(inputs[i], log);
        free(inputs[i]);
    }
    free(inputs);
    fclose(log);
    printf("\nResults written to scalability.log\n");
    return 0;
}
