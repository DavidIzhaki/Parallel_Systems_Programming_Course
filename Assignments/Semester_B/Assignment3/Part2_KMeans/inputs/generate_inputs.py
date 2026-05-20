#!/usr/bin/env python3
"""
Re-runnable: generates 2D blob datasets for the K-means benchmark.

Each output file is a plain CSV-ish text file:
    # k=<int>
    # n=<int>
    x y
    x y
    ...

Seeds are fixed so the same script always produces the same files.
Run this once; commit the .csv files alongside it.

    cd Part2_KMeans/inputs
    python3 generate_inputs.py
"""

import math
import os
import random


def make_blobs(n, k, seed, span=1000.0, spread_factor=1.5):
    """Generate k blob clusters in [0, span]^2.

    spread_factor controls how wide each blob is relative to the grid
    step between cluster centers:
      < 1.0  -> blobs are well-separated (K-means converges fast)
      = 1.0  -> blobs just touch
      > 1.0  -> blobs OVERLAP, boundary points keep flipping clusters,
                K-means takes many more iterations.
    1.5 gives ~50% overlap with adjacent blobs, which is plenty hard.
    """
    rng = random.Random(seed)
    grid = int(math.ceil(math.sqrt(k)))
    step = span / (grid + 1)
    spread = step * spread_factor
    centers = [(step * (i % grid + 1), step * (i // grid + 1)) for i in range(k)]
    points = []
    for i in range(n):
        c = i % k
        cx, cy = centers[c]
        px = cx + (rng.random() - 0.5) * spread
        py = cy + (rng.random() - 0.5) * spread
        points.append((px, py))
    return points


def save(filename, points, k):
    with open(filename, "w") as f:
        f.write(f"# k={k}\n")
        f.write(f"# n={len(points)}\n")
        for x, y in points:
            f.write(f"{x:.4f} {y:.4f}\n")
    size_kb = os.path.getsize(filename) / 1024.0
    print(f"  wrote {filename}  ({len(points)} points, k={k}, {size_kb:.1f} KB)")


def main():
    here = os.path.dirname(os.path.abspath(__file__))

    datasets = [
        ("blobs_small_k4.csv",              5_000,     4,  1),
        ("blobs_medium_k8.csv",             50_000,    8,  2),
        ("blobs_large_k16.csv",             200_000,   16, 3),
        ("blobs_heavy_k32.csv",             1_000_000, 32, 4),
        ("blobs_heavy_heavy_k48.csv",       2_500_000, 48, 5),
        ("blobs_heavy_heavy_heavy_k64.csv", 5_000_000, 64, 6),
    ]

    for filename, n, k, seed in datasets:
        path = os.path.join(here, filename)
        save(path, make_blobs(n, k, seed), k)

    print("Done.")


if __name__ == "__main__":
    main()
