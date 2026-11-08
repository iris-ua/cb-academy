/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SIM_RNG_H
#define SIM_RNG_H

#include <stdint.h>

/* Seedable, platform-independent RNG (splitmix64) so desktop and web runs match. */
typedef struct { uint64_t state; } Rng;

void   rng_seed(Rng *r, uint64_t seed);
double rng_uniform(Rng *r);                          /* [0, 1) */
double rng_normal(Rng *r, double mean, double sigma); /* Box-Muller */

#endif
