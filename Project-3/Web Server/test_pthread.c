#include<stdio.h>
#include<pthread.h>


typedef 
struct __myarg_t {
	int a;
	int b;
}myarg_t;


void 
*mythread (void *arg) {
	myarg_t *m = (myarg_t*) arg;
	printf("\n%d and %d \n", m->a, m->b);
	int  *x ;
	return NULL;
/*
        *x = 22;
	return (void*)x;
*/
}


int 
main(int argc, char *argv[]){
	pthread_t p ;
	int rc;
	int *ret;

	myarg_t args;
	args.a = 12;
	args.b = 14;

	rc = pthread_create(&p, NULL, mythread, &args);
	pthread_join(p, NULL);//(void **) &ret);
 
	return 0;
}
