// arithmetic_coding_demo/src/main.rs

use rust_code::{arithmetic_decode, arithmetic_encode, example_get_probability_fixed};
use std::io::{self, Write};

/// Helper function to print a buffer in binary format.
///
/// # Arguments
///
/// * `buffer` - Slice of bytes to print.
/// * `length` - Number of bits to print.
fn print_bits(buffer: &[u8], length: usize) {
    for i in 0..length {
        print!("{}", (buffer[i / 8] >> (7 - (i % 8))) & 1);
        if (i + 1) % 8 == 0 {
            print!(" ");
        }
    }
    println!();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Example bit sequence: 11001010 (8 bits)
    let input_bits: [u8; 1] = [0b11001010];
    let input_length = 8; // Number of bits
    let context_length = 4; // Example context length: 4 bits

    println!("Original Bit Sequence: ");
    print_bits(&input_bits, input_length);

    // Encoding
    let encoded_data = arithmetic_encode(
        &input_bits,
        input_length,
        context_length,
        example_get_probability_fixed,
    )?;

    println!("Encoded Data ({} bytes): ", encoded_data.len());
    for byte in &encoded_data {
        print!("{:02X} ", byte);
    }
    println!();

    // Decoding
    let decoded_data = arithmetic_decode(
        &encoded_data,
        encoded_data.len(),
        input_length,
        context_length,
        example_get_probability_fixed,
    )?;

    println!("Decoded Bit Sequence: ");
    print_bits(&decoded_data, input_length);

    // Verify correctness
    if decoded_data == input_bits {
        println!("Decoding successful. The decoded bits match the original input.");
    } else {
        println!("Decoding failed. The decoded bits do not match the original input.");
    }

    Ok(())
}
