#pragma once
#ifndef BOUNDED_BUFFER_H
#define BOUNDED_BUFFER_H

#include <stdbool.h> // bool

#include "semaphore.h" // semaphore_t

typedef void *bbuf_elem_t;

typedef struct {
	semaphore_t *empty_sem; // empty at the beginning
	semaphore_t *full_sem;  // empty when queue is full
	semaphore_t *lock_sem;

	size_t capacity;

	volatile size_t newest;
	volatile size_t oldest;

	bbuf_elem_t data[];
} bbuf_t;

/**
 * @brief Creates a new bounded buffer.
 *
 * This function creates a new bounded buffer and all the required helper data
 * structures, including semaphores for synchronization. If an error occurs
 * during the initialization, the implementation frees all resources already
 * allocated by then.
 *
 * @param capacity The number of values that can be stored in the bounded buffer.
 * @return Handle for the created bounded buffer, or @c NULL if an error
 *         occurred.
 */
bbuf_t *bbuf_create(size_t capacity);

/**
 * @brief Destroys a bounded buffer.
 *
 * All resources associated with the bounded buffer are released.
 *
 * @param bb Handle of the bounded buffer that shall be freed. If a @c NULL
 *           pointer is passed, the implementation does nothing.
 */
void bbuf_destroy(bbuf_t *bb);

/**
 * @brief Adds an element to a bounded buffer.
 *
 * This function adds an element to a bounded buffer. If the buffer is full, the
 * function fails until an element has been removed from it.
 *
 * @param bb    Handle of the bounded buffer.
 * @param value Value that shall be added to the buffer.
 * @return true if an element was added, false if not.
 */
bool bbuf_put_nonblock(bbuf_t *bb, bbuf_elem_t value);

/**
 * @brief Adds an element to a bounded buffer.
 *
 * This function adds an element to a bounded buffer. If the buffer is full, the
 * function blocks until an element has been removed from it.
 *
 * @param bb    Handle of the bounded buffer.
 * @param value Value that shall be added to the buffer.
 */
void bbuf_put(bbuf_t *bb, bbuf_elem_t value);

/**
 * @brief Retrieves an element from a bounded buffer.
 *
 * This function removes an element from a bounded buffer. If the buffer is
 * empty, the function blocks until an element has been added.
 *
 * @param bb Handle of the bounded buffer.
 * @return The element.
 */
bbuf_elem_t bbuf_get(bbuf_t *bb);

#endif // BOUNDED_BUFFER_H
