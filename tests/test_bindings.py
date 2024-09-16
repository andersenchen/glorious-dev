import unittest

import glorious


class TestArithmeticCodingBindings(unittest.TestCase):
    def test_large_sequence(self):
        """Test encoding and decoding for a large sequence."""
        sequence = b"a" * (1000000 // 8)  # 1,000,000 bits (125,000 bytes)
        sequence_bit_length = len(sequence) * 8
        context_length_big_test = 50

        encoded = glorious.encode(
            sequence, sequence_bit_length, context_length_big_test
        )
        self.assertIsInstance(encoded, bytes)
        self.assertTrue(len(encoded) > 0, "Encoded output should not be empty.")

        decoded = glorious.decode(encoded, sequence_bit_length, context_length_big_test)
        self.assertEqual(sequence, decoded)

    def test_small_sequence(self):
        """Test encoding and decoding for a small sequence (few bytes)."""
        sequence = b"abc"
        sequence_bit_length = len(sequence) * 8
        context_length = 5

        encoded = glorious.encode(sequence, sequence_bit_length, context_length)
        self.assertIsInstance(encoded, bytes)
        self.assertTrue(len(encoded) > 0, "Encoded output should not be empty.")

        decoded = glorious.decode(encoded, sequence_bit_length, context_length)
        self.assertEqual(sequence, decoded)

    def test_single_byte_sequence(self):
        """Test encoding and decoding of a single byte sequence."""
        sequence = b"a"
        sequence_bit_length = len(sequence) * 8
        context_length = 1

        encoded = glorious.encode(sequence, sequence_bit_length, context_length)
        self.assertIsInstance(encoded, bytes)
        self.assertTrue(len(encoded) > 0, "Encoded output should not be empty.")

        decoded = glorious.decode(encoded, sequence_bit_length, context_length)
        self.assertEqual(sequence, decoded)

    def test_non_ascii_bytes(self):
        """Test encoding and decoding of non-ASCII byte sequences."""
        sequence = bytes([0xFF, 0x00, 0x7F, 0x80])
        sequence_bit_length = len(sequence) * 8
        context_length = 10

        encoded = glorious.encode(sequence, sequence_bit_length, context_length)
        self.assertIsInstance(encoded, bytes)
        self.assertTrue(len(encoded) > 0, "Encoded output should not be empty.")

        decoded = glorious.decode(encoded, sequence_bit_length, context_length)
        self.assertEqual(sequence, decoded)

    def test_large_context_length(self):
        """Test encoding and decoding with a very large context length."""
        sequence = b"test_sequence_large_context"
        sequence_bit_length = len(sequence) * 8
        context_length = 1000  # Large context length

        encoded = glorious.encode(sequence, sequence_bit_length, context_length)
        self.assertIsInstance(encoded, bytes)
        self.assertTrue(len(encoded) > 0, "Encoded output should not be empty.")

        decoded = glorious.decode(encoded, sequence_bit_length, context_length)
        self.assertEqual(sequence, decoded)


if __name__ == "__main__":
    unittest.main()
