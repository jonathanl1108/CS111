#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <getopt.h>    /* for getopt long */
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>


/**
 *>> --input=filename ... use the specified file as standard input (making it the new fd0).
 *>> --output=filename ... create the specified file and use it as standard output (making it the new fd1).
 *>> --segfault ... force a segmentation fault 
 *>> --catch ... use signal(2) to register a SIGSEGV handler that catches the segmentation fault, 
 *      logs an error message (on stderr, file descriptor 2) and exit(2) with a return code of 4.

*/

static struct option args[] = {
            {"input"    , required_argument, NULL,  'i'},
            {"output"   , required_argument, NULL,  'o'},
            {"segfault" , no_argument      , NULL,  's'},
            {"catch"    , no_argument      , NULL,  'c'},
            {0,            0,                 0,     0 }
        };
void IO_Errhandler( int isInput, char* opt )
{
  char* io=NULL;
  if( isInput )
    io="opening input file";
  else
    io="creating output file";
  if( strlen(opt))
    fprintf(stderr, "Error occured when %s: %s, ",io, opt);
   fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
}
void dupErr()
{
  fprintf(stderr, "unexpected dup() ERROR, Exit Code: %d, %s\n",errno,strerror(errno));
  exit(-1);
}
void closeFd_Err( int fd)
{
  fprintf(stderr,"Error closing file dicriptor %d \n",fd);
  exit(-1); 
}
void sigsegv_handler( int sig )
{
    fprintf(stderr,"Catch Segmentation Fault SEGSEV number: %d \n",sig);
    exit(4);
}

#define BUFFERSIZE 1024
char buffer[BUFFERSIZE];
int main(int argc , char *argv[] )
{
    // int verbose = 0; 
    int fd0= 0;
    int fd1 =1 ;
    int doSeg = 0;
    char op;
    while( (op = getopt_long(argc, argv, "i:o:sc", args, NULL) ) != -1) 
    { 
        switch(op) 
        { 
            case 'i':
            {
                /** use the specified file as standard input (making it the new fd0).*/
                // first open the file 
                fd0 = open(optarg,  O_RDONLY);
                /* If you are unable to open the specified input file, report the failure 
                (on stderr, file descriptor 2) using fprintf(3), and exit(2) with a return code of 2.*/
                if( fd0 < 0)
                {
                    IO_Errhandler( 1, optarg);
                    exit(2);
                }

                // we want to redrtiect to the real 0 
                if( close(0) == 0)
                {
                    // dup fd0 to 0
                    if( dup(fd0) == -1 )
                        dupErr();
                    // close
                    if( close(fd0) < 0)
                        closeFd_Err(fd0);

                    
                }else
                {
                    closeFd_Err(0);
                }
                break;
            }
            case 'o':
                fd1 = creat(optarg, 0666);

                if(fd1 < 0)
                {
                    IO_Errhandler( 0, optarg);
                    exit(3);
                } 

                if( dup2(fd1,1) == -1)
                    dupErr();
                break;
            case 's':
                doSeg = 1;
                break;
            case 'c':
                signal(SIGSEGV, sigsegv_handler);
                break;
            default:
                fprintf(stderr,"invalid option --%c \n", op);
                fprintf(stderr,"usage: lab0 [options]\n--input=file name\n --output=file name\n --segfault\n --catch\n");
                exit(-1); 
        } // end switch 
    } //end while

     if(doSeg)
     {
        // cause a seg fault
            char* ptr=NULL;
            (*ptr) ='f';
    } 

    ssize_t ret;
    while((ret=read(0,buffer,BUFFERSIZE)) > 0)
    {
        write(1,buffer,ret);
    }
    // successful and exit
    exit(0);
}

    