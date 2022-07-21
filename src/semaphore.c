#include "semaphore.h"

#include <errno.h>   // errno
#include <pthread.h> // pthread_mutex_init()
#include <stdlib.h>  // calloc()

semaphore_t *sem_create(size_t initVal) {
	semaphore_t *const sem = calloc(1, sizeof(semaphore_t)); // returns {"void *", NULL}
	if (sem == NULL) {
		return NULL; // errno set by calloc
	}

	int e = pthread_mutex_init(&sem->mutex, NULL); // returns {0}
	if (e) {                                       // Should never happen according to manual page
		errno = e;
		free(sem);
		return NULL;
	}

	e = pthread_cond_init(&sem->condition, NULL); // returns {0}
	if (e) {                                      // Should never happen according to manual page
		errno = e;
		pthread_mutex_destroy(&sem->mutex);
		// The return value of the above function is deliberately NOT stored in the
		// errno variable in order not to overwrite its previous value.
		free(sem);
		return NULL;
	}

	sem->value = initVal;

	return sem;
}

void sem_destroy(semaphore_t *sem) {
	if (sem == NULL)
		return;

	int e = pthread_cond_destroy(&sem->condition); // returns {0, EBUSY}
	if (e) {
		errno = e;
		pthread_mutex_destroy(&sem->mutex); // returns {0, EBUSY}
		// The return value of the above function is deliberately NOT stored in the
		// errno variable in order not to overwrite its previous value.
		free(sem);
		return;
	}

	e = pthread_mutex_destroy(&sem->mutex); // returns {0, EBUSY}
	if (e) {
		errno = e;
	}

	free(sem);
}

bool sem_decrement_nonblock(semaphore_t *sem) {
	pthread_mutex_lock(&sem->mutex);

	if (sem->value <= 0) {
		pthread_mutex_unlock(&sem->mutex);
		return false;
	}

	(sem->value)--;

	pthread_mutex_unlock(&sem->mutex);
	return true;
}

void sem_decrement(semaphore_t *sem) {
	pthread_mutex_lock(&sem->mutex);

	while (sem->value <= 0) {
		pthread_cond_wait(&sem->condition, &sem->mutex);
	}

	(sem->value)--;

	pthread_mutex_unlock(&sem->mutex);
}

void sem_increment(semaphore_t *sem) {
	pthread_mutex_lock(&sem->mutex);

	(sem->value)++;

	pthread_cond_broadcast(&sem->condition);
	pthread_mutex_unlock(&sem->mutex);
}
