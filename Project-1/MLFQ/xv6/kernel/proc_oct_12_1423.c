#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc * q[4][NPROC] ;
} ptable;

static struct proc *initproc;

/*verbose flag
int verbose = 0;
int fv = 0;
*/
int waitv =0;
//int fifov = 1;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

//maximum time slice
int MAX_TS[4] = {0,32,16,8};

//maximum wait time slice
int MAX_WAIT_TS[4] = {500,320,160,80};

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

/*increase wait time for all processes in ptable
 *whose state is either runnable || sleeping and
 *not residong in p-q 3
 *except the current running process
 *args : current running proc pointer
 *retrun type void
 */
void incr_wait_time(struct proc *cur_proc) {
  // for testing sch code :
  return;
  if(waitv == 1)
  cprintf("incr wait begin\n");

  struct proc *p,**oq,**nq;
  int old, new;
  int old_q, new_q;
  for(p = ptable.proc;p<&ptable.proc[NPROC];p++) {
    if( p != NULL){
         if(waitv == 1)
            cprintf("incr_wait_time():  proc name %s, state %d,  pid %d and timer ticks in q3:  %d \n",                                                       p->name, p->state, p->pid, p->cur_r_ticks);

    }
    // TA COMMENTS : if(p->state != RUNNABLE && p->state!= SLEEPING && p->res_q == 3) 
    if(p == cur_proc){
        if(waitv == 1)
            cprintf("Didnt increment for current running proc name %s \n", cur_proc->name);
        continue;
    }
     
    if((p->state == RUNNABLE || p->state == SLEEPING) && p->res_q != 3) { 
    
        p->cur_w_ticks++;
        if(p->cur_w_ticks>=MAX_WAIT_TS[p->res_q]) {
              old_q = p->res_q;
              p->t_w_ticks[old_q] += p->cur_w_ticks;
              p->cur_w_ticks = 0;
              p->t_r_ticks[old_q] += p->cur_r_ticks;
              p->cur_r_ticks = 0;
              new_q = ++p->res_q;
              //Now placement in new queue and put the old que to NULL
              old = 0; new = 0;
              oq = &ptable.q[old_q][0];
              nq = &ptable.q[new_q][0];
              while((old==0||new==0)&&
                    oq < &ptable.q[old_q][NPROC]&&
                    nq < &ptable.q[new_q][NPROC]) {
                	 //Found blank one place it 
                         if(((*nq)==NULL) ||
                                ((*nq)->state == UNUSED) || 
                                ((*nq)->state == ZOMBIE)){
                         		(*nq) = p;
                                         new = 1;
                    	 }
                        //Found old reference in old que, make it NULL
                        if((*oq)==p) {
                        	(*oq) = NULL;
                                 old = 1;
                        }
                    // the++
                    nq++;
                    oq++;
              }
        }  
    } 
  }

  if(waitv == 1)
  cprintf("incr wait end\n");

} 

// Set up first user process.
void
userinit(void)
{
  struct proc *p,**ptr;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  int i,flag=0;  

  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  //Initializing the two dimensional array
  // TBD decide to clean up or save time 
  for(i=0;i<4;i++) {
  	for(ptr = &ptable.q[i][0]; ptr < &ptable.q[i][NPROC];ptr++) {
		*ptr=NULL;
	}
  }

  //The init process is put on the first place of que 3
  //Put it in the 3rd que where we find it NULL
  for(ptr = &ptable.q[3][0]; ptr < &ptable.q[3][NPROC];ptr++) {
    if((*ptr)==NULL||(*ptr)->state == UNUSED) {
        flag = 1;
	break;
    }
  }

  if(flag==0) {
    panic("userinit: could not place it in queue");
  }
  *ptr = p;
  //Initializing the current que ticks
  p->cur_r_ticks = 0;
  p->cur_w_ticks = 0;
  p->res_q = 3;
  //Initializing the total que ticks
  i = 0;
  while(i<4){
     p->t_w_ticks[i] = 0;
     p->t_r_ticks[i] = 0;
     i++;
  }
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid,flag=0;
  struct proc *np,**p;
  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  

  //Put it in the 3rd que where we find it NULL 
  for(p = &ptable.q[3][0]; p < &ptable.q[3][NPROC];p++) {
    if(((*p) == NULL) || ((*p)->state == UNUSED) || ((*p)->state == ZOMBIE) ) {
       flag = 1;
       *p = np;
       break;
    }
  }
  
  if(flag==0){
    return -1;
    //TBD: kfree as in copyuvm() failure before 
  } 

  //Now initialize all the information of that ptable
  (*p)->res_q = 3;
  (*p)->cur_r_ticks = 0;
  (*p)->cur_w_ticks = 0;
  //Initializing the total que ticks
  i = 0;
  while(i<4){
     (*p)->t_w_ticks[i] = 0;
     (*p)->t_r_ticks[i] = 0;
     i++;
  }
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  //NEW CODE oct 11
  //update total run ticks
/*
  proc->t_r_ticks[proc->res_q]++;
  proc->t_w_ticks[proc->res_q] += proc->cur_w_ticks; 
  */

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

 
  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}


// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

/* mlfq
 *New implementtation for scheduler
 *with 4 priority queues
 *schedule process in priority queue from 3 to 0
 *
 *loop through individual queue and schedule first 
 *runnable process 
 */
void
scheduler(void)
{
  struct proc **ptr;
  struct proc *p;
  struct proc **ptr2;
  int counter = 0;
  // Infinite for loop for scheduler
  for(;;){
      //NEW CODE
      ptr = NULL;

      // Enable interrupts on this processor.
      sti();

      // Loop over process table looking for process to run.
      acquire(&ptable.lock);

      // Loop through 3-priority queue and check for runnable proc
      counter = 0;
      for (ptr = &ptable.q[3][0] ; ptr < &ptable.q[3][NPROC]; ptr++){
          if( ((*ptr) == NULL ) || 
              ((*ptr)->state != RUNNABLE)) {
              counter++;
              continue;
          }
          p = *ptr;

          // Increase the timer ticks for the 
          // last scheduled process
          (*ptr)->t_r_ticks[3]++;
          (*ptr)->cur_r_ticks++;


          // Found runnable process.  
          // Switch to chosen process.  It is the process's job
          // to release ptable.lock and then reacquire it
          // before jumping back to us.
          proc = p;
      
          switchuvm(proc);
          p->state = RUNNING;
          swtch(&cpu->scheduler, proc->context);
          switchkvm();  
       
          // Demotion logic for gaming pprevention
          if((*ptr)->cur_r_ticks >= MAX_TS[3]){
              for (ptr2 = &ptable.q[2][0]; ptr2 < &ptable.q[2][NPROC]; ptr2++){
                  //find the null entry for p-queue 2 and
                  //replace the nul entry for demotion
                  if(((*ptr2) == NULL) || ((*ptr2)->state == UNUSED) || ((*ptr2)->state == ZOMBIE)){
                      (*ptr2) = (*ptr);
                      //increase totol ticks and re initialise cur level run ticks to zero
                      (*ptr)->cur_r_ticks = 0;

                      (*ptr) = NULL;
                      //update res_q
                      (*ptr2)->res_q = 2;
                      // update total wait ticks for higher p-queue
                      (*ptr2)->t_w_ticks[3] += (*ptr2)->cur_w_ticks;
                      // re initialise current wait ticks
                      (*ptr2)->cur_w_ticks = 0;
                      break;
                  }
              }
          }
          /* Break after scheduling 
           * and increasing run ticks for the proc
           * and demoting the proc to lower queue if
           * time slice in the queue are fully consumed
           */
           // Starvation/Promotion function 
           incr_wait_time(p);

            //re initialise proc
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc = 0;
            break; 
        } /*end of for loop (q[3][])*/

    //if p-queue 3  has no runnable process then search in next queue
    if(counter >= 64 ){
      counter = 0;
      for (ptr = &ptable.q[2][0] ; ptr < &ptable.q[2][NPROC]; ptr++){

        if(((*ptr) == NULL) ||
           ((*ptr)->state != RUNNABLE)){
          counter++;
          continue;
        }
     
        // Found runnable process.
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p = *ptr;

        // Increase the timer ticks for the
        // last scheduled process
        (*ptr)->t_r_ticks[2]++;
        (*ptr)->cur_r_ticks++;

        proc = *ptr;
        switchuvm(*ptr);
        (*ptr)->state = RUNNING;
        swtch(&cpu->scheduler, proc->context);
        switchkvm();
	

        // Demotion logic for starvation pprevention
        if((*ptr)->cur_r_ticks >= MAX_TS[2]){
          for (ptr2 = &ptable.q[1][0]; ptr2 < &ptable.q[1][NPROC]; ptr2++){
            //find the null entry for p-queue 2 and
            //replace the nul entry for demotion
            if(((*ptr2) == NULL) || ((*ptr2)->state == UNUSED) || ((*ptr2)->state == ZOMBIE)){
              (*ptr2) = (*ptr);

              //increase totol ticks and re initialise cur level run ticks to zero
              (*ptr)->cur_r_ticks = 0;

              (*ptr)  = NULL;
              //update res_q
              (*ptr2)->res_q = 1;
              // update total wait ticks
              (*ptr2)->t_w_ticks[2] += (*ptr2)->cur_w_ticks;
              // re initialise current wait ticks
              (*ptr2)->cur_w_ticks = 0;
              break;
            }
          }
        }
        /* Break after scheduling
         * and increasing run ticks for the proc
         * and demoting the proc to lower queue if
         * time slice in the queue are fully consumed
         */

        // Starvation/Promotion function
        incr_wait_time(p);

        //re initialise proc
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        proc = 0;

        break;
      } /* end of for loop (q[2][])*/
    }

    //if p-queue 2  has no runnable process then search in next queue
    if(counter >= 64 ){
      counter = 0;
      for (ptr = &ptable.q[1][0] ; ptr < &ptable.q[1][NPROC]; ptr++){
        if(((*ptr) == NULL )||
           ((*ptr)->state != RUNNABLE)){
             counter++;
             continue;
        }

        // Found runnable process.
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p = *ptr;

        // Increase the timer ticks for the
        // last scheduled process
        (*ptr)->t_r_ticks[1]++;
        (*ptr)->cur_r_ticks++;


        proc = *ptr;
        switchuvm(*ptr);
        (*ptr)->state = RUNNING;
        swtch(&cpu->scheduler, proc->context);
        switchkvm();


        // Demotion logic for starvation pprevention
        if((*ptr)->cur_r_ticks >= MAX_TS[1]){
          for (ptr2 = &ptable.q[0][0]; ptr2 < &ptable.q[0][NPROC]; ptr2++){
            //find the null entry for p-queue 2 and
            //replace the nul entry for demotion
            if(((*ptr2) == NULL) || ((*ptr2)->state == UNUSED) || ((*ptr2)->state == ZOMBIE)){

              (*ptr2) = (*ptr);
               //increase totol ticks and re initialise cur level run ticks to zero
              (*ptr)->cur_r_ticks = 0;

              (*ptr)  = NULL;
              //update res_q
              (*ptr2)->res_q = 0;
              // update total wait ticks
              (*ptr2)->t_w_ticks[1] += (*ptr2)->cur_w_ticks;
              // re initialise current wait ticks
              (*ptr2)->cur_w_ticks = 0;
              break;
            }
          }
        }
        /* Break after scheduling
         * and increasing run ticks for the proc
         * and demoting the proc to lower queue if
         * time slice in the queue are fully consumed
         */

        // Starvation/Promotion function
        incr_wait_time(p);

            //re initialise proc
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc = 0;


        break;
      } /* end of for loop (q[1][])*/
    }

    /* -- FIFO PRIORITY QUEUE -- */
    //if p-queue 2  has no runnable process then search in next queue
    if(counter >= 64 ){
      counter = 0;
      for (ptr = &ptable.q[0][0] ; ptr < &ptable.q[0][NPROC]; ptr++){
        if(((*ptr) == NULL)||
          ((*ptr)->state != RUNNABLE)){
              counter++;
              continue;
        }
        // Found runnable process.
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p = *ptr;

    (*ptr)->t_r_ticks[0]++;


        proc = *ptr;
        switchuvm(*ptr);
        (*ptr)->state = RUNNING;
        swtch(&cpu->scheduler, proc->context);
        switchkvm();

        // Increase run ticks for last scheduled process
     //   (*ptr)->cur_r_ticks++;
//        (*ptr)->t_r_ticks[0]++;
        //re initialise current run ticks to zero
     //   (*ptr)->cur_r_ticks = 0; 

        /* FIFO logic:
         * If the last schdeuled process in the queue is still in
         * RUNNABLE state then re-schedule the same process
         * As in FIFO p-queue 0 the process must run to completion
         */

          //Starvation/Promotion function
          incr_wait_time(p);

        proc = 0;
        break;
      } /* end of for loop (q[0][]) */
    }
  release(&ptable.lock);
  } /*end of for loop (;;)*/
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  //cprintf("Samhith here\n");
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int getpinfo(struct pstat *ptr)
{
    struct proc *p;
    int ii = 0,jj=0;

    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++,ii++){
        //inuse
        if (p->state == UNUSED ){
                ptr->inuse[ii]=0;
        }else{
                ptr->inuse[ii]=1;
        }
       
        //pid
        ptr->pid[ii]=p->pid;
        //priority
        ptr->priority[ii]=p->res_q;
        //procstate state
        ptr->state[ii]=p->state;
        //ticks
        for(jj=0; jj<4; jj++){
                //total accumulated ticks at each queue **  **check failure cases on overflow
                ptr->ticks[ii][jj] = p->t_r_ticks[jj];
                //total wait ticks
                ptr->wait_ticks[ii][jj] = p->t_w_ticks[jj];

        }
    }
    release(&ptable.lock);

    return 0;
}


