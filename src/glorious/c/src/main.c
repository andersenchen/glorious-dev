// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arithmetic_coding.h"
#include "probability.h"

// Function to print a buffer in binary (for debugging)
void print_bits(const uint8_t *buffer, size_t length) {
  for (size_t i = 0; i < length * 8; i++) {
    printf("%d", (buffer[i / 8] >> (7 - (i % 8))) & 1);
    if ((i + 1) % 8 == 0) {
      printf(" ");
    }
  }
  printf("\n");
}

int main() {
  // Example bit sequence: 11001010 (8 bits)
  uint8_t input_bits = 0b11001010;
  size_t input_length = 8;  // Number of bits

  // Encoding
  uint8_t *encoded_data = NULL;
  size_t encoded_size =
      arithmetic_encode(&input_bits, input_length, &encoded_data,
                        4,  // Example context length: 4 bits
                        example_get_probability_fixed);

  printf("Encoded data (%zu bytes): ", encoded_size);
  for (size_t i = 0; i < encoded_size; i++) {
    printf("%02X ", encoded_data[i]);
  }
  printf("\n");

  // Decoding
  uint8_t decoded_bits = 0;
  size_t decoded_length = 8;  // Number of bits to decode

  // Initialize decoded_bits to 0
  memset(&decoded_bits, 0, sizeof(decoded_bits));

  arithmetic_decode(encoded_data, encoded_size, &decoded_bits, decoded_length,
                    4,  // Same context length used in encoding
                    example_get_probability_fixed);

  printf("Decoded bit sequence: ");
  print_bits(&decoded_bits, 1);

  // Verify correctness
  if (decoded_bits == input_bits) {
    printf("Decoding successful. The decoded bits match the original input.\n");
  } else {
    printf(
        "Decoding failed. The decoded bits do not match the original input.\n");
  }

  // Free allocated memory
  free(encoded_data);

  return 0;
}
