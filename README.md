# glorious

**glorious** is a high-performance Python library for efficient arithmetic coding, ideal for lossless data compression tasks. Built with **optimized C code under the hood and seamlessly bound to Python**, it delivers exceptional speed and efficiency. By offering customizable context lengths, it allows you to optimize compression based on the unique patterns within your data.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Getting Started](#getting-started)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Testing](#testing)
- [Building the Package](#building-the-package)
- [Under the Hood](#under-the-hood)
- [Contact](#contact)

## Features

- **Optimized C Core**: Leveraging C for core computations ensures maximum performance and efficiency.
- **High Performance**: **glorious** efficiently handles both large and small byte sequences with speed.
- **Customizable Context Length**: Adjust the context length to enhance compression ratios tailored to your data's patterns.
- **Supports Non-ASCII Bytes**: Fully compatible with various data types, including binary data and non-ASCII bytes.
- **Cross-Platform**: Operates seamlessly on Windows, macOS, and Linux systems.
- **Comprehensive Testing**: Extensive unit tests ensure reliability and stability across different use cases.
- **Memory Safety**: Includes memory leak detection with Valgrind support on Unix-like systems, ensuring robust memory management.

## Installation

### From Source

#### Prerequisites

- Python 3.10 or higher
- GCC or a compatible C compiler
- [Poetry](https://python-poetry.org/docs/#installation) for dependency management

#### Steps

1. **Clone the Repository:**

   ```bash
   git clone https://github.com/yourusername/glorious.git
   cd glorious
   ```

2. **Install Dependencies:**

   ```bash
   poetry install
   ```

3. **Activate the Virtual Environment:**

   ```bash
   poetry shell
   ```

4. **Build and Install the Package:**

   ```bash
   poetry run make install
   ```

   Alternatively, use the provided Makefile:

   ```bash
   make install
   ```

   This will compile the C extension and install the package locally.

## Getting Started

Here's a quick guide to get you up and running with glorious.

### Encoding

```python
import glorious as gl

# Sample byte sequence
sequence = b"Hello, Glorious Coding!"

# Define bit length and context length
sequence_bit_length = len(sequence) * 8
context_length = 5

# Encode the sequence
encoded = gl.encode(sequence, sequence_bit_length, context_length)
print(f"Encoded Data: {encoded}")
```

### Decoding

```python
import glorious as gl

# 'encoded' is obtained from the encode function
# For example purposes, let's assume 'encoded' is already defined
# encoded = b'\x9b...'  # Example encoded data

# Define the bit length and context length used during encoding
decoded_bit_length = len(sequence) * 8
context_length = 5

# Decode the sequence
decoded = gl.decode(encoded, decoded_bit_length, context_length)
print(f"Decoded Data: {decoded}")
```

## API Reference

### encode

```python
def encode(sequence: bytes, sequence_bit_length: int, context_length: int) -> bytes:
    """
    Encode a byte sequence using arithmetic coding.

    Parameters:
        sequence (bytes): The input byte sequence to encode.
        sequence_bit_length (int): The bit length of the input sequence.
        context_length (int): The context length used for encoding.

    Returns:
        bytes: The encoded byte sequence.
    """
```

### decode

```python
def decode(encoded: bytes, decoded_bit_length: int, context_length: int) -> bytes:
    """
    Decode an arithmetic-coded byte sequence.

    Parameters:
        encoded (bytes): The encoded byte sequence to decode.
        decoded_bit_length (int): The bit length of the decoded sequence.
        context_length (int): The context length used for decoding.

    Returns:
        bytes: The decoded byte sequence.
    """
```

## Examples

### Example 1: Text Compression

Compressing text data can significantly reduce storage and transmission costs.

```python
import glorious as gl

text = "The quick brown fox jumps over the lazy dog."
data = text.encode('utf-8')

# Encoding
encoded_data = gl.encode(data, len(data) * 8, context_length=4)

# Decoding
decoded_data = gl.decode(encoded_data, len(data) * 8, context_length=4)
decoded_text = decoded_data.decode('utf-8')

print(f"Decoded Text: {decoded_text}")
```

### Example 2: Binary Data

Efficiently compress and decompress binary data streams.

```python
import glorious as gl
import os

# Generate random binary data
binary_data = os.urandom(1024)  # 1KB of random data

# Encoding
encoded_binary = gl.encode(binary_data, len(binary_data) * 8, context_length=6)

# Decoding
decoded_binary = gl.decode(encoded_binary, len(binary_data) * 8, context_length=6)

assert decoded_binary == binary_data
print("Decoding successful, data integrity verified.")
```

## Testing

Run the test suite to verify that everything is working correctly:

```bash
make test
```

Or using Poetry:

```bash
poetry run python -m unittest discover tests
```

The tests cover:

- Varied Sequence Sizes: Testing with different sizes ensures the algorithm scales properly.
- Diverse Data Types: Validating with non-ASCII bytes and binary data confirms broad compatibility.
- Different Context Lengths: Checking multiple context lengths guarantees flexibility in compression settings.
- Memory Leak Detection: Utilizing Valgrind on Unix-like systems to ensure memory safety and integrity.
- C Integration Tests: Confirming that the C modules are correctly compiled and interfaced with Python.

## Building the Package

To build the package wheel for distribution:

```bash
make wheel
```

The wheel file will be available in the `dist/` directory.

### Compiling the C Extension

The core of glorious is written in optimized C for maximum performance. The build process compiles the C code into a Python extension module.

If you're modifying the C source code or need to compile on a different platform, ensure you have a compatible C compiler installed (e.g., GCC on Linux, clang on macOS, or Visual Studio Build Tools on Windows).

## Under the Hood

At the heart of glorious lies a highly optimized C implementation of arithmetic coding algorithms. By leveraging the efficiency of C, the library achieves superior performance compared to pure Python implementations.

The Python bindings are created using Python's C API, allowing for seamless integration and ease of use within your Python codebase. This combination provides the best of both worlds: the speed of C with the simplicity and flexibility of Python.

### Why C?

- **Performance**: C provides low-level access to memory and processor instructions, enabling high-speed computations essential for data compression algorithms.
- **Efficiency**: Arithmetic coding requires handling of bits and bytes at a granular level. C allows for precise control over data structures and memory.
- **Portability**: C code can be compiled on virtually any platform, ensuring that glorious is cross-platform compatible.

### Python Bindings

The C code is wrapped with Python bindings so you can:

- **Use Python Syntax**: Enjoy the ease of Python's syntax while benefiting from the speed of C.
- **Integrate Easily**: Incorporate the library into existing Python projects without worrying about low-level C code.
- **Extend Functionality**: Advanced users can modify or extend the C code and recompile to customize the library for specific needs.

## Contact

Andy Chen

- GitHub: @andersenchen
