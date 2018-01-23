#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define SHM_NAME "trial_shm"

//The structure to be read
typedef struct {
    	int pid;
    	char birth[25];
    	char clientString[10];
    	int elapsed_sec;
    	double elapsed_msec;
} stats_t;



// Mutex variables
pthread_mutex_t mutex;
pthread_mutexattr_t mutexAttribute;

//Page size variable
long page_size;
int maxClients;
void * ptr = NULL;
int fd_shm = -1;

void exit_handler(int sig)
{
    // ADD
	munmap(ptr, page_size);
	close(fd_shm);
	shm_unlink(SHM_NAME);

        exit(0);
}

int main(int argc, char *argv[])
{
        // ADD
	long counter = 0;
	void *rptr = NULL, *wptr = NULL;
	stats_t init_stat;

	//sigaction
        struct sigaction act;
        act.sa_handler = exit_handler;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
	sigaction(SIGKILL, &act, NULL);
	sigaction(SIGTSTP, &act, NULL);


    	// Creating a new shared memory segment
    	fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0660);   
 	if(fd_shm<0) {
		//Exit due to shared memory file failure
		//May have to inform clients not to come up
		exit(1);
	}
        
	//Getting the page size
	page_size = sysconf(_SC_PAGE_SIZE);	
	if(page_size<1) {
		//Exit due to failure of sysconf system call failure
		//Have to inform clients not to come up
		exit(1);
	}
	maxClients = page_size / 64;
	maxClients--;
	
	//Truncating the file to the page size
	int rc = ftruncate(fd_shm,page_size);
	if(rc<0) {
		//Exit due to failure of ftruncate method
		//Have to inform clients not to come up
		exit(1);
	}
		
	//Mapping the file
	ptr = mmap(NULL, page_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
	if(ptr == (void*) -1) {
		//Exit due to failure of mmap method 
		//May have to inform clients not to come up
		exit(1);
	}
	
	
		
        // Initializing mutex
    	pthread_mutexattr_init(&mutexAttribute);
    	pthread_mutexattr_setpshared(&mutexAttribute, PTHREAD_PROCESS_SHARED);
    	pthread_mutex_init(&mutex, &mutexAttribute);
	
	wptr = ptr;
	//Initializing the shared memory 
	int i;
	for(i=0;i<=maxClients;i++) {	
		// Putting the init stat to figure out empty
		init_stat.pid = -1;
		memcpy(wptr, &init_stat, sizeof(stats_t));
		wptr = wptr+sizeof(stats_t);
	}
	

	//Wrie this mutex lock into first shared memory segment
	wptr = ptr;
	memcpy(wptr,&mutex,sizeof(pthread_mutex_t));

        while (1) {
        	// ADD
		rptr = ptr + sizeof(stats_t);
		for(i=0;i<maxClients;i++) {
			if(((stats_t*)rptr)->pid != -1) {
				fprintf(stdout,"%ld, pid : %d, birth : %s, elapsed : %d s %0.4lf ms, %s \n", counter, ((stats_t*)rptr)->pid, ((stats_t*)rptr)->birth, ((stats_t*)rptr)->elapsed_sec, ((stats_t*)rptr)->elapsed_msec, ((stats_t*)rptr)->clientString );
			}
			rptr = rptr + sizeof(stats_t);
		}
		counter++;
            	sleep(1);
        }
    	return 0;
}
