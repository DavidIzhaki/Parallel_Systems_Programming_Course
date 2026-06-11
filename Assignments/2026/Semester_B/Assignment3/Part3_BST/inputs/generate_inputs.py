#!/usr/bin/env python3
"""
Re-runnable: generates operation streams for the concurrent BST benchmark.

Two modes per the assignment spec:

  fromempty
      Tree starts empty.
      Stream contains INS (unique keys) + FIND + MIN. No DEL.
      The first attack op is always INS of a "root-anchor" key so the
      root pointer is stable across the parallel section.

  buildthenattack
      File begins with INIT_INS lines that the harness applies serially
      to pre-build a tree, then an attack section with INS / DEL / FIND
      / MIN where every write op targets a unique key not currently
      racing with another write (and DEL never targets the root).

Both modes guarantee that the parallel attack section consists of
commuting operations on distinct keys, so the final tree state is
deterministic and serial = parallel.

Run once to populate inputs/:
    python3 inputs/generate_inputs.py
"""

import os
import random


def write_header(f, mode, n):
    f.write(f"# mode={mode}\n")
    f.write(f"# n={n}\n")


# ----- fromempty -----

def make_fromempty(n_ops, seed, ins_frac=0.80, find_frac=0.15):
    """Stream: INS (unique) + FIND + MIN.
    Returns (lines, expected_size). expected_size is for sanity only.
    """
    rng = random.Random(seed)
    n_ins   = int(n_ops * ins_frac)
    n_find  = int(n_ops * find_frac)
    n_min   = n_ops - n_ins - n_find

    # Unique INS keys, drawn from a large enough range
    ins_keys = rng.sample(range(1, 10 * n_ins + 1), n_ins)

    lines = []
    inserted_so_far = []
    # First op is INS(root-anchor): pick a middle-ish value
    root_anchor = ins_keys[0]
    lines.append(f"INS {root_anchor}")
    inserted_so_far.append(root_anchor)

    remaining_ins  = ins_keys[1:]
    remaining_find = n_find
    remaining_min  = n_min

    while remaining_ins or remaining_find or remaining_min:
        choices = []
        if remaining_ins:   choices.append(('INS',  len(remaining_ins)))
        if remaining_find:  choices.append(('FIND', remaining_find))
        if remaining_min:   choices.append(('MIN',  remaining_min))
        # weighted choice
        kinds, weights = zip(*choices)
        kind = rng.choices(kinds, weights=weights, k=1)[0]
        if kind == 'INS':
            v = remaining_ins.pop()
            inserted_so_far.append(v)
            lines.append(f"INS {v}")
        elif kind == 'FIND':
            # 50/50 hit vs miss
            if inserted_so_far and rng.random() < 0.5:
                v = rng.choice(inserted_so_far)
            else:
                v = rng.randint(10 * n_ins + 1, 20 * n_ins + 2)
            lines.append(f"FIND {v}")
            remaining_find -= 1
        else:
            lines.append("MIN")
            remaining_min -= 1

    return lines, len(inserted_so_far)


# ----- buildthenattack -----

def make_buildthenattack(n_init, n_attack, seed,
                         ins_frac=0.30, del_frac=0.30, find_frac=0.35):
    """Pre-populate with n_init unique keys, then run an attack
    of n_attack ops in which every write op targets a unique key
    that does NOT collide with any other write op and DEL never
    targets the root.
    """
    rng = random.Random(seed)

    # init keys: 1..n_init, shuffled (insertion order randomized -> realistic tree shape)
    init_keys = list(range(1, n_init + 1))
    rng.shuffle(init_keys)
    root_key = init_keys[0]   # this is the root of the serially-built tree

    n_ins  = int(n_attack * ins_frac)
    n_del  = int(n_attack * del_frac)
    n_find = int(n_attack * find_frac)
    n_min  = n_attack - n_ins - n_del - n_find

    # Attack INS keys: drawn from above the init range, unique
    attack_ins_keys = rng.sample(range(n_init + 1, n_init + 10 * n_ins + 1), n_ins)
    # Attack DEL keys: drawn from init, unique, NEVER the root
    eligible_for_del = [k for k in init_keys if k != root_key]
    rng.shuffle(eligible_for_del)
    attack_del_keys = eligible_for_del[:n_del]

    # Build init lines first
    lines = [f"INIT_INS {k}" for k in init_keys]

    # Pool of FIND values: 50/50 hit (init or attack INS) vs. miss
    all_init_set = init_keys
    miss_pool_lo = n_init + 10 * n_ins + 1
    miss_pool_hi = miss_pool_lo + 10 * n_ins + 1000

    attack_lines = []
    remaining_ins   = list(attack_ins_keys)
    remaining_del   = list(attack_del_keys)
    remaining_find  = n_find
    remaining_min   = n_min

    while remaining_ins or remaining_del or remaining_find or remaining_min:
        choices = []
        if remaining_ins:  choices.append(('INS',  len(remaining_ins)))
        if remaining_del:  choices.append(('DEL',  len(remaining_del)))
        if remaining_find: choices.append(('FIND', remaining_find))
        if remaining_min:  choices.append(('MIN',  remaining_min))
        kinds, weights = zip(*choices)
        kind = rng.choices(kinds, weights=weights, k=1)[0]
        if kind == 'INS':
            attack_lines.append(f"INS {remaining_ins.pop()}")
        elif kind == 'DEL':
            attack_lines.append(f"DEL {remaining_del.pop()}")
        elif kind == 'FIND':
            if rng.random() < 0.5:
                v = rng.choice(all_init_set)
            else:
                v = rng.randint(miss_pool_lo, miss_pool_hi)
            attack_lines.append(f"FIND {v}")
            remaining_find -= 1
        else:
            attack_lines.append("MIN")
            remaining_min -= 1

    return lines + attack_lines


def save(path, lines, mode, n):
    with open(path, "w") as f:
        write_header(f, mode, n)
        f.write("\n".join(lines))
        f.write("\n")
    size_kb = os.path.getsize(path) / 1024.0
    print(f"  wrote {os.path.basename(path)}  "
          f"({len(lines)} ops, {size_kb:.1f} KB)")


def main():
    here = os.path.dirname(os.path.abspath(__file__))

    # fromempty tier. Bumped so serial takes long enough that parallel
    # wins are visible against background OMP overhead.
    fromempty_specs = [
        ("ops_fromempty_small.txt",   10_000,     101),
        ("ops_fromempty_medium.txt",  500_000,    102),
        ("ops_fromempty_large.txt",   5_000_000,  103),
        ("ops_fromempty_heavy.txt",   20_000_000, 104),
    ]
    for filename, n, seed in fromempty_specs:
        lines, _size = make_fromempty(n, seed)
        save(os.path.join(here, filename), lines, "fromempty", n)

    # buildthenattack tier: bigger init tree => deeper walks => more
    # per-op work that the parallel readers can actually parallelize.
    bta_specs = [
        ("ops_bta_small.txt",     10_000,     10_000,     201),
        ("ops_bta_medium.txt",    500_000,    500_000,    202),
        ("ops_bta_large.txt",     2_000_000,  5_000_000,  203),
        ("ops_bta_heavy.txt",     5_000_000,  20_000_000, 204),
    ]
    for filename, n_init, n_attack, seed in bta_specs:
        lines = make_buildthenattack(n_init, n_attack, seed)
        save(os.path.join(here, filename), lines, "buildthenattack", n_attack)

    print("Done.")


if __name__ == "__main__":
    main()
