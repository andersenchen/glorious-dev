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
  // Extract context_length and count_ones for efficiency
  uint32_t context_length = context_content->context_length;
  uint32_t count_ones = context_content->count_ones;

  // Calculate numerator and denominator
  uint32_t numerator = count_ones + 1;
  uint32_t denominator = context_length + 2;

  // Compute probability based on whether context_length is zero
  uint32_t is_zero = (context_length == 0);
  uint32_t prob_fixed_zero = FIXED_SCALE / 2;
  uint32_t prob_fixed =
      is_zero
          ? prob_fixed_zero
          : (uint32_t)(((uint64_t)numerator * FIXED_SCALE + (denominator / 2)) /
                       denominator);

  // Branchless clamping to ensure 1 <= prob_fixed <= FIXED_SCALE - 1

  // Clamp lower bound: if prob_fixed < 1, set to 1
  // Compute mask: 0xFFFFFFFF if prob_fixed < 1, else 0x00000000
  uint32_t clamp_low_mask = -(prob_fixed < 1);
  // Calculate adjustment: if prob_fixed < 1, add (1 - prob_fixed), else add 0
  uint32_t clamp_low = clamp_low_mask & (1 - prob_fixed);
  prob_fixed += clamp_low;

  // Clamp upper bound: if prob_fixed >= FIXED_SCALE, set to FIXED_SCALE - 1
  // Compute mask: 0xFFFFFFFF if prob_fixed >= FIXED_SCALE, else 0x00000000
  uint32_t clamp_high_mask = -(prob_fixed >= FIXED_SCALE);
  // Calculate adjustment: if prob_fixed >= FIXED_SCALE, add (FIXED_SCALE - 1 -
  // prob_fixed), else add 0
  uint32_t clamp_high = clamp_high_mask & ((FIXED_SCALE - 1) - prob_fixed);
  prob_fixed += clamp_high;

  return prob_fixed;
}
