// arithmetic_coding.h

#ifndef ARITHMETIC_CODING_H
#define ARITHMETIC_CODING_H

#include <stdint.h>
#include <stdlib.h>

#include "context.h"      // Include ContextContent
#include "probability.h"  // Include the probability functions

// Define the precision of the arithmetic coder.
#ifndef PRECISION
#define PRECISION 31
#endif

#define FIXED_SCALE (1 << 16)  // Fixed-point scaling factor (16 bits)
#define MAX_CONTEXT_BYTES \
  (256 * 1000)  // Maximum context size in bytes (adjust as needed)
#define INITIAL_OUTPUT_CAPACITY \
  4096  // Initial buffer size to minimize reallocations

/**
 * @brief Structure representing the state of the arithmetic coder.
 */
typedef struct {
  uint32_t low;            // Lower bound of the current range
  uint32_t high;           // Upper bound of the current range
  uint32_t value;          // Current value for decoding (used in decoding)
  uint8_t *output;         // Output buffer for encoded data
  size_t output_size;      // Current size of the output buffer
  size_t output_capacity;  // Total capacity of the output buffer
  size_t bits_to_follow;   // Number of bits to follow (for rescaling)
  uint8_t bit_buffer;      // Buffer for bits being written
  uint8_t bit_count;       // Number of bits currently in the bit_buffer

  // Ring buffer context management
  uint8_t context_buffer[MAX_CONTEXT_BYTES];  // Buffer to store context bits as
                                              // a ring buffer
  size_t context_capacity;  // Size of the context buffer in bits
  size_t context_index;     // Current index in the ring buffer

  size_t count_ones;  // Number of '1's in the current context
} ArithmeticCoder;

/**
 * @brief Function pointer type for obtaining fixed-point probabilities based on
 * context.
 *
 * @param context_content Pointer to the ContextContent struct.
 * @return uint32_t Fixed-point probability of the current bit being '1', scaled
 * by FIXED_SCALE.
 */
typedef uint32_t (*ProbabilityFunction)(const ContextContent *context_content);

/**
 * @brief Performs arithmetic encoding on a sequence of bits using fixed-point
 * probabilities.
 *
 * @param sequence Pointer to the input bit sequence (packed as bytes).
 * @param length Number of bits in the input sequence.
 * @param encoded_output Pointer to the output buffer where encoded data will be
 * stored. The caller is responsible for freeing this buffer.
 * @param context_length Length of the context in bits.
 * @param get_probability_fixed Function pointer to obtain the probability of
 * bit '1' given the context.
 * @return size_t The size of the encoded output in bytes.
 */
size_t arithmetic_encode(const uint8_t *sequence, size_t length,
                         uint8_t **encoded_output, size_t context_length,
                         ProbabilityFunction get_probability_fixed);

/**
 * @brief Performs arithmetic decoding on an encoded byte sequence using
 * fixed-point probabilities.
 *
 * @param encoded Pointer to the encoded byte array.
 * @param encoded_length Length of the encoded data in bytes.
 * @param decoded Pointer to the buffer where decoded bits will be stored
 * (packed as bytes). The buffer should be pre-allocated by the caller and
 * should be large enough to hold the decoded bits.
 * @param decoded_length Number of bits to decode.
 * @param context_length Length of the context in bits.
 * @param get_probability_fixed Function pointer to obtain the probability of
 * bit '1' given the context.
 */
void arithmetic_decode(const uint8_t *encoded, size_t encoded_length,
                       uint8_t *decoded, size_t decoded_length,
                       size_t context_length,
                       ProbabilityFunction get_probability_fixed);

#endif  // ARITHMETIC_CODING_H
