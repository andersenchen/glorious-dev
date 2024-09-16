// utilities.h

#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>
#include <stdlib.h>

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
                                    uint32_t probability_fixed);

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
                           int print_info);

#endif  // UTILITIES_H
