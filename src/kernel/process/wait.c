#include <kernel/process/wait.h>

static inline void __add_wait_queue(wait_queue_head_t *head,
                                    wait_queue_entry_t *wq) {
  list_head_insert_before(&wq->task_list, &head->task_list);
}

static inline void __remove_wait_queue(wait_queue_head_t *head,
                                       wait_queue_entry_t *wq) {
  list_head_remove(&wq->task_list);
}

void init_waitqueue_entry(wait_queue_entry_t *wq, struct task_struct *task) {
  wq->flags = 0;
  wq->task  = task;
  wq->func  = default_wake_function;
}

void add_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq) {
  wq->flags &= ~WQ_FLAG_EXCLUSIVE;
  spinlock_lock(&head->lock);
  __add_wait_queue(head, wq);
  spinlock_unlock(&head->lock);
}

void remove_wait_queue(wait_queue_head_t *head, wait_queue_entry_t *wq) {
  spinlock_lock(&head->lock);
  __remove_wait_queue(head, wq);
  spinlock_unlock(&head->lock);
}
