
#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#define PGSIZE (4096)

//Thread create API
int thread_create(void (*fcn)(void*), void *arg) {
  void *stack;
  stack = (void *) malloc(PGSIZE);
  if(stack != NULL)
    return clone(fcn, arg, stack);
  else
    return -1;
}

//Thread join
int thread_join() {
  void *stack;
  int ret = join(&stack);
  free(stack);
  return ret;
}

//Initializing lock
void lock_init(lock_t *lock) {
  lock->flag = 0;
}

//Take lock by setting it to 1
void lock_acquire(lock_t *lock) {
  while(xchg(&lock->flag, 1) != 0);
}

//Release lock by setting it to 0
void lock_release(lock_t *lock) {
  xchg(&lock->flag, 0);
}

