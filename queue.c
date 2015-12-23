#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "queue.h"

typedef struct _task_list {
	work_task callback;
	void *data;

	struct _task_list *next;
} task_list;

struct _work_queue {
	task_list *tasks;
	pthread_mutex_t task_list_mutex;

	int worker_count;
	pthread_t *threads;
	pthread_mutex_t barrier_mutex;
	pthread_cond_t barrier;

	bool suspended;
	bool quit;
};

void *thread_start(void *data);

work_queue queue_create(int worker_count) {
	work_queue q = calloc(sizeof(struct _work_queue), 1);
	
	q->worker_count = worker_count;
	q->suspended    = true;

	// setup worker barrier
	pthread_mutex_init(&q->task_list_mutex, NULL);
	pthread_mutex_init(&q->barrier_mutex, NULL);
	pthread_cond_init(&q->barrier, NULL);

	// Create worker threads
	pthread_t *threads = calloc(sizeof(pthread_t), worker_count);
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	for(int i = 0; i < worker_count; i++) {
		if (pthread_create(threads + i, &attr, thread_start, q) != 0) {
			abort();
		}
	}
	pthread_attr_destroy(&attr);

	return q;
}

bool queue_free(work_queue queue) {
	// queue can only be freed if no tasks are queued
	if (queue->tasks) {
		return false;	
	}

	// signal all threads to exit
	queue->quit = true;
	pthread_cond_broadcast(&queue->barrier);

	// wait for all threads to finish
	for(int i = 0; i < queue->worker_count; i++) {
		pthread_join(queue->threads[i], NULL);
	}

	// clean up handle
	pthread_cond_destroy(&queue->barrier);
	pthread_mutex_destroy(&queue->barrier_mutex);
	pthread_mutex_destroy(&queue->task_list_mutex);
	free(queue->threads);
	free(queue);
	return true;
}

void queue_resume(work_queue queue) {
	if (queue->suspended) {
		// resume work, wake all threads
		queue->suspended = false;
		pthread_cond_broadcast(&queue->barrier);
	}
}

void queue_suspend(work_queue queue) {
	// signal threads to sleep
	queue->suspended = true;
}

int queue_taskcount(work_queue queue) {
	pthread_mutex_lock(&queue->task_list_mutex);

	// count items in task list
	int i = 0;
	task_list *task = queue->tasks;
	while ((task = task->next)) {
		i++;
	}
	
	pthread_mutex_unlock(&queue->task_list_mutex);
	return i;
}

void queue_add_task(work_queue queue, work_task task, void *data) {
	pthread_mutex_lock(&queue->task_list_mutex);

	// fetch last list item
	task_list *t = queue->tasks;
	while (t->next != NULL) {
		t = t->next;
	}

	// create new item
	task_list *new_task = calloc(sizeof(task_list), 1);
	new_task->callback = task;
	new_task->data = data;

	t->next = new_task;

	// signal one thread to pick it up
	pthread_mutex_unlock(&queue->task_list_mutex);
	if (!queue->suspended) {
		pthread_cond_signal(&queue->barrier);
	}
}

//
// Internal
//

static task_list *queue_fetch_task(work_queue q) {
	// aquire lock
	pthread_mutex_lock(&q->task_list_mutex);

	// if task list is empty return NULL
	if (q->tasks == NULL) {
		pthread_mutex_unlock(&q->task_list_mutex);
		return NULL;
	}

	// fetch a task from the queue
	task_list *task = q->tasks;
	q->tasks = q->tasks->next;
	task->next = NULL;

	// unlock mutex and return task
	pthread_mutex_unlock(&q->task_list_mutex);
	return task;
}

void *thread_start(void *data) {
	work_queue q = (work_queue)data;

	while (42) {
		// quit signal, join main thread
		if (q->quit) {
			pthread_exit(NULL);
		}

		// if queue is suspended sleep immediately
		if (q->suspended) {
			pthread_cond_wait(&q->barrier, &q->barrier_mutex);
		}

		// fetch work from task list, run the callback and free the task
		task_list *task = queue_fetch_task(q);
		if (task) {
			task->callback(task->data);
			free(task);
		} else { 
			// queue empty go to sleep
			pthread_cond_wait(&q->barrier, &q->barrier_mutex);
		}
	}
}
