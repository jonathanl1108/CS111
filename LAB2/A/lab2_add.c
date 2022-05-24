#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>



// 
/*
 number of iteration 
 number of threads to use 
 let threads to yield 
 sync = lock type  
*/
static struct option long_options[] =
{
    { "iterations", required_argument, NULL, 'i' },
    { "threads",    required_argument, NULL, 't' },
    { "yield",      no_argument,       NULL, 'y' },
    { "sync",       required_argument, NULL, 's' },
    {     0,             0,           0,      0  }
};

// sync_flgs 
int sync_mutex = 0;
int sync_spin = 0;
long isLockHeld = 0;
int sync_cas = 0 ;// compare and swap 
// var
int thread_num=1;
int iter_num=1;
/*  lock */
//motex
pthread_mutex_t mutexLock=PTHREAD_MUTEX_INITIALIZER;

long long counter = 0 ;

// flags for the operations
// opt flg
int opt_thread = 0;
int opt_iter = 0;
int opt_yield = 0;
int opt_sysnc = 0;


void error(char *msg)
{
    perror(msg);
    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
    exit(1);
}


void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (opt_yield)
        sched_yield();
    *pointer = sum;
}

void* thread_worker(void*iteration)
{
    int myitr = *((int*) iteration );

    for(int value = 1 ; value >= -1; value -=2)
    {
        int itr;
        for( itr =0 ; itr < myitr; itr++)
        {
            if( sync_mutex )
            {
                pthread_mutex_lock(&mutexLock);
                add(&counter, value);
                pthread_mutex_unlock(&mutexLock);
            }else if( sync_spin )
            {
                while (__sync_lock_test_and_set(&isLockHeld, 1));
                add(&counter, value);
                __sync_lock_release(&isLockHeld);
            }else if( sync_cas )
            {

                /*
                bool sync_bool_compare_and_swap(long long *p, long long old, long long new ) {
                if (*p == old) {  see if value has been changed 
                *p = new; // if not, set it to new value 
                return true;  tell caller he succeeded 
                }
                else  someone else changed *p
                return false;  tell caller he failed 
                }
                */
                long long old, sum;
                do {
                    old = counter;
                    sum = old + value;
                    if (opt_yield)
                        sched_yield();
                } while (__sync_val_compare_and_swap(&counter, old, sum) != old);
                
            }else
            {
                // no lock
                add(&counter, value);
            }
        }
        
    }
    return NULL;
    
   
}

// clock_gettime API 
static inline unsigned long get_nanosec_from_timespec(struct timespec * spec)
{
    unsigned long ret= spec->tv_sec; // second 
    ret = ret * 1000000000 + spec->tv_nsec; // nanosecond 
    return ret;
}

void setLock( char opt )
{
    switch( opt)
    {
        case 'm':
            sync_mutex = 1; 
            pthread_mutex_init(&mutexLock, NULL);
            break;
        case 's':
            sync_spin = 1;
            break;
        case 'c':
            sync_cas = 1;
            break;
        default:
            fprintf( stderr, " Unrecongized Option\n");
            fprintf( stderr, "Usage: --sync=m --sync=s --sync=c");
            exit(1);
    }

}
void creatOp_Output()
{

     /*
     add-none ... no yield, no synchronization
     add-m ... no yield, mutex synchronization
     add-s ... no yield, spin-lock synchronization
     add-c ... no yield, compare-and-swap synchronization
     add-yield-none ... yield, no synchronization
     add-yield-m ... yield, mutex synchronization
     add-yield-s ... yield, spin-lock synchronization
     add-yield-c ... yield, compare-and-swap synchronization
     */
    printf("add");
    if(opt_yield)
        printf("-yield");
    if(sync_mutex)
        printf("-m");
    else if(sync_spin)
        printf("-s");
    else if(sync_cas)
        printf("-c");
    else
        printf("-none");

}

int main(int argc, char* argv[])
{
    
    int opt =0 ;
    while(1)
    {
        opt = getopt_long(argc, argv, "t:i:ys:", long_options,0);
        
        if( opt == -1 )
            break;
        switch( opt )
        {
            case 'i':
            {
                opt_thread = 1;
                iter_num = atoi(optarg);
                break;
            }
            case 't':
            {
                opt_thread = 1;
                thread_num = atoi(optarg);
                break;
            }
            case 'y':
            {
                opt_yield = 1;
                break;
            }
            case 's':
            {
                opt_sysnc = 1;
                char s_opt=optarg[0];
                setLock(s_opt);
                break;
            }
            default:
                fprintf( stderr, " Unrecongized Option %s\n", argv[optind-1]);
                fprintf( stderr,
                        "Usage: lab2a  --threads= #num --iteractions= #num --yeld --sync=m --sync=s --sync=c");
                exit(1);
        }
        
    } // end opt while
     
    
    struct timespec begin, end;
    long long diff = 0;

    // init threads prep 
    pthread_t threads[thread_num];
    
    // start time 
    if (clock_gettime(CLOCK_REALTIME, &begin) < 0)
        error("Error Occured when calling clock_gettime ");
    
    int i;
    for (i = 0; i < thread_num; ++i)
    {
        if (pthread_create(&threads[i], NULL, thread_worker, (void*)&iter_num)){
            error("Error Occured when Creating Thread");
        }
        
    }
    for (i = 0; i < thread_num; ++i)
    {
        if (pthread_join(threads[i], NULL)){
            error("Error Occured when Join Thread");
        }
        
    }
    if (clock_gettime(CLOCK_REALTIME, &end) < 0)
        error("Error Occured when calling clock_gettime");
    
    diff =  get_nanosec_from_timespec(&end) -get_nanosec_from_timespec(&begin);  //diff stores the execution time in ns

    creatOp_Output();
    long long operation_count = 2 * (long long) thread_num * (long long)iter_num;
    long long time_per_operation = diff / operation_count;
    printf(",%d,%d,%lld,%lld,%lld,%lld\n",thread_num, iter_num, operation_count, diff, time_per_operation, counter);
    /*
    ./lab2_add --iterations=10000 --threads=10 add-none,10,10000,200000,6574000,32,374
     */
    exit(0);

}
