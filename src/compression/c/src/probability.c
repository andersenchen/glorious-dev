// probability.c

#include "probability.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arithmetic_coding.h"  // For FIXED_SCALE

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
                                       size_t context_length) {
  if (context_length == 0) {
    // If no context is used, return a neutral probability scaled by FIXED_SCALE
    return FIXED_SCALE / 2;
  }

  // Count the number of '1's in the context
  size_t ones = 0;
  for (size_t i = 0; i < context_length; i++) {
    uint8_t bit = (context[i / 8] >> (7 - (i % 8))) & 1;
    if (bit) {
      ones++;
    }
  }

  // Apply smoothing: (ones + 1) / (context_length + 2)
  double probability = (double)(ones + 1) / (context_length + 2);

  // Clamp probability to [1/FIXED_SCALE, (FIXED_SCALE-1)/FIXED_SCALE]
  if (probability < 1.0 / FIXED_SCALE) {
    probability = 1.0 / FIXED_SCALE;
  } else if (probability > (double)(FIXED_SCALE - 1) / FIXED_SCALE) {
    probability = (double)(FIXED_SCALE - 1) / FIXED_SCALE;
  }

  // Convert to fixed-point representation
  uint32_t p_fixed = (uint32_t)(probability * FIXED_SCALE);

  return p_fixed;
}
