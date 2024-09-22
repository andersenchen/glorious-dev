// src/glorious/c/src/arithmetic_coding.c

#include "arithmetic_coding.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "probability.h"

// -----------------------------
// Compiler Optimization Hints
// -----------------------------

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

// -----------------------------
// Static Inline Helper Functions
// -----------------------------

/**
 * @brief Reads a single bit from the encoded data.
 *
 * This function reads a bit from the encoded byte array based on the current
 * bit index. If the end of the data is reached, it returns 0 by default.
 *
 * @param encoded Pointer to the encoded byte array. (Non-aliasing)
 * @param bit_index Pointer to the current bit index. This is updated after
 * reading.
 * @param encoded_length Length of the encoded data in bytes.
 * @return int The bit value (0 or 1). Returns 0 if end of data is reached.
 */
static inline int read_bit(const uint8_t *restrict encoded, size_t *bit_index,
                           size_t encoded_length) {
  size_t current_bit = *bit_index;
  size_t byte_pos = current_bit >> 3;

  if (UNLIKELY(byte_pos >= encoded_length)) {
    return 0;  // End of data reached; default to 0
  }

  // Extract the bit: Shift the byte right by (7 - bit_position) and mask with 1
  uint8_t current_byte = encoded[byte_pos];
  uint8_t shift = 7 - (current_bit & 7);
  int bit = (current_byte >> shift) & 1;

  // Increment bit_index using pre-increment for potential optimization
  *bit_index = current_bit + 1;

  return bit;
}

/**
 * @brief Ensures that the output buffer has enough capacity.
 *
 * If the current output buffer cannot accommodate additional bytes, it is
 * resized.
 *
 * @param coder Pointer to the ArithmeticCoder instance. (Non-aliasing)
 * @param additional Number of additional bytes to accommodate.
 */
static inline void ensure_output_capacity(ArithmeticCoder *restrict coder,
                                          size_t additional) {
  size_t required = coder->output_size + additional;

  if (UNLIKELY(required > coder->output_capacity)) {
    size_t new_capacity = coder->output_capacity;
    if (new_capacity == 0) {
      new_capacity = INITIAL_OUTPUT_CAPACITY;
    }

    // Calculate the next power of two greater than or equal to required
    // Using a bit-twiddling method for efficiency
    if (new_capacity < required) {
      new_capacity = required;
      // If not already a power of two, find the next power of two
      new_capacity--;
      new_capacity |= new_capacity >> 1;
      new_capacity |= new_capacity >> 2;
      new_capacity |= new_capacity >> 4;
      new_capacity |= new_capacity >> 8;
      new_capacity |= new_capacity >> 16;
#if SIZE_MAX > UINT32_MAX
      new_capacity |= new_capacity >> 32;
#endif
      new_capacity++;
    }

    uint8_t *new_output = realloc(coder->output, new_capacity);
    if (UNLIKELY(!new_output)) {
      perror("Realloc failed");
      exit(EXIT_FAILURE);
    }

    coder->output = new_output;
    coder->output_capacity = new_capacity;
  }
}

/**
 * @brief Outputs a single bit to the ArithmeticCoder's buffer.
 *
 * This function shifts the current bit_buffer left by one, adds the new bit,
 * and increments the bit_count. When bit_count reaches 8, the buffer is flushed
 * to the output array. If the output buffer is full, it is resized.
 *
 * @param coder Pointer to the ArithmeticCoder instance. (Non-aliasing)
 * @param bit The bit to output (0 or 1).
 */
static inline void output_bit(ArithmeticCoder *restrict coder, int bit) {
  // Shift the bit_buffer left by 1 and add the new bit
  coder->bit_buffer = (coder->bit_buffer << 1) | (bit & 1);
  coder->bit_count++;  // Increment the bit count

  // If the bit_buffer is full (8 bits), flush it to the output
  if (LIKELY(coder->bit_count == 8)) {
    ensure_output_capacity(coder, 1);
    coder->output[coder->output_size++] = (uint8_t)coder->bit_buffer;
    coder->bit_buffer = 0;
    coder->bit_count = 0;
  }
}

/**
 * @brief Flushes any remaining bits in the ArithmeticCoder's buffer to the
 * output.
 *
 * If there are bits left in the bit_buffer that don't make up a full byte,
 * this function pads them with zeros and appends the final byte to the output.
 *
 * @param coder Pointer to the ArithmeticCoder instance. (Non-aliasing)
 */
static inline void flush_output(ArithmeticCoder *restrict coder) {
  if (LIKELY(coder->bit_count > 0)) {
    // Pad the remaining bits with zeros by shifting left
    coder->bit_buffer <<= (8 - coder->bit_count);

    // Ensure there's enough space in the output buffer
    ensure_output_capacity(coder, 1);

    // Append the final byte
    coder->output[coder->output_size++] = (uint8_t)coder->bit_buffer;

    // Reset the buffer
    coder->bit_buffer = 0;
    coder->bit_count = 0;
  }
}

/**
 * @brief Clamps the probability to avoid extremes.
 *
 * This function ensures that the probability of bit '1' does not become too
 * close to 0 or 1, which could lead to numerical issues during
 * encoding/decoding.
 *
 * @param p1_fixed Probability of bit '1' in fixed-point.
 * @return uint32_t Clamped probability in fixed-point.
 */
static inline uint32_t clamp_probability_fixed(uint32_t p1_fixed) {
  // Branchless clamping to ensure 1 <= p1_fixed <= FIXED_SCALE - 1

  // Compute masks
  // mask_low is all ones if p1_fixed < 1, else all zeros
  uint32_t mask_low = -(p1_fixed < 1);
  // mask_high is all ones if p1_fixed >= FIXED_SCALE, else all zeros
  uint32_t mask_high = -(p1_fixed >= FIXED_SCALE);

  // Apply clamping in a single step
  uint32_t clamped_p1_fixed =
      (p1_fixed & ~(mask_low | mask_high))  // Keep original if within range
      | (1 & mask_low)                      // Set to 1 if below range
      |
      ((FIXED_SCALE - 1) & mask_high);  // Set to FIXED_SCALE - 1 if above range

  return clamped_p1_fixed;
}

/**
 * @brief Outputs bits that followed a rescaling event.
 *
 * During encoding, when the range is rescaled, certain bits may need to be
 * outputted after a rescaling event. This function handles that by outputting
 * the specified bit multiple times.
 *
 * @param coder Pointer to the ArithmeticCoder instance. (Non-aliasing)
 * @param bit The bit to output (0 or 1).
 */
static inline void output_following_bits(ArithmeticCoder *restrict coder,
                                         int bit) {
  size_t count = coder->bits_to_follow;
  if (count == 0) return;

  // Optimize by handling multiple bits at once if possible
  while (count > 0) {
    size_t bits_to_write =
        count < 8 - coder->bit_count ? count : 8 - coder->bit_count;
    for (size_t i = 0; i < bits_to_write; i++) {
      coder->bit_buffer = (coder->bit_buffer << 1) | (bit & 1);
      coder->bit_count++;
    }
    count -= bits_to_write;

    if (coder->bit_count == 8) {
      ensure_output_capacity(coder, 1);
      coder->output[coder->output_size++] = (uint8_t)coder->bit_buffer;
      coder->bit_buffer = 0;
      coder->bit_count = 0;
    }
  }

  coder->bits_to_follow = 0;
}

/**
 * @brief Updates the context using a ring buffer and maintains the count of
 * '1's.
 *
 * Inserts a new bit into the context buffer at the current index, updates the
 * count of '1's based on the new and old bit, and updates the index in a
 * circular manner.
 *
 * @param coder Pointer to the ArithmeticCoder instance. (Non-aliasing)
 * @param new_bit The new bit to add (0 or 1).
 */
static inline void update_context_ring_buffer(ArithmeticCoder *restrict coder,
                                              int new_bit) {
  if (LIKELY(coder->context_capacity > 0)) {
    size_t byte_pos = coder->context_index >> 3;      // Equivalent to /8
    size_t bit_pos = 7 - (coder->context_index & 7);  // Equivalent to %8

    // Retrieve the old bit value
    int old_bit = (coder->context_buffer[byte_pos] >> bit_pos) & 1;

    // Set or clear the bit at the current position using precomputed mask
    uint8_t mask = (1U << bit_pos);
    coder->context_buffer[byte_pos] =
        (coder->context_buffer[byte_pos] & ~mask) | ((new_bit & 1) << bit_pos);

    // Update the count of '1's
    coder->count_ones += (uint32_t)new_bit - (uint32_t)old_bit;

    // Update the context index in a circular manner
    coder->context_index++;
    if (UNLIKELY(coder->context_index >= coder->context_capacity)) {
      coder->context_index -= coder->context_capacity;
    }
  }
}

// -----------------------------
// Arithmetic Encoding Function
// -----------------------------

/**
 * @brief Encodes a sequence of bits using arithmetic coding.
 *
 * @param sequence Pointer to the input bit sequence. (Non-aliasing)
 * @param length Number of bits in the input sequence.
 * @param encoded_output Pointer to the buffer where encoded data will be
 * stored. Memory is allocated within the function and should be freed by the
 * caller.
 * @param context_length Length of the context in bits.
 * @param get_probability_fixed Function pointer to obtain the probability of
 * bit '1' given the context. It should return a fixed-point probability scaled
 * by FIXED_SCALE.
 * @return size_t The size of the encoded output in bytes.
 */
size_t arithmetic_encode(const uint8_t *restrict sequence, size_t length,
                         uint8_t **encoded_output, size_t context_length,
                         ProbabilityFunction get_probability_fixed) {
  // Precompute constants based on PRECISION
  const uint32_t TOTAL_FREQUENCY = 1U << PRECISION;  // Total range (e.g., 2^31)
  const uint32_t HALF = 1U << (PRECISION - 1);       // Half of the range
  const uint32_t QUARTER = 1U << (PRECISION - 2);    // Quarter of the range
  const uint32_t THREE_QUARTER =
      3U << (PRECISION - 2);  // Three quarters of the range

  // Initialize ArithmeticCoder using compound literal
  ArithmeticCoder coder = {0};
  coder.high = TOTAL_FREQUENCY - 1;
  coder.low = 0;  // Initialize low as 0
  coder.output = malloc(INITIAL_OUTPUT_CAPACITY);
  if (UNLIKELY(!coder.output)) {
    perror("Initial malloc failed");
    exit(EXIT_FAILURE);
  }
  coder.output_size = 0;
  coder.output_capacity = INITIAL_OUTPUT_CAPACITY;
  coder.bits_to_follow = 0;
  coder.bit_buffer = 0;
  coder.bit_count = 0;
  memset(coder.context_buffer, 0,
         MAX_CONTEXT_BYTES);  // Initialize context to zero
  coder.context_capacity = context_length;
  coder.context_index = 0;
  coder.count_ones = 0;  // Initialize count of '1's to zero

  // Initialize ContextContent
  ContextContent context_content = {0};
  context_content.context_length = context_length;

  // Iterate over each bit in the input sequence
  for (size_t i = 0; i < length; i++) {
    // Extract the current bit to encode
    uint8_t byte = sequence[i >> 3];
    uint8_t bit = (byte >> (7 - (i & 7))) & 1;

    // Populate ContextContent with the current count of '1's
    context_content.count_ones = coder.count_ones;

    // Obtain the probability of the current bit being '1' based on the context
    uint32_t p1_fixed = get_probability_fixed(&context_content);
    uint32_t p0_fixed = FIXED_SCALE - p1_fixed;

    // Scale probabilities to the total frequency range
    uint32_t scaled_p0 =
        (uint32_t)(((uint64_t)p0_fixed * TOTAL_FREQUENCY) / FIXED_SCALE);
    scaled_p0 =
        (scaled_p0 >= TOTAL_FREQUENCY) ? (TOTAL_FREQUENCY - 1) : scaled_p0;

    // Calculate the current range
    uint32_t range = coder.high - coder.low + 1;

    // Update coder.low and coder.high based on the bit
    if (bit == 0) {
      // For bit '0', adjust the upper bound
      coder.high =
          coder.low + ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY - 1;
    } else {
      // For bit '1', adjust the lower bound
      coder.low += ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY;
    }

    // Renormalization: Shift the range until the high and low share the same
    // top bits
    while (1) {
      if (coder.high < HALF) {
        // The range is entirely in the lower half
        output_bit(&coder, 0);
        output_following_bits(&coder, 1);
        coder.low <<= 1;
        coder.high = (coder.high << 1) | 1;
      } else if (coder.low >= HALF) {
        // The range is entirely in the upper half
        output_bit(&coder, 1);
        output_following_bits(&coder, 0);
        coder.low = (coder.low - HALF) << 1;
        coder.high = ((coder.high - HALF) << 1) | 1;
      } else if (coder.low >= QUARTER && coder.high < THREE_QUARTER) {
        // The range is in the middle half
        coder.bits_to_follow++;
        coder.low = (coder.low - QUARTER) << 1;
        coder.high = ((coder.high - QUARTER) << 1) | 1;
      } else {
        break;  // No renormalization needed
      }
    }

    // Update the context with the current bit using the ring buffer
    update_context_ring_buffer(&coder, bit);
  }

  // Final bits: After processing all input bits, handle the remaining range
  coder.bits_to_follow++;
  if (coder.low < QUARTER) {
    // If the final range is in the lower quarter, output a '0' bit followed by
    // '1's
    output_bit(&coder, 0);
    output_following_bits(&coder, 1);
  } else {
    // Otherwise, output a '1' bit followed by '0's
    output_bit(&coder, 1);
    output_following_bits(&coder, 0);
  }

  // Flush any remaining bits in the buffer to the output
  flush_output(&coder);

  // Set the encoded_output pointer to the coder's output buffer
  *encoded_output = coder.output;

  // Return the size of the encoded output in bytes
  return coder.output_size;
}

// -----------------------------
// Arithmetic Decoding Function
// -----------------------------

/**
 * @brief Decodes a sequence of bits encoded using arithmetic coding.
 *
 * @param encoded Pointer to the encoded byte array. (Non-aliasing)
 * @param encoded_length Length of the encoded data in bytes.
 * @param decoded Pointer to the buffer where decoded bits will be stored.
 *                The buffer should be preallocated by the caller.
 * (Non-aliasing)
 * @param decoded_length Number of bits to decode.
 * @param context_length Length of the context in bits.
 * @param get_probability_fixed Function pointer to obtain the probability of
 * bit '1' given the context. It should return a fixed-point probability scaled
 * by FIXED_SCALE.
 */
void arithmetic_decode(const uint8_t *restrict encoded, size_t encoded_length,
                       uint8_t *restrict decoded, size_t decoded_length,
                       size_t context_length,
                       ProbabilityFunction get_probability_fixed) {
  // Precompute constants based on PRECISION
  const uint32_t TOTAL_FREQUENCY =
      1U << PRECISION;  // Total frequency range (e.g., 2^31)
  const uint32_t HALF = 1U << (PRECISION - 1);     // Half of the range
  const uint32_t QUARTER = 1U << (PRECISION - 2);  // Quarter of the range
  const uint32_t THREE_QUARTER =
      3U << (PRECISION - 2);  // Three quarters of the range

  // Initialize ArithmeticCoder using compound literal
  ArithmeticCoder coder = {0};
  coder.low = 0;
  coder.high = TOTAL_FREQUENCY - 1;
  coder.value = 0;
  coder.bits_to_follow = 0;
  coder.bit_buffer = 0;
  coder.bit_count = 0;
  memset(coder.context_buffer, 0,
         MAX_CONTEXT_BYTES);  // Initialize context to zero
  coder.context_capacity = context_length;
  coder.context_index = 0;
  coder.count_ones = 0;  // Initialize count of '1's to zero

  // Initialize ContextContent
  ContextContent context_content = {0};
  context_content.context_length = context_length;

  size_t bit_index = 0;  // Initialize bit index for reading encoded data

  // Initialize coder.value by reading the first PRECISION bits from the encoded
  // data
  for (int i = 0; i < PRECISION; i++) {
    coder.value =
        (coder.value << 1) | read_bit(encoded, &bit_index, encoded_length);
  }

  // Iterate over each bit to decode
  for (size_t i = 0; i < decoded_length; i++) {
    // Populate ContextContent with the current count of '1's
    context_content.count_ones = coder.count_ones;

    // Obtain the probability of the current bit being '1' based on the context
    uint32_t p1_fixed = get_probability_fixed(&context_content);
    uint32_t p0_fixed = FIXED_SCALE - p1_fixed;

    // Scale probabilities to the total frequency range
    uint32_t scaled_p0 =
        (uint32_t)(((uint64_t)p0_fixed * TOTAL_FREQUENCY) / FIXED_SCALE);
    scaled_p0 =
        (scaled_p0 >= TOTAL_FREQUENCY) ? (TOTAL_FREQUENCY - 1) : scaled_p0;

    // Calculate the current range
    uint32_t range = coder.high - coder.low + 1;

    // Calculate the scaled value which determines the current bit
    uint64_t temp = (uint64_t)(coder.value - coder.low + 1);
    temp *= TOTAL_FREQUENCY;
    temp -= 1;
    uint32_t scaled_value = (uint32_t)(temp / range);

    uint8_t bit = (scaled_value < scaled_p0) ? 0 : 1;

    // Set the decoded bit in the output buffer using precomputed masks
    if (bit) {
      decoded[i >> 3] |=
          (1U << (7 - (i & 7)));  // Set the corresponding bit to '1'
    } else {
      decoded[i >> 3] &=
          ~(1U << (7 - (i & 7)));  // Ensure the corresponding bit is '0'
    }

    // Update the context with the decoded bit using the ring buffer
    update_context_ring_buffer(&coder, bit);

    // Adjust coder.high and coder.low based on the decoded bit
    if (bit == 0) {
      coder.high =
          coder.low + ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY - 1;
    } else {
      coder.low += ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY;
    }

    // Renormalization: Shift the range until the high and low share the same
    // top bits
    while (1) {
      if (coder.high < HALF) {
        // The range is entirely in the lower half
        // No adjustment needed for value
      } else if (coder.low >= HALF) {
        // The range is entirely in the upper half
        coder.value -= HALF;  // Remove the lower half from value
        coder.low -= HALF;    // Remove the lower half from low\

        coder.high -= HALF;   // Remove the lower half from high
      } else if (coder.low >= QUARTER && coder.high < THREE_QUARTER) {
        // The range is in the middle half
        coder.value -= QUARTER;  // Remove the lower quarter from value
        coder.low -= QUARTER;    // Remove the lower quarter from low
        coder.high -= QUARTER;   // Remove the lower quarter from high
      } else {
        break;  // No renormalization needed
      }

      // Shift the range left by one bit
      coder.low <<= 1;
      coder.high = (coder.high << 1) | 1;  // Shift high left and set the LSB

      // Shift the value left and read the next bit from the encoded data
      coder.value =
          (coder.value << 1) | read_bit(encoded, &bit_index, encoded_length);
    }
  }

  // No memory to free for coder.output as it's not used in decoding
}
