// rust_code/src/arithmetic_coding.rs

use crate::probability::{ContextContent, ProbabilityFunction};

/// Constants for arithmetic coding precision and scaling.
const PRECISION: u32 = 32; // Number of bits in precision
const FIXED_SCALE: u32 = 1 << 16; // Must match the probability module.
const INITIAL_OUTPUT_CAPACITY: usize = 1024;
const MAX_CONTEXT_BYTES: usize = 256;

/// Represents the state of the Arithmetic Coder.
pub struct ArithmeticCoder {
    low: u64,
    high: u64,
    value: u64, // Only used in decoding mode
    bits_to_follow: u32,
    bit_buffer: u8,
    bit_count: u8,
    output: Vec<u8>,
    context_buffer: Vec<u8>,
    context_capacity: usize,
    context_index: usize,
    count_ones: u32,
}

/// Error type for encoding and decoding operations.
#[derive(Debug)]
pub enum ArithmeticCodingError {
    /// Indicates that the input data is invalid or corrupted.
    InvalidInput(String),
    /// Indicates memory allocation failures.
    MemoryAllocation(String),
}

impl std::fmt::Display for ArithmeticCodingError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ArithmeticCodingError::InvalidInput(msg) => write!(f, "Invalid input: {}", msg),
            ArithmeticCodingError::MemoryAllocation(msg) => {
                write!(f, "Memory allocation failed: {}", msg)
            }
        }
    }
}

impl std::error::Error for ArithmeticCodingError {}

/// Encodes a sequence of bits using arithmetic coding.
///
/// # Arguments
///
/// * `sequence` - Slice of input bits (`0` or `1`).
/// * `length` - Number of bits to encode.
/// * `context_length` - Length of the context in bits.
/// * `get_probability_fixed` - Function to obtain bit '1' probability.
///
/// # Returns
///
/// * `Result<Vec<u8>, ArithmeticCodingError>` - Encoded byte vector or an error.
///
/// # Examples
///
/// ```
/// use rust_code::{arithmetic_encode, example_get_probability_fixed, ContextContent};
///
/// let input_bits = [0b11001010];
/// let encoded = arithmetic_encode(&input_bits, 8, 4, example_get_probability_fixed).unwrap();
/// ```
pub fn arithmetic_encode(
    sequence: &[u8],
    length: usize,
    context_length: usize,
    get_probability_fixed: ProbabilityFunction,
) -> Result<Vec<u8>, ArithmeticCodingError> {
    // Initialize coder with low=0, high=TOTAL_FREQUENCY -1, value=0
    let mut coder = ArithmeticCoder::new(context_length)?;

    // Precompute constants based on PRECISION
    let total_frequency = 1u64 << PRECISION;
    let half = 1u64 << (PRECISION - 1);
    let quarter = 1u64 << (PRECISION - 2);
    let three_quarter = 3u64 << (PRECISION - 2);

    // Iterate over each bit in the input sequence
    for i in 0..length {
        // Extract the current bit to encode
        let byte = sequence[i / 8];
        let bit = (byte >> (7 - (i % 8))) & 1;

        // Populate ContextContent with the current count of '1's
        let context_content = ContextContent {
            context_length,
            count_ones: coder.count_ones,
        };

        // Obtain the probability of the current bit being '1' given the context
        let p1_fixed = get_probability_fixed(&context_content);
        let p0_fixed = FIXED_SCALE - p1_fixed;

        // Scale probabilities to the total frequency range
        let scaled_p0 =
            ((p0_fixed as u64 * total_frequency) / FIXED_SCALE as u64).min(total_frequency - 1);

        // Calculate the current range
        let range = coder.high - coder.low + 1;

        // Update coder.low and coder.high based on the bit
        if bit == 0 {
            // For bit '0', adjust the upper bound
            coder.high = coder.low + ((range * scaled_p0) / total_frequency) - 1;
        } else {
            // For bit '1', adjust the lower bound
            coder.low += (range * scaled_p0) / total_frequency;
        }

        // Renormalization: Shift the range until high and low share the same top bits
        loop {
            if coder.high < half {
                // The range is entirely in the lower half
                coder.output_bit(0)?;
                coder.output_following_bits(1)?;
                coder.low <<= 1;
                coder.high = (coder.high << 1) | 1;
            } else if coder.low >= half {
                // The range is entirely in the upper half
                coder.output_bit(1)?;
                coder.output_following_bits(0)?;
                coder.low = (coder.low - half) << 1;
                coder.high = ((coder.high - half) << 1) | 1;
            } else if coder.low >= quarter && coder.high < three_quarter {
                // The range is in the middle half
                coder.bits_to_follow += 1;
                coder.low = (coder.low - quarter) << 1;
                coder.high = ((coder.high - quarter) << 1) | 1;
            } else {
                break; // No renormalization needed
            }
        }

        // Update the context with the current bit using the ring buffer
        coder.update_context(bit)?;
    }

    // Final bits: After processing all input bits, handle the remaining range
    coder.bits_to_follow += 1;
    if coder.low < quarter {
        // If the final range is in the lower quarter, output a '0' bit followed by '1's
        coder.output_bit(0)?;
        coder.output_following_bits(1)?;
    } else {
        // Otherwise, output a '1' bit followed by '0's
        coder.output_bit(1)?;
        coder.output_following_bits(0)?;
    }

    // Flush any remaining bits in the buffer to the output
    coder.flush_output()?;

    Ok(coder.output)
}

/// Decodes a sequence of bits encoded using arithmetic coding.
///
/// # Arguments
///
/// * `encoded` - Slice of encoded bytes.
/// * `encoded_length` - Length of the encoded data in bytes.
/// * `decoded_length` - Number of bits to decode.
/// * `context_length` - Length of the context in bits.
/// * `get_probability_fixed` - Function to obtain bit '1' probability.
///
/// # Returns
///
/// * `Result<Vec<u8>, ArithmeticCodingError>` - Decoded byte vector or an error.
///
/// # Examples
///
/// ```
/// use rust_code::{arithmetic_decode, example_get_probability_fixed, ContextContent};
///
/// let encoded_data = vec![0xAB, 0xCD];
/// let decoded = arithmetic_decode(&encoded_data, 2, 8, 4, example_get_probability_fixed).unwrap();
/// ```
pub fn arithmetic_decode(
    encoded: &[u8],
    encoded_length: usize,
    decoded_length: usize,
    context_length: usize,
    get_probability_fixed: ProbabilityFunction,
) -> Result<Vec<u8>, ArithmeticCodingError> {
    // Initialize coder with low=0, high=TOTAL_FREQUENCY -1, value=0
    let mut coder = ArithmeticCoder::new(context_length)?;

    // Precompute constants based on PRECISION
    let total_frequency = 1u64 << PRECISION;
    let half = 1u64 << (PRECISION - 1);
    let quarter = 1u64 << (PRECISION - 2);
    let three_quarter = 3u64 << (PRECISION - 2);

    // Initialize coder.value by reading the first PRECISION bits from the encoded data
    for _ in 0..PRECISION {
        let bit = coder.read_bit(encoded, encoded_length);
        coder.value = (coder.value << 1) | (bit as u64);
    }

    let mut decoded = vec![0u8; (decoded_length + 7) / 8];

    // Iterate over each bit to decode
    for i in 0..decoded_length {
        // Populate ContextContent with the current count of '1's
        let context_content = ContextContent {
            context_length,
            count_ones: coder.count_ones,
        };

        // Obtain the probability of the current bit being '1' based on the context
        let p1_fixed = get_probability_fixed(&context_content);
        let p0_fixed = FIXED_SCALE - p1_fixed;

        // Scale probabilities to the total frequency range
        let scaled_p0 =
            ((p0_fixed as u64 * total_frequency) / FIXED_SCALE as u64).min(total_frequency - 1);

        // Calculate the current range
        let range = coder.high - coder.low + 1;

        // Calculate the scaled value which determines the current bit
        let temp = ((coder.value - coder.low + 1) as u128 * total_frequency as u128) - 1;
        let scaled_value = (temp / range as u128) as u64;

        let bit = if scaled_value < scaled_p0 { 0 } else { 1 };

        // Set the decoded bit in the output buffer
        if bit == 1 {
            decoded[i / 8] |= 1 << (7 - (i % 8));
        } else {
            decoded[i / 8] &= !(1 << (7 - (i % 8)));
        }

        // Update the context with the decoded bit using the ring buffer
        coder.update_context(bit)?;

        // Update coder.high and coder.low based on the decoded bit
        if bit == 0 {
            coder.high = coder.low + ((range * scaled_p0) / total_frequency) - 1;
        } else {
            coder.low += (range * scaled_p0) / total_frequency;
        }

        // Renormalization: Shift the range until high and low share the same top bits
        loop {
            if coder.high < half {
                // The range is entirely in the lower half
                // No adjustment needed for value
            } else if coder.low >= half {
                // The range is entirely in the upper half
                coder.value -= half;
                coder.low -= half;
                coder.high -= half;
            } else if coder.low >= quarter && coder.high < three_quarter {
                // The range is in the middle half
                coder.value -= quarter;
                coder.low -= quarter;
                coder.high -= quarter;
            } else {
                break; // No renormalization needed
            }

            // Shift the range left by one bit
            coder.low <<= 1;
            coder.high = (coder.high << 1) | 1;

            // Shift the value left and read the next bit from the encoded data
            let bit = coder.read_bit(encoded, encoded_length);
            coder.value = (coder.value << 1) | (bit as u64);
        }
    }

    Ok(decoded)
}

impl ArithmeticCoder {
    /// Constructs a new `ArithmeticCoder` instance.
    ///
    /// # Arguments
    ///
    /// * `context_length` - Length of the context in bits.
    ///
    /// # Errors
    ///
    /// Returns an error if `context_length` exceeds `MAX_CONTEXT_BYTES * 8`.
    fn new(context_length: usize) -> Result<Self, ArithmeticCodingError> {
        if context_length > MAX_CONTEXT_BYTES * 8 {
            return Err(ArithmeticCodingError::InvalidInput(format!(
                "Context length {} exceeds maximum allowed {} bits.",
                context_length,
                MAX_CONTEXT_BYTES * 8
            )));
        }

        Ok(ArithmeticCoder {
            low: 0,
            high: (1u64 << PRECISION) - 1,
            value: 0,
            bits_to_follow: 0,
            bit_buffer: 0,
            bit_count: 0,
            output: Vec::with_capacity(INITIAL_OUTPUT_CAPACITY),
            context_buffer: vec![0u8; (context_length + 7) / 8],
            context_capacity: context_length,
            context_index: 0,
            count_ones: 0,
        })
    }

    /// Reads a single bit from the encoded data based on the current `bits_to_follow`.
    ///
    /// Returns `0` if attempting to read beyond the encoded data, mirroring the C implementation.
    ///
    /// # Arguments
    ///
    /// * `encoded` - Slice of encoded bytes.
    /// * `encoded_length` - Total length of the encoded data in bytes.
    ///
    /// # Returns
    ///
    /// * `u8` - The read bit (`0` or `1`). Defaults to `0` beyond data.
    fn read_bit(&mut self, encoded: &[u8], encoded_length: usize) -> u8 {
        let bit_index = self.bits_to_follow;
        let byte_pos = (bit_index / 8) as usize;
        if byte_pos >= encoded_length {
            return 0;
        }
        let shift = 7 - (bit_index % 8);
        let bit = (encoded[byte_pos] >> shift) & 1;
        self.bits_to_follow += 1;
        bit
    }

    /// Outputs a single bit to the buffer.
    ///
    /// # Arguments
    ///
    /// * `bit` - Bit value to output (`0` or `1`).
    ///
    /// # Errors
    ///
    /// Returns an error if writing to the buffer fails.
    fn output_bit(&mut self, bit: u8) -> Result<(), ArithmeticCodingError> {
        self.bit_buffer = (self.bit_buffer << 1) | (bit & 1);
        self.bit_count += 1;

        if self.bit_count == 8 {
            self.output.push(self.bit_buffer);
            self.bit_buffer = 0;
            self.bit_count = 0;
        }

        Ok(())
    }

    /// Outputs multiple following bits after a rescaling event.
    ///
    /// # Arguments
    ///
    /// * `bit` - Bit value to output (`0` or `1`).
    ///
    /// # Errors
    ///
    /// Returns an error if writing to the buffer fails.
    fn output_following_bits(&mut self, bit: u8) -> Result<(), ArithmeticCodingError> {
        for _ in 0..self.bits_to_follow {
            self.output_bit(bit)?;
        }
        self.bits_to_follow = 0;
        Ok(())
    }

    /// Flushes any remaining bits in the buffer to the output.
    ///
    /// # Errors
    ///
    /// Returns an error if writing to the buffer fails.
    fn flush_output(&mut self) -> Result<(), ArithmeticCodingError> {
        if self.bit_count > 0 {
            self.bit_buffer <<= 8 - self.bit_count;
            self.output.push(self.bit_buffer);
            self.bit_buffer = 0;
            self.bit_count = 0;
        }
        Ok(())
    }

    /// Updates the context buffer with a new bit using a ring buffer approach.
    ///
    /// # Arguments
    ///
    /// * `new_bit` - New bit to add (`0` or `1`).
    ///
    /// # Errors
    ///
    /// Returns an error if context capacity is exceeded.
    fn update_context(&mut self, new_bit: u8) -> Result<(), ArithmeticCodingError> {
        if self.context_capacity > 0 {
            let byte_pos = self.context_index / 8;
            let bit_pos = 7 - (self.context_index % 8);
            let old_bit = (self.context_buffer[byte_pos] >> bit_pos) & 1;

            let mask = 1 << bit_pos;
            self.context_buffer[byte_pos] =
                (self.context_buffer[byte_pos] & !mask) | ((new_bit & 1) << bit_pos);

            self.count_ones = self.count_ones + (new_bit as u32) - (old_bit as u32);

            self.context_index = (self.context_index + 1) % self.context_capacity;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use proptest::prelude::*;
    use rand::Rng;

    use crate::probability::example_get_probability_fixed;

    /// Helper function to generate random bit sequences for testing.
    fn generate_random_bits(length: usize) -> Vec<u8> {
        let mut rng = rand::thread_rng();
        let bytes = (length + 7) / 8;
        let mut bits = vec![0u8; bytes];
        for i in 0..length {
            let byte = i / 8;
            let bit = 7 - (i % 8);
            bits[byte] |= (rng.gen_bool(0.5) as u8) << bit;
        }
        bits
    }

    #[test]
    fn test_arithmetic_encode_decode() {
        let input_bits = [0b11001010];
        let input_length = 8;
        let context_length = 4;

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded, input_bits);
    }

    #[test]
    fn test_empty_encode_decode() {
        let input_bits: [u8; 0] = [];
        let input_length = 0;
        let context_length = 4;

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(encoded.len(), 1); // Changed from 0 to 1

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded.len(), 0);
    }

    #[test]
    fn test_single_bit_encode_decode() {
        let input_bits = [0b10000000];
        let input_length = 1;
        let context_length = 4;

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded[0] & 0b10000000, input_bits[0] & 0b10000000);
    }

    #[test]
    fn test_multiple_bytes_encode_decode() {
        let input_bits = [0b11110000, 0b10101010, 0b00001111];
        let input_length = 24;
        let context_length = 8;

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded, input_bits);
    }

    #[test]
    fn test_arithmetic_encode_decode_random() {
        let input_length = 1024;
        let context_length = 16;
        let input_bits = generate_random_bits(input_length);

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();

        // Truncate the last byte if input_length is not a multiple of 8
        let mut expected = input_bits.clone();
        if input_length % 8 != 0 {
            let excess_bits = 8 - (input_length % 8);
            let last_byte_mask = 0xFF << excess_bits;
            if let Some(last_byte) = expected.last_mut() {
                *last_byte &= last_byte_mask;
            }
        }

        assert_eq!(decoded, expected);
    }

    #[test]
    fn test_encode_decode_with_all_zeros() {
        let input_length = 64;
        let context_length = 8;
        let input_bits = vec![0u8; 8]; // 64 zeros

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded, input_bits);
    }

    #[test]
    fn test_encode_decode_with_all_ones() {
        let input_length = 64;
        let context_length = 8;
        let input_bits = vec![0xFFu8; 8]; // 64 ones

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded, input_bits);
    }

    proptest! {
        #[test]
        fn test_arithmetic_encode_decode_prop(
            input_bits in proptest::collection::vec(0u8..=255, 0usize..=8192),
            length in 0usize..=65536,
            context_length in 1usize..=256
        ) {
            // Adjust the input_bits to have at least 'length' bits
            let required_bytes = (length + 7) / 8;
            let mut bits = input_bits.clone();
            bits.resize(required_bytes, 0);

            let encoded = arithmetic_encode(&bits, length, context_length, example_get_probability_fixed).unwrap();
            let decoded = arithmetic_decode(&encoded, encoded.len(), length, context_length, example_get_probability_fixed).unwrap();

            // Recreate the expected bits
            let mut expected = vec![0u8; (length + 7) / 8];
            for i in 0..length {
                let byte = i / 8;
                let bit_pos = 7 - (i % 8);
                let bit = (bits[byte] >> bit_pos) & 1;
                if bit == 1 {
                    expected[byte] |= 1 << bit_pos;
                } else {
                    expected[byte] &= !(1 << bit_pos);
                }
            }

            assert_eq!(decoded, expected);
        }
    }

    #[test]
    fn test_encode_decode_max_context() {
        let input_length = 128;
        let context_length = 256; // Maximum allowed

        // Generate a pattern with alternating bits
        let mut input_bits = vec![0u8; 16];
        for i in 0..input_length {
            if i % 2 == 0 {
                input_bits[i / 8] |= 1 << (7 - (i % 8));
            }
        }

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded, input_bits);
    }

    /// Test that encoding with a context length of zero behaves correctly.
    #[test]
    fn test_encode_decode_zero_context() {
        let input_length = 16;
        let context_length = 0; // No context

        let input_bits = [0b10101010, 0b01010101];

        let encoded = arithmetic_encode(
            &input_bits,
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert!(!encoded.is_empty());

        let decoded = arithmetic_decode(
            &encoded,
            encoded.len(),
            input_length,
            context_length,
            example_get_probability_fixed,
        )
        .unwrap();
        assert_eq!(decoded, input_bits);
    }

    /// Test encoding and decoding with varying context lengths.
    #[test]
    fn test_encode_decode_various_contexts() {
        let input_length = 64;
        let input_bits = generate_random_bits(input_length);

        for context_length in [1, 4, 8, 16, 32, 64] {
            let encoded = arithmetic_encode(
                &input_bits,
                input_length,
                context_length,
                example_get_probability_fixed,
            )
            .unwrap();
            assert!(!encoded.is_empty());

            let decoded = arithmetic_decode(
                &encoded,
                encoded.len(),
                input_length,
                context_length,
                example_get_probability_fixed,
            )
            .unwrap();
            // Truncate the last byte if input_length is not a multiple of 8
            let mut expected = input_bits.clone();
            if input_length % 8 != 0 {
                let excess_bits = 8 - (input_length % 8);
                let last_byte_mask = 0xFF << excess_bits;
                if let Some(last_byte) = expected.last_mut() {
                    *last_byte &= last_byte_mask;
                }
            }
            assert_eq!(decoded, expected);
        }
    }

    proptest! {
        #[test]
        fn test_encode_decode_large_data(
            input_bits in proptest::collection::vec(0u8..=255, 0usize..=65536),
            length in 0usize..=524288,
            context_length in 1usize..=256
        ) {
            // Adjust the input_bits to have at least 'length' bits
            let required_bytes = (length + 7) / 8;
            let mut bits = input_bits.clone();
            bits.resize(required_bytes, 0);

            let encoded = arithmetic_encode(&bits, length, context_length, example_get_probability_fixed).unwrap();
            let decoded = arithmetic_decode(&encoded, encoded.len(), length, context_length, example_get_probability_fixed).unwrap();

            // Recreate the expected bits
            let mut expected = vec![0u8; (length + 7) / 8];
            for i in 0..length {
                let byte = i / 8;
                let bit_pos = 7 - (i % 8);
                let bit = (bits[byte] >> bit_pos) & 1;
                if bit == 1 {
                    expected[byte] |= 1 << bit_pos;
                } else {
                    expected[byte] &= !(1 << bit_pos);
                }
            }

            assert_eq!(decoded, expected);
        }
    }
}
