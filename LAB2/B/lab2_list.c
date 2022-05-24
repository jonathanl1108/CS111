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

SortedList_t* head;
SortedListElement_t* elements;

const int KEYSIZE =5; 

//flags
int opt_thread = 0;
int opt_iter = 0;
int opt_sysnc = 0;
int opt_mLock = 0;
long opt_sLock = 0;
int sub_list=1; // by default lets just have one list 
char* yeld_arg=NULL;
// first define reconginzed through the external key word 
int opt_yield;
long wait_time = 0;

unsigned long num_threads=1;
long num_iter=1;
//spin lock
long* is_slock_held=0;
//motex lock
pthread_mutex_t *mutextlock;

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
    { "lists",       required_argument, NULL, 'l' },
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
            // pthread_mutex_init(&mutextlock, NULL);
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
// clock_gettime API 
static inline unsigned long get_nanosec_from_timespec(struct timespec * spec)
{
    unsigned long ret= spec->tv_sec; // second 
    ret = ret * 1000000000 + spec->tv_nsec; // nanosecond 
    return ret;
}
//key should be in based on a simple hash of the key, modulo the number of lists.
int hash(const char* key)
{
    int kseize=4;
    int i, sum;
    for (sum=0, i=0; i < kseize; i++)
        sum += key[i];
    return sum % sub_list;
}


void* thread_worker( void* keySet ) {

    /**
     * starts with a set of pre-allocated and initialized elements (--iterations=#)
        inserts them all into the multi-list (which sublist the key should go into determined by a hash of the key)
        gets the list length
        looks up and deletes each of the keys it inserted
        exits to re-join the parent thread
     * 
     * 
     * 
     * 
    */
    // printf("GOt here 1 \n");
    SortedListElement_t* m_key_set_head = keySet;
	struct timespec lkw_begin, lkw_end;
    int i;
    int index;
    long total_list_len = 0;
	for (i = 0; i < num_iter; i++) {
		index = hash(m_key_set_head[i].key);
        // start waitting
		clock_gettime(CLOCK_MONOTONIC, &lkw_begin);
		if (opt_mLock) {
	        	pthread_mutex_lock(&mutextlock[index]);
	    	} else if (opt_sLock) {
	    		while (__sync_lock_test_and_set(&is_slock_held[index], 1));
	    	}
        // end waitting 
	   	clock_gettime(CLOCK_MONOTONIC, &lkw_end);
		wait_time += get_nanosec_from_timespec(&lkw_end)-get_nanosec_from_timespec(&lkw_begin);  //diff stores the execution time in ns
        // printf("GOt here 1-2-test %s \n",head[index].key);
        SortedList_insert(&head[index], &m_key_set_head[i]);
        //    printf("GOt here 2 \n");
    	if (opt_mLock) {
        		pthread_mutex_unlock(&mutextlock[index]);
   		} else if (opt_sLock) {
        		__sync_lock_release(&is_slock_held[index]);
    	}
	}
   //gets the list length
   //figure out how to (safely and correctly) obtain the length of the list, which now involves enumerating all of the sub-lists.
    for (i = 0; i < sub_list; i++) {
     	if (opt_mLock) {
	        clock_gettime(CLOCK_MONOTONIC, &lkw_begin);
	        pthread_mutex_lock(&mutextlock[i]);
	        clock_gettime(CLOCK_MONOTONIC, &lkw_end);
      	} else if (opt_sLock) {
            clock_gettime(CLOCK_MONOTONIC, &lkw_begin);
         	while (__sync_lock_test_and_set(&is_slock_held[i], 1))
             {

             };
            clock_gettime(CLOCK_MONOTONIC, &lkw_end);
      	}
        wait_time += get_nanosec_from_timespec(&lkw_end)-get_nanosec_from_timespec(&lkw_begin);  //diff stores the execution time in ns
    }

   	for (i = 0; i < sub_list ; i++)
    		total_list_len += SortedList_length(&head[i]);
	
	if (total_list_len < num_iter) {
		fprintf(stderr, "synchronization error resulted in a corrupted list.\n");
        //safeExist();
        exit(2);
	}

	for (i = 0; i < sub_list ; i++)
    {
      if (opt_mLock) {
        	pthread_mutex_unlock(&mutextlock[i]);
   		} else if (opt_sLock) {
        	__sync_lock_release(&is_slock_held[i]);
    	}
    }

//  printf("GOt here 4 \n");
    //looks up and deletes each of the keys it inserted
	for (i = 0; i < num_iter; i++) 
    {
        index = hash(m_key_set_head[i].key);
        clock_gettime(CLOCK_MONOTONIC, &lkw_begin);
		if (opt_mLock) {
	        	pthread_mutex_lock(&mutextlock[index]);
	    	} else if (opt_sLock) {
	    		while (__sync_lock_test_and_set(&is_slock_held[index], 1));
	    	}
        // end waitting 
	   	clock_gettime(CLOCK_MONOTONIC, &lkw_end);
		wait_time += get_nanosec_from_timespec(&lkw_end)-get_nanosec_from_timespec(&lkw_begin);  //diff stores the execution time in ns

        SortedListElement_t *killme = SortedList_lookup(&head[index], m_key_set_head[i].key);

        if (killme == NULL || SortedList_delete(killme)) 
        {
            fprintf(stderr, "synchronization error resulted in a corrupted list during delet.\n");
            exit(2);
        }

		if (opt_mLock) {
        		pthread_mutex_unlock(&mutextlock[index]);
   		} else if (opt_sLock) {
        		__sync_lock_release(&is_slock_held[index]);
    	}
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

void lockConstructor()
{
    int i =0;
    if( opt_mLock)
    {
      mutextlock = malloc( sub_list*sizeof(pthread_mutex_t));
        for(; i < sub_list ;i++)
        {
            pthread_mutex_init(& mutextlock[i], NULL);
        }
        
    }else if( opt_sLock)
    {
        is_slock_held = malloc( sub_list * sizeof(long));
        for(; i < sub_list ;i++)
        {
            is_slock_held[i]=0;
        }
        
    }
}
void listConstruct()
{
    // we now need numlist of heads
    head = (SortedList_t *) malloc(sub_list*sizeof(SortedList_t));
    for( int i =0; i < sub_list ; i++)
    {
        // let prv and next points to itself 
        head[i].prev = NULL;
        head[i].next = NULL;
    }
   
    
}



int main(int argc, char* argv[]) {

	
	opt_yield = 0;
    char s_opt=' ';
	int opt;
	while (1) 
    {
        opt = getopt_long(argc, argv, "t:i:ys:l:", long_options, 0);
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
            case 'l':
                sub_list= atoi(optarg);
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
   
    // lock initilzation 
    lockConstructor();
    // list initilzation 
    listConstruct();


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
    
    int total_list_len =0 ;
    for (int j = 0; j < sub_list ; j++)
    		total_list_len += SortedList_length(&head[j]);
	
	if (total_list_len !=0) {
		fprintf(stderr, "final Delet result in Failure List are not empty\n");
        //safeExist();
        exit(2);
	}

    diff =  get_nanosec_from_timespec(&end) -get_nanosec_from_timespec(&begin);  //diff stores the execution time in ns
    
	  
	long operations = 3 * num_threads * num_iter; // insr / lkp/ delt 
	long time_per_operation = diff / operations;
    long long avg_lock_wait = wait_time/operations;
    creatOp_Output();
	printf(",%ld,%ld,%d,%ld,%ld,%ld,%lld\n", num_threads, num_iter, sub_list, operations, diff, time_per_operation,avg_lock_wait);
	free(elements);
    free(head);
	exit(0);
}
