// arithmetic_coding_bindings.c

#define PY_SSIZE_T_CLEAN
#include "arithmetic_coding_bindings.h"

#include <Python.h>
#include <stdlib.h>
#include <string.h>

// Include the necessary C headers
#include "arithmetic_coding.h"
#include "probability.h"

// Expose the get_probability_wrapper function
uint32_t get_probability_wrapper(const uint8_t *context,
                                 size_t context_length) {
  return example_get_probability_fixed(context, context_length);
}

// Python wrapper for arithmetic_encode with bit length support
static PyObject *py_arithmetic_encode(PyObject *self, PyObject *args) {
  (void)self;  // Suppress unused parameter warning

  const char *sequence;
  Py_ssize_t sequence_length;
  Py_ssize_t sequence_bit_length;  // New parameter for bit length
  Py_ssize_t context_length;

  // Parse Python arguments (now includes bit length)
  if (!PyArg_ParseTuple(args, "y#nn", &sequence, &sequence_length,
                        &sequence_bit_length, &context_length)) {
    return NULL;
  }

  if (context_length <= 0) {
    PyErr_SetString(PyExc_ValueError, "context_length must be positive.");
    return NULL;
  }

  // Call the C arithmetic_encode function with the bit length
  uint8_t *encoded_output = NULL;
  size_t encoded_length = arithmetic_encode(
      (const uint8_t *)sequence,
      (size_t)sequence_bit_length,  // Use the bit length directly
      &encoded_output, (size_t)context_length, get_probability_wrapper);

  if (encoded_length == 0 || encoded_output == NULL) {
    PyErr_SetString(PyExc_RuntimeError, "Encoding failed.");
    return NULL;
  }

  PyObject *result =
      PyBytes_FromStringAndSize((const char *)encoded_output, encoded_length);
  free(encoded_output);
  return result;
}

// Python wrapper for arithmetic_decode with bit length support
static PyObject *py_arithmetic_decode(PyObject *self, PyObject *args) {
  (void)self;  // Suppress unused parameter warning

  const char *encoded;
  Py_ssize_t encoded_length;
  Py_ssize_t decoded_bit_length;  // New parameter for decoded bit length
  Py_ssize_t context_length;

  // Parse Python arguments (includes decoded bit length)
  if (!PyArg_ParseTuple(args, "y#nn", &encoded, &encoded_length,
                        &decoded_bit_length, &context_length)) {
    return NULL;
  }

  if (context_length <= 0) {
    PyErr_SetString(PyExc_ValueError, "context_length must be positive.");
    return NULL;
  }

  // Allocate space for the decoded output based on bit length
  size_t decoded_byte_length = (decoded_bit_length + 7) / 8;
  uint8_t *decoded_output =
      (uint8_t *)calloc(decoded_byte_length, sizeof(uint8_t));
  if (!decoded_output) {
    PyErr_NoMemory();
    return NULL;
  }

  // Call the C arithmetic_decode function with the decoded bit length
  arithmetic_decode((const uint8_t *)encoded, (size_t)encoded_length,
                    decoded_output, (size_t)decoded_bit_length,
                    (size_t)context_length, get_probability_wrapper);

  PyObject *result = PyBytes_FromStringAndSize((const char *)decoded_output,
                                               decoded_byte_length);
  free(decoded_output);
  return result;
}

// Module method definitions with updated docstrings
PyMethodDef ArithmeticCodingMethods[] = {
    {"encode", py_arithmetic_encode, METH_VARARGS,
     "encode(sequence: bytes, sequence_bit_length: int, context_length: int) "
     "-> bytes\n\n"
     "Encodes a byte sequence using arithmetic coding.\n\n"
     "Parameters:\n"
     "  sequence (bytes): The input byte sequence to encode.\n"
     "  sequence_bit_length (int): The bit length of the input sequence.\n"
     "  context_length (int): The length of the context used for encoding.\n\n"
     "Returns:\n"
     "  bytes: The encoded byte sequence."},
    {"decode", py_arithmetic_decode, METH_VARARGS,
     "decode(encoded: bytes, decoded_bit_length: int, context_length: int) -> "
     "bytes\n\n"
     "Decodes an arithmetic-coded byte sequence.\n\n"
     "Parameters:\n"
     "  encoded (bytes): The encoded byte sequence to decode.\n"
     "  decoded_bit_length (int): The bit length of the decoded sequence.\n"
     "  context_length (int): The length of the context used for decoding.\n\n"
     "Returns:\n"
     "  bytes: The decoded byte sequence."},
    {NULL, NULL, 0, NULL}  // Sentinel
};

// Module definition
struct PyModuleDef arithmeticcodingmodule = {
    PyModuleDef_HEAD_INIT,
    "arithmetic_coding_bindings",             /* Module name */
    "Python bindings for arithmetic coding.", /* Module documentation */
    -1, /* Size of per-interpreter state of the module */
    ArithmeticCodingMethods,
    NULL, /* m_slots */
    NULL, /* m_traverse */
    NULL, /* m_clear */
    NULL  /* m_free */
};

// Module initialization
PyMODINIT_FUNC PyInit_glorious(void) {
  return PyModule_Create(&arithmeticcodingmodule);
}
