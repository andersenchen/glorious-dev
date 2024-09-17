// probability.h

#ifndef PROBABILITY_H
#define PROBABILITY_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Example implementation of a fixed-point probability function based on
 * context.
 *
 * This function calculates the probability of the current bit being '1'
 * based on the number of '1's in the given context. It employs smoothing
 * by adding 1 to the numerator and 2 to the denominator to avoid probabilities
 * of exactly 0 or 1.
 *
 * @param context Pointer to the context bit array.
 * @param context_length Length of the context in bits.
 * @return uint32_t Fixed-point probability of the current bit being '1',
 * scaled by FIXED_SCALE.
 */
uint32_t example_get_probability_fixed(const uint8_t *context,
                                       size_t context_length);

#endif  // PROBABILITY_H
