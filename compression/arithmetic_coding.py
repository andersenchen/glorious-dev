"""Implements finite precision arithmetic encode and decode functions using p1_func for Bernoulli probability."""

import numpy as np
from typing import Callable
from bitarray import bitarray
from bitarray.util import int2ba, ba2int

def arithmetic_encode_finite(bit_sequence: bitarray,
                             p1_func: Callable[[bitarray], float],
                             precision: int = 32,
                             context_window: int = 10) -> bitarray:
  """Encodes a bit sequence using finite precision arithmetic coding with context-dependent probability.

  Args:
    bit_sequence: The input bit sequence to encode.
    p1_func: A function that takes a context (bitarray of previous bits) and returns the probability of a 1 bit.
    precision: The precision in bits. Defaults to 32.
    context_window: The number of previous bits to use as context. Defaults to 10.

  Returns:
    The encoded bit sequence as a bitarray.
  """
  scale_bits = 0
  encoded_bits = bitarray()
  low = 0
  high = (1 << precision) - 1  # All 1s in `precision` bits
  context = bitarray('0' * context_window)  # Initialize context with zeros

  def output_bit(bit):
    nonlocal scale_bits
    encoded_bits.append(bit)
    for _ in range(scale_bits):
      encoded_bits.append(not bit)  # Flip bits due to carry propagation
    scale_bits = 0

  # Main encoding loop
  for bit in bit_sequence:
    p1 = p1_func(context)
    p1_scaled = int(p1 * (1 << precision))  # Scale the probability

    range_width = high - low + 1
    midpoint = high - (range_width * p1_scaled >> precision)

    if bit == 0:
      high = midpoint
    else:
      low = midpoint + 1

    # Update context
    context = context[1:] + bitarray([bit])

    # Renormalization and bit shifting
    while True:
      if high < (1 << (precision - 1)):
        output_bit(0)
        low <<= 1
        high = (high << 1) | 1
      elif low >= (1 << (precision - 1)):
        output_bit(1)
        low = (low - (1 << (precision - 1))) << 1
        high = ((high - (1 << (precision - 1))) << 1) | 1
      elif low >= (1 << (precision - 2)) and high < (3 << (precision - 2)):
        scale_bits += 1
        low = (low - (1 << (precision - 2))) << 1
        high = ((high - (1 << (precision - 2))) << 1) | 1
      else:
        break

  # Final bit output to ensure the last interval is represented
  if low < (1 << (precision - 2)):
    output_bit(0)
    encoded_bits.append(1)  # Append an additional bit to ensure completeness
  else:
    output_bit(1)

  # Ensure we output at least 'precision' number of bits
  while len(encoded_bits) < precision:
    output_bit(0)

  return encoded_bits

def arithmetic_decode_finite(encoded_bits: bitarray,
                             p1_func: Callable[[bitarray], float],
                             sequence_length: int,
                             precision: int = 32,
                             context_window: int = 10) -> bitarray:
  """Decodes a bit sequence using finite precision arithmetic coding with context-dependent probability.

  Args:
    encoded_bits: The encoded bit sequence to decode.
    p1_func: A function that takes a context (bitarray of previous bits) and returns the probability of a 1 bit.
    sequence_length: The length of the original sequence.
    precision: The precision in bits. Defaults to 32.
    context_window: The number of previous bits to use as context. Defaults to 10.

  Returns:
    The decoded bit sequence as a bitarray.
  """
  low = 0
  high = (1 << precision) - 1
  
  # Ensure encoded_bits is at least precision bits long
  if len(encoded_bits) < precision:
    padded_bits = encoded_bits + bitarray('0' * (precision - len(encoded_bits)))
  else:
    padded_bits = encoded_bits

  value = ba2int(padded_bits[:precision])
  context = bitarray('0' * context_window)  # Initialize context with zeros

  decoded_sequence = bitarray()

  # Main decoding loop
  bit_index = precision
  for _ in range(sequence_length):
    p1 = p1_func(context)
    p1_scaled = int(p1 * (1 << precision))  # Scale the probability

    range_width = high - low + 1
    midpoint = high - (range_width * p1_scaled >> precision)

    if value <= midpoint:
      decoded_bit = 0
      high = midpoint
    else:
      decoded_bit = 1
      low = midpoint + 1

    decoded_sequence.append(decoded_bit)

    # Update context
    context = context[1:] + bitarray([decoded_bit])

    # Renormalization and bit shifting
    while True:
      if high < (1 << (precision - 1)):
        low <<= 1
        high = (high << 1) | 1
        value <<= 1
        if bit_index < len(padded_bits):
          value |= padded_bits[bit_index]
          bit_index += 1
      elif low >= (1 << (precision - 1)):
        low = (low - (1 << (precision - 1))) << 1
        high = ((high - (1 << (precision - 1))) << 1) | 1
        value = (value - (1 << (precision - 1))) << 1
        if bit_index < len(padded_bits):
          value |= padded_bits[bit_index]
          bit_index += 1
      elif low >= (1 << (precision - 2)) and high < (3 << (precision - 2)):
        low = (low - (1 << (precision - 2))) << 1
        high = ((high - (1 << (precision - 2))) << 1) | 1
        value = (value - (1 << (precision - 2))) << 1
        if bit_index < len(padded_bits):
          value |= padded_bits[bit_index]
          bit_index += 1
      else:
        break

  return decoded_sequence

def main():
  """Demonstrates the usage of arithmetic encoding and decoding with constant Bernoulli probability."""
  sequence_length = 10000
  context_window = 10
  precision = 32
  p = 0.2  # Constant probability for Bernoulli distribution

  # Constant probability function (always returns 0.2)
  def p1_func(context):
    return 0.2

  # Generate a bit sequence using Bernoulli distribution
  bit_sequence = bitarray(np.random.binomial(1, p, sequence_length).tolist())

  # Encode and decode with finite precision arithmetic
  encoded_bits = arithmetic_encode_finite(bit_sequence, p1_func, precision, context_window)
  decoded_sequence = arithmetic_decode_finite(encoded_bits, p1_func, sequence_length, precision, context_window)

  # Calculate compression rate
  original_size = len(bit_sequence)
  encoded_size = len(encoded_bits)
  compression_rate = encoded_size / original_size if original_size > 0 else float('inf')

  # Check if the decoded sequence matches the original
  match = bit_sequence == decoded_sequence

  # Show the results
  print({
      'compression_rate': compression_rate,
      'match': match,
      'first_10_differences': [
          (i, int(bit_sequence[i]), int(decoded_sequence[i]))
          for i in range(sequence_length)
          if bit_sequence[i] != decoded_sequence[i]
      ][:10]
  })

if __name__ == '__main__':
  main()