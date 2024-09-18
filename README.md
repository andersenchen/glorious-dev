# glorious

**glorious** is a high-performance Python library for bitwise arithmetic encoding, combining C's speed with Python's ease of use. It offers cross-platform, blazingly fast lossless data compression with customizable context lengths.

## Installation

```bash
git clone https://github.com/yourusername/glorious.git
cd glorious
make install
```

## Features

- Fast C core with Python bindings
- Cross-platform compatibility (Windows, macOS, Linux)
- Customizable context lengths for optimized compression
- Comprehensive testing suite
- Memory leak detection with Valgrind (Unix-like systems)

## Quick Start

### Encoding

```python
import glorious as gl

sequence = b"Hello, Glorious Coding!"  # Input: bytes
encoded = gl.encode(sequence, len(sequence) * 8, context_length=5)  # Output: bytes
print(f"Encoded Data: {encoded}")
```

### Decoding

```python
decoded = gl.decode(encoded, len(sequence) * 8, context_length=5)  # Input: bytes, Output: bytes
print(f"Decoded Data: {decoded}")
```

## Examples

### Text Compression

```python
import glorious as gl

text = "The quick brown fox jumps over the lazy dog."  # Input: str
data = text.encode('utf-8')  # Convert to bytes

# Encode function takes bytes input, returns bytes output
encoded_data = gl.encode(data, len(data) * 8, context_length=4)

# Decode function takes bytes input, returns bytes output
decoded_data = gl.decode(encoded_data, len(data) * 8, context_length=4)

decoded_text = decoded_data.decode('utf-8')  # Convert back to str
print(f"Decoded Text: {decoded_text}")
```

### Binary Data Compression

```python
import glorious as gl
import os

binary_data = os.urandom(1024)  # 1KB of random bytes

# Both encode and decode functions work with bytes input and output
encoded_binary = gl.encode(binary_data, len(binary_data) * 8, context_length=6)
decoded_binary = gl.decode(encoded_binary, len(binary_data) * 8, context_length=6)

assert decoded_binary == binary_data
print("Decoding successful, data integrity verified.")
```

## Testing

Run the test suite:

```bash
make test
```

## Building the Package

To build the package wheel:

```bash
make wheel
```

The wheel file will be available in the `dist/` directory.

## Contact

Andy Chen

- GitHub: @andersenchen
