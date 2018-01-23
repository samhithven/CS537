#include "types.h"
#include "stat.h"
#include "user.h"

#define COUNT 12
#define TIME 10

void thread_code(void *arg) {
  int N = *(int*) arg;
  int i = 0;
  printf(1, "Thread is working n = %d &i = %d(%p)\n\n", N,i,&i);
  for(i = 0; i < N; i++) {
    printf(1, "Thread | i = %d | &i = %p\n", i, &i);
  }
  exit();
}

int main(int argc, char *argv[]) {
  int tid = 0;
  int N = COUNT;
  printf(1,"Thread test started\n");
  tid = thread_create(thread_code, (void*) &N);

  for(int i = 0; i < COUNT; i++) {
    printf(1, "%s: %d and %d\n", __func__, i,tid);
  }
  printf(1, "Parent Sleeping\n");
  //thread_join(tid);
  printf(1, "Sleeping done\n");
  exit();
}

