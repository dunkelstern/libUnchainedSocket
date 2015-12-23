#ifndef __queue_h
#define __queue_h

/** Opaque work queue handle */
typedef struct _work_queue *work_queue;

/** Worker callback function pointer type */
typedef void (*work_task)(void *data);

/** Create a new work queue
 *
 * A new queue starts in suspended state, so don't forget to resume
 * @param worker_count: maximum parallel worker threads
 * @return new work queue handle
 */
work_queue queue_create(int worker_count);

/** Free a work queue
 *
 * Freeing is only possible if there are no tasks queued.
 * Function blocks until all threads have finished
 * @param queue: The queue to free, handle will be invalid after this call
 * @returns true if the queue could be freed, false if there were tasks queued.
 */
bool queue_free(work_queue queue);

/** Resume work
 *
 * @param queue: Queue to resume
 */
void queue_resume(work_queue queue);

/** Suspend work
 *
 * @param queue: Queue to suspend
 */
void queue_suspend(work_queue queue);

/** Fetch task count currently queued
 * 
 * @attention counting blocks the queue, running tasks are not included in count
 * @param queue: The queue to query
 * @returns number of items queued
 */
int queue_taskcount(work_queue queue);

/** Add task to queue
 *
 * @attention the task has to manage the memory it gets with the data pointer!
 * @param queue: The queue to add to
 * @param task: callback
 * @param data: data to send to the callback
 */
void queue_add_task(work_queue queue, work_task task, void *data);

#endif /* __queue_h */
