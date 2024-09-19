// probability.c

#include "probability.h"

// Define the fixed scaling factor if not already defined
#ifndef FIXED_SCALE
#define FIXED_SCALE (1 << 16)  // 16-bit fixed-point scaling factor
#endif

uint32_t example_get_probability_fixed(const ContextContent *context_content) {
  if (context_content->context_length == 0) {
    // Avoid division by zero; return a neutral probability
    return FIXED_SCALE / 2;
  }

  // Apply smoothing: (count_ones + 1) / (context_length + 2)
  double probability = ((double)(context_content->count_ones + 1)) /
                       (context_content->context_length + 2);

  // Ensure the probability is within (0,1)
  if (probability <= 0.0) {
    probability = 1.0 / FIXED_SCALE;  // Minimum probability
  } else if (probability >= 1.0) {
    probability = 1.0 - (1.0 / FIXED_SCALE);  // Maximum probability
  }

  // Scale the probability
  return (uint32_t)(probability * FIXED_SCALE);
}
