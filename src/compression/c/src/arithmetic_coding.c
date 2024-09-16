// arithmetic_coding.c

#include "arithmetic_coding.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "probability.h"

// -----------------------------
// Arithmetic Coding Implementation
// -----------------------------

// -----------------------------
// Static Inline Helper Functions
// -----------------------------

/**
 * @brief Reads a single bit from the encoded data.
 *
 * This function reads a bit from the encoded byte array based on the current
 * bit index. If the end of the data is reached, it returns 0 by default.
 *
 * @param encoded Pointer to the encoded byte array.
 * @param bit_index Pointer to the current bit index. This is updated after
 * reading.
 * @param encoded_length Length of the encoded data in bytes.
 * @return int The bit value (0 or 1). Returns 0 if end of data is reached.
 */
static inline int read_bit(const uint8_t *encoded, size_t *bit_index,
                           size_t encoded_length) {
  size_t byte_pos = *bit_index / 8;  // Determine the byte position
  if (byte_pos >= encoded_length) {
    return 0;  // End of data reached; default to 0
  }
  int bit = (encoded[byte_pos] >> (7 - (*bit_index % 8))) &
            1;     // Extract the desired bit
  (*bit_index)++;  // Move to the next bit
  return bit;
}

/**
 * @brief Ensures that the output buffer has enough capacity.
 *
 * If the current output buffer cannot accommodate additional bytes, it is
 * resized.
 *
 * @param coder Pointer to the ArithmeticCoder instance.
 * @param additional Number of additional bytes to accommodate.
 */
static inline void ensure_output_capacity(ArithmeticCoder *coder,
                                          size_t additional) {
  if (coder->output_size + additional > coder->output_capacity) {
    size_t new_capacity = coder->output_capacity * 2;
    if (new_capacity < coder->output_size + additional)
      new_capacity = coder->output_size + additional;
    uint8_t *new_output = realloc(coder->output, new_capacity);
    if (!new_output) {
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
 * @param coder Pointer to the ArithmeticCoder instance.
 * @param bit The bit to output (0 or 1).
 */
static inline void output_bit(ArithmeticCoder *coder, int bit) {
  // Shift the bit_buffer left by 1 and add the new bit (ensuring it's only 0 or
  // 1)
  coder->bit_buffer = (coder->bit_buffer << 1) | (bit & 1);
  coder->bit_count++;  // Increment the bit count

  // If the bit_buffer is full (8 bits), flush it to the output
  if (coder->bit_count == 8) {
    ensure_output_capacity(coder, 1);
    coder->output[coder->output_size++] = coder->bit_buffer & 0xFF;
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
 * @param coder Pointer to the ArithmeticCoder instance.
 */
static inline void flush_output(ArithmeticCoder *coder) {
  if (coder->bit_count > 0) {
    // Pad the remaining bits with zeros by shifting left
    coder->bit_buffer <<= (8 - coder->bit_count);

    // Ensure there's enough space in the output buffer
    ensure_output_capacity(coder, 1);

    // Append the final byte
    coder->output[coder->output_size++] = coder->bit_buffer & 0xFF;

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
  if (p1_fixed < 1) return 1;
  if (p1_fixed > FIXED_SCALE - 1) return FIXED_SCALE - 1;
  return p1_fixed;
}

/**
 * @brief Outputs bits that followed a rescaling event.
 *
 * During encoding, when the range is rescaled, certain bits may need to be
 * outputted after a rescaling event. This function handles that by outputting
 * the specified bit multiple times.
 *
 * @param coder Pointer to the ArithmeticCoder instance.
 * @param bit The bit to output (0 or 1).
 */
static inline void output_following_bits(ArithmeticCoder *coder, int bit) {
  while (coder->bits_to_follow > 0) {
    output_bit(coder, bit);
    coder->bits_to_follow--;
  }
}

/**
 * @brief Updates the context using a ring buffer.
 *
 * Inserts a new bit into the context buffer at the current index and
 * updates the index in a circular manner.
 *
 * @param coder Pointer to the ArithmeticCoder instance.
 * @param new_bit The new bit to add (0 or 1).
 */
static inline void update_context_ring_buffer(ArithmeticCoder *coder,
                                              int new_bit) {
  if (coder->context_capacity == 0) {
    return;  // No context to update
  }

  size_t byte_pos = coder->context_index / 8;
  size_t bit_pos = 7 - (coder->context_index % 8);

  // Set or clear the bit at the current position
  if (new_bit) {
    coder->context_buffer[byte_pos] |= (1 << bit_pos);
  } else {
    coder->context_buffer[byte_pos] &= ~(1 << bit_pos);
  }

  // Update the context index in a circular manner
  coder->context_index = (coder->context_index + 1) % coder->context_capacity;
}

/**
 * @brief Retrieves the current context as a byte array.
 *
 * Constructs a contiguous bit sequence from the ring buffer for use in
 * probability calculations.
 *
 * @param coder Pointer to the ArithmeticCoder instance.
 * @param context_length Length of the context in bits.
 * @param buffer Pointer to the preallocated buffer to store the context bits.
 */
static inline void get_current_context_buffer(const ArithmeticCoder *coder,
                                              size_t context_length,
                                              uint8_t *buffer) {
  if (context_length == 0) {
    return;
  }

  size_t context_bytes = (context_length + 7) / 8;
  memset(buffer, 0, context_bytes);  // Initialize buffer to zero

  for (size_t i = 0; i < context_length; i++) {
    size_t bit_pos = (coder->context_index + i) % context_length;
    size_t byte = bit_pos / 8;
    size_t bit = 7 - (bit_pos % 8);
    int bit_value = (coder->context_buffer[byte] >> bit) & 1;

    size_t dest_byte = i / 8;
    size_t dest_bit = 7 - (i % 8);
    buffer[dest_byte] |= (bit_value << dest_bit);
  }
}

// -----------------------------
// Arithmetic Encoding Function
// -----------------------------

/**
 * @brief Encodes a sequence of bits using arithmetic coding.
 *
 * @param sequence Pointer to the input bit sequence.
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
size_t arithmetic_encode(const uint8_t *sequence, size_t length,
                         uint8_t **encoded_output, size_t context_length,
                         uint32_t (*get_probability_fixed)(
                             const uint8_t *context, size_t context_length)) {
  // Precompute constants based on PRECISION
  const uint32_t TOTAL_FREQUENCY = 1U << PRECISION;  // Total range (e.g., 2^31)
  const uint32_t HALF = 1U << (PRECISION - 1);       // Half of the range
  const uint32_t QUARTER = 1U << (PRECISION - 2);    // Quarter of the range
  const uint32_t THREE_QUARTER =
      3U << (PRECISION - 2);  // Three quarters of the range

  // Initialize ArithmeticCoder
  ArithmeticCoder coder;
  coder.low = 0;
  coder.high = TOTAL_FREQUENCY - 1;
  coder.value = 0;  // Not used in encoding
  coder.output = malloc(INITIAL_OUTPUT_CAPACITY);
  if (!coder.output) {
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

  // Preallocate a buffer for current context
  uint8_t current_context_buffer[MAX_CONTEXT_BYTES];
  memset(current_context_buffer, 0, sizeof(current_context_buffer));

  // Iterate over each bit in the input sequence
  for (size_t i = 0; i < length; i++) {
    // Extract the current bit to encode
    uint8_t bit = (sequence[i / 8] >> (7 - (i % 8))) & 1;

    // Obtain the probability of the current bit being '1' based on the context
    get_current_context_buffer(&coder, context_length, current_context_buffer);
    uint32_t p1_fixed = clamp_probability_fixed(
        get_probability_fixed(current_context_buffer, context_length));
    uint32_t p0_fixed = FIXED_SCALE - p1_fixed;

    // Scale probabilities to the total frequency range
    uint32_t scaled_p0 = ((uint64_t)p0_fixed * TOTAL_FREQUENCY) / FIXED_SCALE;
    if (scaled_p0 >= TOTAL_FREQUENCY) {
      scaled_p0 = TOTAL_FREQUENCY - 1;
    }

    // Calculate the current range
    uint32_t range = coder.high - coder.low + 1;

    // Update coder.low and coder.high based on the bit
    if (bit == 0) {
      // For bit '0', adjust the upper bound
      coder.high =
          coder.low + ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY - 1;
    } else {
      // For bit '1', adjust the lower bound
      coder.low = coder.low + ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY;
    }

    // Renormalization: Shift the range until the high and low share the same
    // top bits
    while (1) {
      if (coder.high < HALF) {
        // The range is entirely in the lower half
        // Output a '0' bit and handle any following bits
        output_bit(&coder, 0);
        output_following_bits(&coder, 1);
        coder.low <<= 1;  // Shift low left by 1
        coder.high =
            (coder.high << 1) | 1;  // Shift high left by 1 and set the LSB
      } else if (coder.low >= HALF) {
        // The range is entirely in the upper half
        // Output a '1' bit and handle any following bits
        output_bit(&coder, 1);
        output_following_bits(&coder, 0);
        coder.low = (coder.low - HALF) << 1;  // Adjust low and shift left by 1
        coder.high = ((coder.high - HALF) << 1) |
                     1;  // Adjust high, shift left, and set LSB
      } else if (coder.low >= QUARTER && coder.high < THREE_QUARTER) {
        // The range is in the middle half
        // Increment bits_to_follow and adjust the range
        coder.bits_to_follow++;
        coder.low = (coder.low - QUARTER)
                    << 1;  // Remove the lower quarter and shift left
        coder.high = ((coder.high - QUARTER) << 1) |
                     1;  // Remove the lower quarter, shift left, and set LSB
      } else {
        // No renormalization needed
        break;
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
 * @param encoded Pointer to the encoded byte array.
 * @param encoded_length Length of the encoded data in bytes.
 * @param decoded Pointer to the buffer where decoded bits will be stored.
 *                The buffer should be preallocated by the caller.
 * @param decoded_length Number of bits to decode.
 * @param context_length Length of the context in bits.
 * @param get_probability_fixed Function pointer to obtain the probability of
 * bit '1' given the context. It should return a fixed-point probability scaled
 * by FIXED_SCALE.
 */
void arithmetic_decode(const uint8_t *encoded, size_t encoded_length,
                       uint8_t *decoded, size_t decoded_length,
                       size_t context_length,
                       uint32_t (*get_probability_fixed)(
                           const uint8_t *context, size_t context_length)) {
  // Precompute constants based on PRECISION
  const uint32_t TOTAL_FREQUENCY =
      1U << PRECISION;  // Total frequency range (e.g., 2^31)
  const uint32_t HALF = 1U << (PRECISION - 1);     // Half of the range
  const uint32_t QUARTER = 1U << (PRECISION - 2);  // Quarter of the range
  const uint32_t THREE_QUARTER =
      3U << (PRECISION - 2);  // Three quarters of the range

  // Initialize ArithmeticCoder
  ArithmeticCoder coder;
  coder.low = 0;
  coder.high = TOTAL_FREQUENCY - 1;
  coder.value = 0;
  coder.output = NULL;        // Not used in decoding
  coder.output_size = 0;      // Not used in decoding
  coder.output_capacity = 0;  // Not used in decoding
  coder.bits_to_follow = 0;
  coder.bit_buffer = 0;
  coder.bit_count = 0;
  memset(coder.context_buffer, 0,
         MAX_CONTEXT_BYTES);  // Initialize context to zero
  coder.context_capacity = context_length;
  coder.context_index = 0;

  // Preallocate a buffer for current context
  uint8_t current_context_buffer[MAX_CONTEXT_BYTES];
  memset(current_context_buffer, 0, sizeof(current_context_buffer));

  size_t bit_index = 0;  // Initialize bit index for reading encoded data

  // Initialize coder.value by reading the first PRECISION bits from the encoded
  // data
  for (int i = 0; i < PRECISION; i++) {
    coder.value =
        (coder.value << 1) | read_bit(encoded, &bit_index, encoded_length);
  }

  // Iterate over each bit to decode
  for (size_t i = 0; i < decoded_length; i++) {
    // Obtain the probability of the current bit being '1' based on the context
    get_current_context_buffer(&coder, context_length, current_context_buffer);
    uint32_t p1_fixed = clamp_probability_fixed(
        get_probability_fixed(current_context_buffer, context_length));
    uint32_t p0_fixed = FIXED_SCALE - p1_fixed;

    // Scale probabilities to the total frequency range
    uint32_t scaled_p0 = ((uint64_t)p0_fixed * TOTAL_FREQUENCY) / FIXED_SCALE;
    if (scaled_p0 >= TOTAL_FREQUENCY) {
      scaled_p0 = TOTAL_FREQUENCY - 1;
    }

    // Calculate the current range
    uint32_t range = coder.high - coder.low + 1;

    // Calculate the scaled value which determines the current bit
    uint64_t scaled_value =
        ((uint64_t)(coder.value - coder.low + 1) * TOTAL_FREQUENCY - 1) / range;

    uint8_t bit;
    if (scaled_value < scaled_p0) {
      // The bit is '0'
      bit = 0;
      // Adjust the upper bound
      coder.high =
          coder.low + ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY - 1;
    } else {
      // The bit is '1'
      bit = 1;
      // Adjust the lower bound
      coder.low += ((uint64_t)range * scaled_p0) / TOTAL_FREQUENCY;
    }

    // Set the decoded bit in the output buffer
    if (bit) {
      decoded[i / 8] |=
          (1 << (7 - (i % 8)));  // Set the corresponding bit to '1'
    } else {
      decoded[i / 8] &=
          ~(1 << (7 - (i % 8)));  // Ensure the corresponding bit is '0'
    }

    // Update the context with the decoded bit using the ring buffer
    update_context_ring_buffer(&coder, bit);

    // Renormalization: Shift the range until the high and low share the same
    // top bits
    while (1) {
      if (coder.high < HALF) {
        // The range is entirely in the lower half
        // No adjustment needed for value
      } else if (coder.low >= HALF) {
        // The range is entirely in the upper half
        // Adjust the value and the range
        coder.value -= HALF;  // Remove the lower half from value
        coder.low -= HALF;    // Remove the lower half from low
        coder.high -= HALF;   // Remove the lower half from high
      } else if (coder.low >= QUARTER && coder.high < THREE_QUARTER) {
        // The range is in the middle half
        // Adjust the value and the range
        coder.value -= QUARTER;  // Remove the lower quarter from value
        coder.low -= QUARTER;    // Remove the lower quarter from low
        coder.high -= QUARTER;   // Remove the lower quarter from high
      } else {
        // No renormalization needed
        break;
      }

      // Shift the range left by one bit
      coder.low <<= 1;
      coder.high = (coder.high << 1) | 1;  // Shift high left and set the LSB

      // Shift the value left and read the next bit from the encoded data
      coder.value =
          (coder.value << 1) | read_bit(encoded, &bit_index, encoded_length);
    }
  }

  // Note: No memory to free for coder.output as it's not used in decoding
}
