// probability.c

#include "probability.h"

// Define the fixed scaling factor if not already defined
#ifndef FIXED_SCALE
#define FIXED_SCALE (1 << 16)  // 16-bit fixed-point scaling factor
#endif

/**
 * @brief Optimized function to obtain the fixed-point probability of bit '1'.
 *
 * This function replaces floating-point arithmetic with branchless integer
 * operations to enhance performance while maintaining the original behavior.
 *
 * @param context_content Pointer to the ContextContent structure containing
 *                        context length and count of '1's.
 * @return uint32_t The fixed-point probability of bit '1', scaled by
 * FIXED_SCALE.
 */
uint32_t example_get_probability_fixed(const ContextContent *context_content) {
  uint32_t context_length = context_content->context_length;
  uint32_t count_ones = context_content->count_ones;

  // Calculate numerator and denominator
  uint32_t numerator = count_ones + 1;
  uint32_t denominator = context_length + 2;

  // Compute probability without explicit zero context check
  uint32_t prob_fixed =
      (uint32_t)(((uint64_t)numerator * FIXED_SCALE + (denominator / 2)) /
                 denominator);

  // Branchless clamping to ensure 1 <= prob_fixed <= FIXED_SCALE - 1

  // Clamp low: if prob_fixed < 1, set to 1
  uint32_t clamp_low = -(prob_fixed < 1) & (1 - prob_fixed);
  prob_fixed += clamp_low;

  // Clamp high: if prob_fixed >= FIXED_SCALE, set to FIXED_SCALE - 1
  uint32_t clamp_high =
      -(prob_fixed >= FIXED_SCALE) & ((FIXED_SCALE - 1) - prob_fixed);
  prob_fixed += clamp_high;

  return prob_fixed;
}
