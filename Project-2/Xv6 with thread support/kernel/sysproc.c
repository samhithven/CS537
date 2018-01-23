#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "sysfunc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//Thread implementation changes

int sys_clone(void) {
  void *fcn, *arg, *stack;

  //Obtain func name
  if(argptr(0, (void*)&fcn, sizeof(void *)) < 0)
    return -1;

  //Obtain args
  if(argptr(1, (void*)&arg, sizeof(void *)) < 0)
    return -1;

  //Obtain stack address
  if(argptr(2, (void*)&stack, sizeof(void *)) < 0) 
    return -1;

  //Call clone 
  int tid = clone(fcn, arg, stack);
  return tid;
}

int sys_join(void) {
  void **stack;

  if(argptr(0, (void*)&stack, sizeof(void *)) < 0)
    return -1;
  
  int ret_val = join(stack);

  return ret_val;
}

int sys_sleepcv(void) {
  void *cv;
  lock_t *lock;

  if(argptr(0, (void *)&cv, sizeof(void *)) < 0)
    return -1;

  if(argptr(1, (void *)&lock, sizeof(void *)) < 0)
    return -1;

  sleep2(cv,lock);

  return 0;
}

int sys_wakecv(void) {
  void *cv;

  if(argptr(0, (void *)&cv, sizeof(void *)) < 0)
    return -1;

  wakeup2(cv);

  return 0;
}
