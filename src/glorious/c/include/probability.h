// probability.h

#ifndef PROBABILITY_H
#define PROBABILITY_H

#include <stddef.h>
#include <stdint.h>

#include "context.h"  // Include the ContextContent definition

/**
 * @brief Example implementation of a fixed-point probability function based on
 * ContextContent.
 *
 * This function calculates the probability of the current bit being '1'
 * based on the count of '1's in the given context. It employs smoothing
 * by adding 1 to the numerator and 2 to the denominator to avoid probabilities
 * of exactly 0 or 1.
 *
 * @param context_content Pointer to the ContextContent struct.
 * @return uint32_t Fixed-point probability of the current bit being '1',
 * scaled by FIXED_SCALE.
 */
uint32_t example_get_probability_fixed(const ContextContent *context_content);

#endif  // PROBABILITY_H
