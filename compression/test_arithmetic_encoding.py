import unittest
import numpy as np
from bitarray import bitarray
from typing import Callable

# Import the functions to be tested
from arithmetic_coding import arithmetic_encode_finite, arithmetic_decode_finite

class TestArithmeticCoding(unittest.TestCase):
  def setUp(self):
    self.precision = 32
    self.context_window = 10

  def generate_sequence(self, length: int, p: float) -> bitarray:
    return bitarray(np.random.binomial(1, p, length).tolist())

  def constant_p1_func(self, p: float) -> Callable[[bitarray], float]:
    return lambda context: p

  def run_encode_decode(self, sequence: bitarray, p1_func: Callable[[bitarray], float]) -> bitarray:
    encoded = arithmetic_encode_finite(sequence, p1_func, self.precision, self.context_window)
    return arithmetic_decode_finite(encoded, p1_func, len(sequence), self.precision, self.context_window)

  def test_short_sequences(self):
    p = 0.2
    for length in range(1, 33):  # Test sequences from length 1 to 32
      with self.subTest(length=length):
        sequence = self.generate_sequence(length, p)
        p1_func = self.constant_p1_func(p)
        decoded = self.run_encode_decode(sequence, p1_func)
        self.assertEqual(sequence, decoded)

  def test_various_lengths(self):
    p = 0.2
    for length in [10, 100, 1000, 10000]:
      with self.subTest(length=length):
        sequence = self.generate_sequence(length, p)
        p1_func = self.constant_p1_func(p)
        decoded = self.run_encode_decode(sequence, p1_func)
        self.assertEqual(sequence, decoded)

  def test_various_bernoulli_probs(self):
    length = 10000
    for p in [0.1, 0.3, 0.5, 0.7, 0.9]:
      with self.subTest(p=p):
        sequence = self.generate_sequence(length, p)
        p1_func = self.constant_p1_func(p)
        decoded = self.run_encode_decode(sequence, p1_func)
        self.assertEqual(sequence, decoded)

  def test_mismatched_probs(self):
    length = 10000
    sampling_p = 0.3
    encoding_p = 0.7
    sequence = self.generate_sequence(length, sampling_p)
    p1_func = self.constant_p1_func(encoding_p)
    decoded = self.run_encode_decode(sequence, p1_func)
    self.assertEqual(sequence, decoded)

  def test_edge_cases(self):
    length = 1000
    edge_cases = [
      (0.0, bitarray('0' * length)),
      (1.0, bitarray('1' * length)),
      (0.5, bitarray('01' * (length // 2))),
    ]
    for p, sequence in edge_cases:
      with self.subTest(p=p):
        p1_func = self.constant_p1_func(p)
        decoded = self.run_encode_decode(sequence, p1_func)
        self.assertEqual(sequence, decoded)

  def test_long_sequence(self):
    length = 100000
    p = 0.2
    sequence = self.generate_sequence(length, p)
    p1_func = self.constant_p1_func(p)
    decoded = self.run_encode_decode(sequence, p1_func)
    self.assertEqual(sequence, decoded)

  def test_very_low_probability(self):
    length = 10000
    p = 0.01
    sequence = self.generate_sequence(length, p)
    p1_func = self.constant_p1_func(p)
    decoded = self.run_encode_decode(sequence, p1_func)
    self.assertEqual(sequence, decoded)

  def test_very_high_probability(self):
    length = 10000
    p = 0.99
    sequence = self.generate_sequence(length, p)
    p1_func = self.constant_p1_func(p)
    decoded = self.run_encode_decode(sequence, p1_func)
    self.assertEqual(sequence, decoded)

  def test_alternating_bits(self):
    length = 10000
    sequence = bitarray('10' * (length // 2))
    p1_func = self.constant_p1_func(0.5)
    decoded = self.run_encode_decode(sequence, p1_func)
    self.assertEqual(sequence, decoded)

  def test_compression_rate(self):
    length = 100000
    p = 0.1
    sequence = self.generate_sequence(length, p)
    p1_func = self.constant_p1_func(p)
    encoded = arithmetic_encode_finite(sequence, p1_func, self.precision, self.context_window)
    compression_rate = len(encoded) / len(sequence)
    self.assertLess(compression_rate, 1.0, "Expected compression for low entropy sequence")

  def test_different_precision(self):
    length = 1000
    p = 0.2
    for precision in [16, 24, 40]:
      with self.subTest(precision=precision):
        sequence = self.generate_sequence(length, p)
        p1_func = self.constant_p1_func(p)
        encoded = arithmetic_encode_finite(sequence, p1_func, precision, self.context_window)
        decoded = arithmetic_decode_finite(encoded, p1_func, length, precision, self.context_window)
        self.assertEqual(sequence, decoded)

  def test_different_context_window(self):
    length = 1000
    p = 0.2
    for context_window in [5, 15, 20]:
      with self.subTest(context_window=context_window):
        sequence = self.generate_sequence(length, p)
        p1_func = self.constant_p1_func(p)
        encoded = arithmetic_encode_finite(sequence, p1_func, self.precision, context_window)
        decoded = arithmetic_decode_finite(encoded, p1_func, length, self.precision, context_window)
        self.assertEqual(sequence, decoded)

if __name__ == '__main__':
  unittest.main()