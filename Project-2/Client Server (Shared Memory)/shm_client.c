#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

#define SHM_NAME "trial_shm"
#define SHM_FILE_NAME "/dev/shm/trial_shm"

typedef struct {
        int pid;
        char birth[25];
        char clientString[10];
        int elapsed_sec;
        double elapsed_msec;
} stats_t;

//client segment start pointer
void  *wptr;

// Mutex variables
pthread_mutex_t mutex;

void exit_handler(int sig) {
    // ADD
    // critical section begins
    pthread_mutex_lock(&mutex);
        // Client leaving; needs to reset its segment   
	((stats_t*)wptr)->pid = -1;
    pthread_mutex_unlock(&mutex);
    // critical section ends

    exit(0);
}

float time_diff(struct timeval cur_time, struct timeval dob){
	float time_diff;
	time_diff = (cur_time.tv_sec - dob.tv_sec) * 1000.0f + (cur_time.tv_usec - dob.tv_usec) / 1000.0f;
	return time_diff;
}

int get_sec(float elapsed_time){
	int sec;
	sec = (int)(elapsed_time/1000);
	return sec;
}

int main(int argc, char *argv[]) {
        if(strlen(argv[1])>10) 
          exit(0);

   	
	struct timeval dob, cur_time;
	time_t ctime_sec;
	char *dob_ctime;
	float elapsed, msec;
	int sec;

	//sigaction
	struct sigaction act;
	act.sa_handler = exit_handler;
    sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGKILL, &act, NULL);
	sigaction(SIGTSTP, &act, NULL);


	//get date of birth and store ctime args
	gettimeofday(&dob, 0);
	ctime_sec = dob.tv_sec;
	dob_ctime = ctime(&ctime_sec);

	void *ptr, *rptr;   
    	int fd_shm = shm_open(SHM_NAME, O_RDWR, 0660);
	if(fd_shm<0) {
                //Exit due to shared memory file failure
                //May have to inform clients not to come up
                exit(1);
        }

        //Getting the page size
        long page_size = sysconf(_SC_PAGE_SIZE);
        if(page_size<1) {
                //Exit due to failure of sysconf system call failure
                //Have to inform clients not to come up
                exit(0);
        }
        int maxClients = page_size / 64;
	maxClients--;

	//Mapping the file
        ptr = mmap(NULL, page_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
        if(ptr == (void*) -1) {
                //Exit due to failure of mmap method 
                //May have to inform clients not to come up
                exit(0);
        }

	rptr = ptr;
	mutex = *((pthread_mutex_t*)rptr);
		
        // critical section begins
        
    	// client updates available segment
	rptr = ptr + sizeof(stats_t);
	for(int i=0;i<maxClients;i++) {
        	pthread_mutex_lock(&mutex);
		if(((stats_t*)rptr)->pid == -1) {
			wptr = rptr;
			//store the pid 
			((stats_t*)rptr)->pid = getpid();
    			pthread_mutex_unlock(&mutex);
			break;
		}
    		pthread_mutex_unlock(&mutex);
		rptr = rptr + sizeof(stats_t);
	}

	if(wptr==NULL) {
		//Haven't found free space in shared memory
		exit(0);
	}
        // critical section ends
	//store the client name 
	snprintf(((stats_t*)wptr)->clientString, 10, "%s", argv[1]);
	//store the dob
	snprintf(((stats_t*)wptr)->birth, 25,"%s", dob_ctime);

	//calculate elapsed time
        gettimeofday(&cur_time, 0);
        elapsed = time_diff(cur_time, dob);

	sec = get_sec(elapsed);
	msec = elapsed - (sec * 1000);
        //store sec and msec
	((stats_t*)wptr)->elapsed_sec = sec;
 	((stats_t*)wptr)->elapsed_msec = (double)msec;        


    	while (1) {
        	//check the status of the shared mem file
		if (access(SHM_FILE_NAME, F_OK) == -1){
			//server got killed and sh mem file deleted
			exit(0);
		} 

        	// update client segement data
        	
		//calculate elapsed time
	        gettimeofday(&cur_time, 0);
       		elapsed = time_diff(cur_time, dob);

      		sec = get_sec(elapsed);
      		msec = elapsed - (sec * 1000);
     		//store sec and msec
      		((stats_t*)wptr)->elapsed_sec = sec;
      		((stats_t*)wptr)->elapsed_msec = (double)msec;

            	sleep(1);

        	// Print active clients
		fprintf(stdout,"Active clients :");
		rptr = ptr + sizeof(stats_t);
       		 for(int i=0;i<maxClients;i++) {
                	if(((stats_t*)rptr)->pid != -1) {
                     		fprintf(stdout," %d",((stats_t*)rptr)->pid);
                	}
              		rptr = rptr + sizeof(stats_t);
        	}
		fprintf(stdout,"\n");
        }
        return 0;
}



