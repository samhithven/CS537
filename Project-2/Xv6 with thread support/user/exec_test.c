#include "types.h"
#include "stat.h"
#include "user.h"

char *echoargv[] = { "echo", "Samhith", "It", "Passed", 0};

int main (int argc, char *argv[]) {
  if(fork() == 0) {
    printf(1, "Child\n");
    exec("thread_test", echoargv);
    printf(1, "Exec Failed\n");
    exit();
  } else {
    wait();
    printf(1, "parent\n");
  }
  exit();
}

