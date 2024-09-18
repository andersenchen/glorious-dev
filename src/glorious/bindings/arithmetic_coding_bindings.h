// arithmetic_coding_bindings.h

#ifndef ARITHMETIC_CODING_BINDINGS_H
#define ARITHMETIC_CODING_BINDINGS_H

#include <Python.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Wrapper function for getting the fixed-point probability.
 *
 * This function wraps the C-level probability function to match the expected
 * signature for arithmetic encoding and decoding.
 *
 * @param context Pointer to the context bit array.
 * @param context_length Length of the context in bits.
 * @return uint32_t Fixed-point probability of the current bit being '1', scaled
 * by FIXED_SCALE.
 */
uint32_t get_probability_wrapper(const uint8_t *context, size_t context_length);

/**
 * @brief Python wrapper for arithmetic_encode.
 *
 * Encodes a byte sequence using arithmetic coding.
 *
 * Parameters:
 *   sequence (bytes): The input byte sequence to encode.
 *   sequence_bit_length (int): The bit length of the input sequence.
 *   context_length (int): The length of the context used for encoding.
 *
 * Returns:
 *   bytes: The encoded byte sequence.
 */
static PyObject *py_arithmetic_encode(PyObject *self, PyObject *args);

/**
 * @brief Python wrapper for arithmetic_decode.
 *
 * Decodes an arithmetic-coded byte sequence.
 *
 * Parameters:
 *   encoded (bytes): The encoded byte sequence to decode.
 *   decoded_bit_length (int): The bit length of the decoded sequence.
 *   context_length (int): The length of the context used for decoding.
 *
 * Returns:
 *   bytes: The decoded byte sequence.
 */
static PyObject *py_arithmetic_decode(PyObject *self, PyObject *args);

/**
 * @brief Module method definitions.
 */
extern PyMethodDef ArithmeticCodingMethods[];

/**
 * @brief Module definition.
 */
extern struct PyModuleDef arithmeticcodingmodule;

/**
 * @brief Module initialization function.
 *
 * Initializes the arithmetic_coding_bindings module.
 *
 * @return PyObject* The initialized module object.
 */
PyMODINIT_FUNC PyInit__glorious(void); // Updated to match C file

#endif // ARITHMETIC_CODING_BINDINGS_H
