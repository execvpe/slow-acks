#include "bounded_buffer.h"

#include <errno.h>   // errno
#include <stdbool.h> // bool
#include <stdlib.h>  // calloc()

bbuf_t *bbuf_create(size_t capacity) {
	bbuf_t *bb = calloc(1, sizeof(bbuf_t) + (capacity * sizeof(bbuf_elem_t)));
	if (bb == NULL) {
		return NULL;
	}

	bb->capacity = capacity;
	bb->newest   = bb->capacity - 1;
	bb->oldest   = 0;

	bb->empty_sem = sem_create(0);
	if (bb->empty_sem == NULL) {
		free(bb);
		return NULL;
	}

	bb->full_sem = sem_create(bb->capacity);
	if (bb->full_sem == NULL) {
		sem_destroy(bb->empty_sem);
		free(bb);
		return NULL;
	}

	bb->lock_sem = sem_create(1);
	if (bb->lock_sem == NULL) {
		sem_destroy(bb->empty_sem);
		sem_destroy(bb->full_sem);
		free(bb);
		return NULL;
	}

	return bb;
}

void bbuf_destroy(bbuf_t *bb) {
	if (bb == NULL) {
		return;
	}

	sem_destroy(bb->empty_sem);
	sem_destroy(bb->full_sem);

	free(bb);
}

bool bbuf_put_nonblock(bbuf_t *bb, bbuf_elem_t value) {
	if (!sem_decrement_nonblock(bb->full_sem)) {
		return false;
	}
	sem_decrement(bb->lock_sem);

	bb->newest++;
	bb->newest %= bb->capacity;

	bb->data[bb->newest] = value;

	sem_increment(bb->lock_sem);
	sem_increment(bb->empty_sem);
	return true;
}

void bbuf_put(bbuf_t *bb, bbuf_elem_t value) {
	// Block if queue is full
	sem_decrement(bb->full_sem);
	sem_decrement(bb->lock_sem);

	bb->newest++;
	bb->newest %= bb->capacity;

	bb->data[bb->newest] = value;

	sem_increment(bb->lock_sem);
	sem_increment(bb->empty_sem);
}

bbuf_elem_t bbuf_get(bbuf_t *bb) {
	// Block if queue is empty
	sem_decrement(bb->empty_sem);
	sem_decrement(bb->lock_sem);

	bbuf_elem_t value = bb->data[bb->oldest];

	bb->oldest++;
	bb->oldest %= bb->capacity;

	sem_increment(bb->lock_sem);
	sem_increment(bb->full_sem);

	return value;
}
