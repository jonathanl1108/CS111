#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "SortedList.h"
#include <signal.h>

SortedList_t head;
SortedListElement_t* elements;

const int KEYSIZE =5; 

//flags
int opt_thread = 0;
int opt_iter = 0;
int opt_sysnc = 0;
int opt_mLock = 0;
long opt_sLock = 0;
char* yeld_arg=NULL;
// first define reconginzed through the external key word 
int opt_yield;

unsigned long num_threads=1;
long num_iter=1;
int is_slock_held=0;
//motex lock
pthread_mutex_t mutextlock=PTHREAD_MUTEX_INITIALIZER;

void handle_segfault() {
	fprintf(stderr, "Segmentation Fault Detected");
	exit(2);
}
void error(char *msg)
{
    perror(msg);
    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
    exit(1);
}
static struct option long_options[] =
{
    { "threads",    required_argument, NULL, 't' },
    { "iterations", required_argument, NULL, 'i' },
    { "yield",      required_argument, NULL, 'y' },
    { "sync",       required_argument, NULL, 's' },
    {     0,             0,           0,      0  }
};



/*
refrencing for random string generator in C
https://codereview.stackexchange.com/questions/29198/random-string-generator-in-c
*/
void keyGenerator( char * key, const int len)
{
    // word bank
    char* bank  ="ABCDEFGHIJKLM01234";
    char* bank2 ="NOPQRSTUVWXYZ56789";
    char* bank3="987654321abcdefghijklmnopqrstuvwxyz";
    char* applySet="";
    int set = rand()%3+1;
    switch(set)
    {
        case 1:
            applySet = bank;
            break;
        case 2:
            applySet = bank2;
            break;
        case 3:
            applySet = bank3;
            break;
        default:
            applySet = bank;
            break;
    } // end switch 
    
    int size =(int) strlen(applySet);
    int i =0 ;
    for (; i < len; ++i) {
        key[i] = applySet[rand() % size];
    }
    key[len] ='\0';
}
void assignKey(int num)
{
    elements = (SortedListElement_t*) malloc(num * sizeof(SortedListElement_t));
    for (int i =0 ; i < num; ++i)
    {
        // size + 1 (\0) ending char 
        char* key = (char*) malloc((KEYSIZE+ 1) * sizeof(char));
        keyGenerator(key, KEYSIZE);
        // printf("insert %s\n", key);
        elements[i].key = key;
    }
}

void setLock( char s_opt)
{
    switch( s_opt)
    {
        case 'm':
            opt_mLock=1;
            pthread_mutex_init(&mutextlock, NULL);
            break;
        case 's':
            opt_sLock=1;
            // a flag that indicate if the spink is_slock_held is held 
            is_slock_held=0;
            break;
        default:
            fprintf( stderr, "Unrecongized Option\n");
            fprintf( stderr, "Usage: noopt --sync=m --sync=s");
            exit(1);
    }
}
void processYeldArg( const char* myarg)
{
    /*
     (--yield=[idl], i for insert, d for delete, l for lookups
     */

    /**
        #define	INSERT_YIELD	0x01	// yield in insert critical section
        #define	DELETE_YIELD	0x02	// yield in delete critical section
        #define	LOOKUP_YIELD	0x04	// yield in lookup/length critical esction
        insert : 0001
        delet  : 0010
        lookup : 0100
    **/
    for ( int i =0 ;i < (int)strlen(myarg); ++i)
    {
        // apply bit wise inclusive OR to constuct a mask for sortlist.c to trigger yielding 
        if (myarg[i] == 'i')
            opt_yield =  opt_yield|INSERT_YIELD;
        else if (myarg[i] == 'd')
            opt_yield =  opt_yield|DELETE_YIELD;
        else if (myarg[i] == 'l')
            opt_yield =  opt_yield|LOOKUP_YIELD;
        else
        {
            fprintf( stderr, "Unrecongized Option for yield \n");
            fprintf( stderr, "Usage: yieldopts = {none, i,d,l,id,il,dl,idl}");
            exit(1);
        }
    }
    
}


void* thread_worker( void* keySet ) {
	
    SortedListElement_t* m_key_set_head = keySet;
    
    // lock protection if any apply 
    if( opt_mLock)
    {
         pthread_mutex_lock(&mutextlock);
    }else if( opt_sLock){
         while (__sync_lock_test_and_set(&is_slock_held, 1));
    }

    // since total element = iter * thread , it means each thread should do iter number of work 
	for (int i = 0; i < num_iter; i++) {
		SortedList_insert(&head, (SortedListElement_t *)&m_key_set_head[i] );
	}


	if (SortedList_length(&head) < num_iter) 
    {
		 fprintf(stderr, "synchronization error resulted in a corrupted list.\n");
         exit(2);
	}

	
	for (int i = 0; i < num_iter; i++) {

        SortedListElement_t *killme = SortedList_lookup(&head, m_key_set_head[i].key);
        if (killme == NULL || SortedList_delete(killme)) 
        {
            fprintf(stderr, "synchronization error resulted in a corrupted list.\n");
            exit(2);
        }

	}
    
    // UNLOCK
    if( opt_mLock) 
    { 
        pthread_mutex_unlock(&mutextlock);
	} else if( opt_sLock) {
       	__sync_lock_release(&is_slock_held);
    }
    return NULL;
}

void creatOp_Output()
{
    /*
     the name of the test, which is of the form: list-yieldopts-syncopts:
     yieldopts = {none, i,d,l,id,il,dl,idl}
     syncopts = {none,s,m}
     the number of threads (from --threads=)
     the number of iterations (from --iterations=)
     the number of lists (always 1 in this project)
     the total number of operations performed: threads x iterations x 3 (insert + lookup + delete)
     the total run time (in nanoseconds) for all threads
     the average time per operation (in nanoseconds).
     
     */

	printf("list");
    if( opt_yield)
    {
       printf("-%s", yeld_arg);
    }else
        printf("-none");
  
    if(opt_mLock)
        printf("-m");
    else if(opt_sLock)
        printf("-s");
    else
        printf("-none");

}

// clock_gettime API 
static inline unsigned long get_nanosec_from_timespec(struct timespec * spec)
{
    unsigned long ret= spec->tv_sec; // second 
    ret = ret * 1000000000 + spec->tv_nsec; // nanosecond 
    return ret;
}

int main(int argc, char* argv[]) {

	
	opt_yield = 0;
    char s_opt=' ';
	int opt;
	while (1) 
    {
        opt = getopt_long(argc, argv, "t:i:ys:", long_options, 0);
        if( opt == -1 )
            break;
		switch (opt) {
			case 't':
                opt_thread = 1 ;
				num_threads = atoi(optarg);
				break;
			case 'i':
                opt_iter = 1;
				num_iter = atoi(optarg);
				break;
			case 'y':
                yeld_arg=optarg;
                processYeldArg(yeld_arg);
				break;
			case 's':
                s_opt = optarg[0];
                setLock(s_opt);
				break;
			default:
				fprintf( stderr, " Unrecongized Option %s\n", argv[optind-1]);
                fprintf( stderr,
                        "Usage: lab2a  --threads= #num --iteractions= #num --yield=[idl] --sync=m --sync=s");
                exit(1);
		}
	}// end opt 

	const unsigned long elements_total = num_threads * num_iter;
	assignKey(elements_total);

     // set up get time
    long diff;
    struct timespec begin, end;
    pthread_t threads[num_threads];

  	if (clock_gettime(CLOCK_REALTIME, &begin) < 0)
        error("Error Occured when calling getime() for begin");

    signal(SIGSEGV, handle_segfault);

    unsigned long i;
  	for (i = 0; i < num_threads; i++) 
    {
  		if (pthread_create(&threads[i], NULL, &thread_worker, (void *)&elements[i*num_iter])) {
  			error("Error Occured when Creating Thread");
  			exit(1);
  		}
  	}

  	for (i = 0; i < num_threads; i++) {
  		if (pthread_join(threads[i], NULL)){
            error("Error Occured when Join Thread");
        }
  	}

    if (clock_gettime(CLOCK_REALTIME, &end) < 0)
        error("Error Occured when calling getime() for end");
    
    // check if the list is indeed empty, after all deletion 
  	if (SortedList_length(&head) != 0) {
  		fprintf(stderr, "List operation had corrupted the list \n");
  		exit(2);
  	}

    diff =  get_nanosec_from_timespec(&end) -get_nanosec_from_timespec(&begin);  //diff stores the execution time in ns
    

    
	  
	long operations = 3 * num_threads * num_iter; // insr / lkp/ delt 
	long time_per_operation = diff / operations;
    creatOp_Output();
	printf(",%ld,%ld,%d,%ld,%ld,%ld\n", num_threads, num_iter, 1, operations, diff, time_per_operation);
	free(elements);
	exit(0);
}
