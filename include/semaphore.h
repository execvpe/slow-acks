#pragma once
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <pthread.h> // pthread_mutex_t
#include <stdbool.h> // bool

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t condition;

	volatile size_t value;
} semaphore_t;

/**
 * @brief Creates a new semaphore.
 *
 * This function creates a new semaphore. If an error occurs during the
 * initialization, the implementation frees all resources already allocated by
 * then and sets @c errno to an appropriate value.
 *
 * It is legal to initialize the semaphore with a negative value. If this is the
 * case, in order to reset the semaphore counter to zero, the V-operation must be
 * performed @c (-initVal) times.
 *
 * @param initVal The initial value of the semaphore.
 * @return Handle for the created semaphore, or @c NULL if an error occurred.
 */
semaphore_t *sem_create(size_t initVal);

/**
 * @brief Destroys a semaphore and frees all associated resources.
 * @param sem Handle of the semaphore to destroy. If a @c NULL pointer is
 *            passed, the implementation does nothing.
 */
void sem_destroy(semaphore_t *sem);

/**
 * @brief P-operation.
 *
 * Attempts to decrement the semaphore value by 1. If the semaphore value is not a
 * positive number, the operation fails until a V-operation increments the value
 * and the P-operation succeeds.
 *
 * @param sem Handle of the semaphore to decrement.
 * @return true if the semaphore value could be decremented, false if not.
 */
bool sem_decrement_nonblock(semaphore_t *sem);

/**
 * @brief P-operation.
 *
 * Attempts to decrement the semaphore value by 1. If the semaphore value is not a
 * positive number, the operation blocks until a V-operation increments the value
 * and the P-operation succeeds.
 *
 * @param sem Handle of the semaphore to decrement.
 */
void sem_decrement(semaphore_t *sem);

/**
 * @brief V-operation.
 *
 * Increments the semaphore value by 1 and notifies P-operations that are
 * blocked on the semaphore of the change.
 *
 * @param sem Handle of the semaphore to increment.
 */
void sem_increment(semaphore_t *sem);

#endif // SEMAPHORE_H
