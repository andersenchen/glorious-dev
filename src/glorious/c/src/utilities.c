// utilities.c

#include "utilities.h"

#include <inttypes.h>  // For PRIuPTR, Windows compatibility
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arithmetic_coding.h"
#include "probability.h"

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

// Macro for platform-independent size_t formatting
#if defined(_MSC_VER) || defined(__MINGW32__)
#define SIZE_T_FMT "%Iu"
#else
#define SIZE_T_FMT "%zu"
#endif

/**
 * @brief Generates a random bit sequence with a given fixed-point probability.
 *
 * This function populates the `sequence` array with `length` bits, where each
 * bit is set to '1' with a probability specified by `probability_fixed`. The
 * probability is represented in fixed-point format, scaled by FIXED_SCALE.
 *
 * @param sequence Pointer to the buffer where the generated bits will be
 * stored. The buffer should be preallocated and have space for at least (length
 * + 7)/8 bytes.
 * @param length Number of bits to generate.
 * @param probability_fixed Fixed-point probability of setting a bit to '1',
 * scaled by FIXED_SCALE.
 */
void generate_random_sequence_fixed(uint8_t *sequence, size_t length,
                                    uint32_t probability_fixed) {
  uint32_t state = (uint32_t)time(NULL);
  // Convert fixed-point probability to a threshold
  // The threshold is scaled to UINT32_MAX for comparison with rand_val
  uint32_t threshold = ((uint64_t)probability_fixed * UINT32_MAX) / FIXED_SCALE;

  for (size_t i = 0; i < (length + 7) / 8; i++) {
    sequence[i] = 0;
    for (int j = 0; j < 8 && i * 8 + j < length; j++) {
      // Xorshift random number generator
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      uint32_t rand_val = state;
      if (rand_val < threshold) {
        sequence[i] |= (1 << (7 - j));
      }
    }
  }
}

/**
 * @brief Tests the arithmetic coding implementation by encoding and decoding a
 * sequence.
 *
 * This function encodes the provided bit sequence using arithmetic encoding and
 * then decodes it. It compares the original sequence with the decoded sequence
 * to verify correctness.
 *
 * @param sequence Pointer to the input bit sequence (packed as bytes).
 * @param length Number of bits in the input sequence.
 * @param context_length Length of the context in bits.
 * @param get_probability_fixed Function pointer to obtain the probability of
 * bit '1' given the context. It should return a fixed-point probability scaled
 * by FIXED_SCALE.
 * @param print_info If non-zero, prints compression information.
 * @return int Returns 1 if the decode matches the original sequence, 0
 * otherwise.
 */
int test_arithmetic_coding(const uint8_t *sequence, size_t length,
                           size_t context_length,
                           uint32_t (*get_probability_fixed)(
                               const uint8_t *context, size_t context_length),
                           int print_info) {
  // Encode the input sequence
  uint8_t *encoded_output = NULL;
  size_t encoded_length = arithmetic_encode(
      sequence, length, &encoded_output, context_length, get_probability_fixed);

  if (encoded_output == NULL || encoded_length == 0) {
    fprintf(stderr, "Encoding failed\n");
    return 0;
  }

  // Calculate compression rate: (bits in encoded data) / (bits in original
  // data)
  double compression_rate = (double)(encoded_length * 8) / length;

  // Allocate memory for decoded output
  uint8_t *decoded_output =
      (uint8_t *)calloc((length + 7) / 8, sizeof(uint8_t));
  if (!decoded_output) {
    fprintf(stderr, "Memory allocation failed in test_arithmetic_coding\n");
    free(encoded_output);
    return 0;
  }

  // Perform arithmetic decoding
  arithmetic_decode(encoded_output, encoded_length, decoded_output, length,
                    context_length, get_probability_fixed);

  // Compare original and decoded sequences
  int match = (memcmp(sequence, decoded_output, (length + 7) / 8) == 0);

  if (!match) {
    fprintf(stderr, "Mismatch detected. Original vs Decoded:\n");
    for (size_t i = 0; i < length; i++) {
      uint8_t original_bit = (sequence[i / 8] >> (7 - (i % 8))) & 1;
      uint8_t decoded_bit = (decoded_output[i / 8] >> (7 - (i % 8))) & 1;
      if (original_bit != decoded_bit) {
        fprintf(stderr, "Position " SIZE_T_FMT ": %d vs %d\n", i, original_bit,
                decoded_bit);
      }
    }
  }

  // Optionally print compression information
  if (print_info) {
    printf("Sequence length: " SIZE_T_FMT ", Context length: " SIZE_T_FMT
           ", Compression rate: %.2f\n",
           length, context_length, compression_rate);
  }

  // Clean up allocated memory
  free(encoded_output);
  free(decoded_output);

  return match;
}
