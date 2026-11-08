/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "rng.h"
#include <math.h>

#define TWO_PI 6.283185307179586

void rng_seed(Rng *r, uint64_t seed) { r->state = seed; }

static uint64_t next_u64(Rng *r)
{
    uint64_t z = (r->state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

double rng_uniform(Rng *r) { return (next_u64(r) >> 11) * (1.0 / 9007199254740992.0); }

double rng_normal(Rng *r, double mean, double sigma)
{
    if (sigma == 0.0) return mean;
    double u1 = 1.0 - rng_uniform(r); /* (0, 1] avoids log(0) */
    double u2 = rng_uniform(r);
    return mean + sigma * sqrt(-2.0 * log(u1)) * cos(TWO_PI * u2);
}
