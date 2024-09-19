#!/usr/bin/env python3

import io
import urllib.request
import glorious


def download_image_to_memory(url):
  """Downloads an image file from the provided URL and returns it as raw bytes."""
  with urllib.request.urlopen(url) as response:
    image_bytes = response.read()  # Read the image as raw bytes
  return image_bytes


def compress_image_data(image_data, bit_length, context_length):
  """Compress the image data using glorious."""
  encoded = glorious.encode(image_data, bit_length, context_length)
  return encoded


def decompress_image_data(encoded_data, bit_length, context_length):
  """Decompress the encoded image data using glorious."""
  decoded = glorious.decode(encoded_data, bit_length, context_length)
  return decoded


def calculate_compression_ratio(original_data, compressed_data):
  """Calculate the compression ratio based on the original and compressed data sizes."""
  original_bits_per_byte = 8  # Fixed, since there are always 8 bits in a byte
  compressed_bits_per_byte = len(compressed_data) * 8 / len(original_data)
  return original_bits_per_byte, compressed_bits_per_byte


if __name__ == "__main__":
  # Step 1: Use the provided image URL
  image_url = "https://freight.cargo.site/t/original/i/238bba1a42d1f45eabd8b67d36149419f9890f53bc59562f756b6c9a854dfb30/MS_Merkel_Angela_CloseUp.jpg"

  # Step 2: Download the image to memory
  image_data = download_image_to_memory(image_url)

  # Step 3: Get the bit length (number of bits in the image data)
  bit_length = len(image_data) * 8

  # Step 4: Set a reasonable context length for compression (e.g., 16 bits)
  context_length = 1000

  # Step 5: Compress the image data
  compressed_data = compress_image_data(image_data, bit_length, context_length)

  # Step 6: Decompress the image data
  decompressed_data = decompress_image_data(
      compressed_data, bit_length, context_length
  )

  # Step 7: Check if decompressed data matches the original data
  if decompressed_data == image_data:
    print("Success! Decompressed data matches the original.")
  else:
    print("Error: Decompressed data does not match the original.")

  # Step 8: Calculate and print the compression ratio (bits per byte)
  original_bits_per_byte, compressed_bits_per_byte = calculate_compression_ratio(
      image_data, compressed_data
  )

  print(f"Original bits per byte: {original_bits_per_byte}")
  print(f"Compressed bits per byte: {compressed_bits_per_byte}")
