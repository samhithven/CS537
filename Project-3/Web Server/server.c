#include "cs537.h"
#include "request.h"
#include <pthread.h>
#include  <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

//chgs: Init mutex, lock, cond_v, req_buffer, thread_arr, thread_str
// declare lock
pthread_mutex_t lock;

//declare thread cond var
pthread_cond_t service_connfd_cv = PTHREAD_COND_INITIALIZER;

//declare main thread cond var
pthread_cond_t  insert_connfd_cv = PTHREAD_COND_INITIALIZER;

//init buffer
int buffer[INT_MAX];

/*function to check if buffer full
 * arg : buffer length as limit
 * return val 
 * -1    - buffer full
 * index - where buffer val is -1 
 * i.e. first empty space
 */
int 
check_buffer_full(int limit){
	int index = -1;
	for (int ii = 0; ii < limit; ii++){
		if (buffer[ii] == -1){
			index = ii;
			break;
		}	
	}
        //printf ("buff full function limit %d and index %d\n", limit, index);
	return index;
}

/* function to check if buffer empty
 * arg: buffer length as limit
 * return val
 * -1 - buffer empty
 * index - where first non -1 buffer val 
 * i.e. first available connfd
 */
int
check_buffer_empty(int limit){
        int index = -1;
        for (int ii = 0; ii< limit; ii++){
                if (buffer[ii] != -1){
                        index = ii;
                        break;
                }
        }
        //printf ("empty func index ret %d\n", index);
        return index;
}

//thread arg struct
typedef
struct _thread_arg_t{
    int th_id;
    int buff_len;
}thread_arg_t;

//thread function
void
*thread_function(void* arg){
   int serv_fd, connfd_index; 
   //obtain thread args : thread id - th_id and buffer lenght - buff_len
   thread_arg_t *targ = (thread_arg_t *) arg;
   //tid = targ->th_id;
   //printf ("thread : %d buff_len %d \n", targ->th_id, targ->buff_len);

   // put everything in while 1
   /*
    read buffer
    if buffer empty == cond_wait 
    successfull service -- wake main thread cv cond_signal
   */
   while (1){
	
   //printf ("thread id:%d before lock\n", tid);
        // take lock
        pthread_mutex_lock(&lock);
        //check if buffer is empty
   //printf ("thread id:%d before cond_wait\n", tid);
        while ( check_buffer_empty(targ->buff_len) == -1 )  //cond_wait
                pthread_cond_wait(&insert_connfd_cv, &lock);

	connfd_index = check_buffer_empty(targ->buff_len);
	serv_fd = buffer[connfd_index];
 //printf ("thread id:%d buffer not empty confd_index %d and connfd to be serviced %d\n", tid, connfd_index, serv_fd );
	buffer[connfd_index] = -1;
	//release the lock before servicing request
        pthread_mutex_unlock(&lock);
	requestHandle(serv_fd);          
	Close(serv_fd);

   //printf ("\nthread id:%d serviced connfd %d \n",tid, serv_fd);
	//acquire the lock before signalling
        pthread_mutex_lock(&lock);
        // cond signal to wake main thread
        pthread_cond_signal(&service_connfd_cv);
        // unlock after signalling worker thread
        pthread_mutex_unlock(&lock);
   }
}

int
main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
    int max_threads, buff_len, rc, buff_index;
    struct sockaddr_in clientaddr;

    if (argc != 4){
        fprintf(stderr, "Usage: %s <port> <threads> <buffers>\n", argv[0]);
	exit(1);
    }

    //init port thread count and buffer length
    //TBD check if argv 1 2 3 arent garbage
    port = atoi(argv[1]);
    if (port < 0)
	exit(0);
    max_threads = atoi(argv[2]);
    if (max_threads < 0)
	exit(0);
    buff_len = atoi(argv[3]);
    if(buff_len < 0)
	exit(0);
    //memset to -1 len == buff_len
    memset(buffer, -1, buff_len * sizeof(int));
    for (int j = 0 ; j < buff_len; j++){

	//printf("buffer[%d] = %d \n",j,buffer[j]);
    }
    // init thread array
    pthread_t  thread_arr[max_threads] ;
    // init thread arg array
    thread_arg_t thread_arg[max_threads];

    //printf("port %d thread count %d buff len %d\n", port, max_threads, buff_len);

    //init lock
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n pthread_mutex_init failed\n");
        exit(0);
    }    

    // 
    // CS537: Create some threads...
    for (int i=0 ; i<max_threads; i++){
        thread_arg[i].th_id = i;
        thread_arg[i].buff_len = buff_len;
        if ((rc = pthread_create(&thread_arr[i], NULL, thread_function, &thread_arg[i]))) {
            fprintf(stderr, "pthread_create failed rc: %d\n", rc);
            exit(0);
        }     
    }

    //

    listenfd = Open_listenfd(port);
    while (1) {
         
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        //printf("\nmain thread: new connfd %d\n", connfd);
	// 
	// CS537: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 

	/*
	if buffer full === cond wait 
	put connfd in buff == cond_signal
	*/

	// take lock
   //printf ("main thread : before lock\n");
	pthread_mutex_lock(&lock);
	//check if buffer is full
   //printf ("main thread : before cond_wait\n");
	while ( check_buffer_full(buff_len) == -1 )  //cond_wait  
		pthread_cond_wait(&service_connfd_cv, &lock);

	buff_index = check_buffer_full(buff_len);
   //printf ("main thread : found empty buffer space buff_index %d\n", buff_index);
	buffer[buff_index] = connfd;

	// cond signal to wake worker thread
	pthread_cond_signal(&insert_connfd_cv);	
	// unlock after signalling worker thread
   //printf ("main thread : added connfd to buffer\n");
	pthread_mutex_unlock(&lock);
	// 	requestHandle(connfd);  	Close(connfd);
    }

}
// RTFB !!!!
