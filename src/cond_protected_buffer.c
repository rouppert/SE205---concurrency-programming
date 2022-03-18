#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "circular_buffer.h"
#include "protected_buffer.h"
#include "utils.h"

// Initialize the protected buffer structure above. 
protected_buffer_t * cond_protected_buffer_init(int length) {
  protected_buffer_t * b;
  b = (protected_buffer_t *)malloc(sizeof(protected_buffer_t));
  b->buffer = circular_buffer_init(length);
  b->not_empty = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  b->not_full = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  b->m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  pthread_cond_init(b->not_empty, NULL);
  pthread_cond_init(b->not_full, NULL);
  pthread_mutex_init(b->m, NULL);
  return b;
}

// Extract an element from buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void * cond_protected_buffer_get(protected_buffer_t * b){
  void * d;
  
  // Enter mutual exclusion
  pthread_mutex_lock(b->m);
  // Wait until there is a full slot to get data from the unprotected
  // circular buffer (circular_buffer_get).
  while (b->buffer->size==0) {
    pthread_cond_wait(b->not_empty, b->m);
  }
  
  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_broadcast(b->not_full);

  d = circular_buffer_get(b->buffer);
  print_task_activity ("get", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(b->m);
  
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, the method call blocks until it is.
void cond_protected_buffer_put(protected_buffer_t * b, void * d){

  // Enter mutual exclusion
  pthread_mutex_lock(b->m);
  // Wait until there is an empty slot to put data in the unprotected
  // circular buffer (circular_buffer_put).
  while (b->buffer->size==b->buffer->max_size) {
    pthread_cond_wait(b->not_full, b->m);
  }
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  pthread_cond_broadcast(b->not_empty);

  circular_buffer_put(b->buffer, d);
  print_task_activity ("put", d);

  // Leave mutual exclusion
  pthread_mutex_unlock(b->m);

}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, return NULL. Otherwise, return the element.
void * cond_protected_buffer_remove(protected_buffer_t * b){
  void * d;
  
  pthread_mutex_lock(b->m);
  // Signal or broadcast that an empty slot is available in the
  // unprotected circular buffer (if needed)

  d = circular_buffer_get(b->buffer);
  if (d!=NULL) pthread_cond_broadcast(b->not_full);
  print_task_activity ("remove", d);
  pthread_mutex_unlock(b->m);
  return d;
}

// Insert an element into buffer. If the attempted operation is
// not possible immedidately, return 0. Otherwise, return 1.
int cond_protected_buffer_add(protected_buffer_t * b, void * d){
  int done;
  
  // Enter mutual exclusion
  pthread_mutex_lock(b->m);
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  done = circular_buffer_put(b->buffer, d);
  if (!done) d = NULL;
  else pthread_cond_broadcast(b->not_empty);
  print_task_activity ("add", d);
  // Leave mutual exclusion
  pthread_mutex_unlock(b->m);
  return done;
}

// Extract an element from buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return the element if
// successful. Otherwise, return NULL.
void * cond_protected_buffer_poll(protected_buffer_t * b, struct timespec *abstime){
  void * d = NULL;
  
  // Enter mutual exclusion
  pthread_mutex_lock(b->m);
  // Wait until there is an empty slot to put data in the unprotected
  // circular buffer (circular_buffer_put) but waits no longer than
  // the given timeout.
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  int wait = 0;
  while (b->buffer->size==0 && wait==0) {
    wait = pthread_cond_timedwait(b->not_empty, b->m, abstime);
  }
  d = circular_buffer_get(b->buffer);
  if (d!=NULL) pthread_cond_broadcast(b->not_full);
  print_task_activity ("poll", d);
  // Leave mutual exclusion
  pthread_mutex_unlock(b->m);
  return d;
}

// Insert an element into buffer. If the attempted operation is not
// possible immedidately, the method call blocks until it is, but
// waits no longer than the given timeout. Return 0 if not
// successful. Otherwise, return 1.
int cond_protected_buffer_offer(protected_buffer_t * b, void * d, struct timespec * abstime){
  int done = 0;
  
  // Enter mutual exclusion
  pthread_mutex_lock(b->m);
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed) but waits no longer than
  // the given timeout.
  // Signal or broadcast that a full slot is available in the
  // unprotected circular buffer (if needed)
  int wait = 0;
  while (b->buffer->size==b->buffer->max_size && wait==0) {
    wait = pthread_cond_timedwait(b->not_full, b->m, abstime);
  }
  if (wait==-1) return NULL;
  else {
    done = circular_buffer_put(b->buffer, d);
    if (!done) d = NULL;
    else pthread_cond_broadcast(b->not_empty);
    print_task_activity ("offer", d);
      
    // Leave mutual exclusion
    pthread_mutex_unlock(b->m);
    return done;
  }
}