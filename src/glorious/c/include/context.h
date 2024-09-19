// context.h

#ifndef CONTEXT_H
#define CONTEXT_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Structure holding context information for probability functions.
 */
typedef struct {
  int count_ones;         ///< Count of '1's in the context.
  size_t context_length;  ///< Length of the context in bits.
  // Additional fields can be added here for more complex contexts.
} ContextContent;

#endif  // CONTEXT_H
