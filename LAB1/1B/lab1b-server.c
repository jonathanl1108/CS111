#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>
#include <sys/wait.h>
#include <signal.h>
#include <zlib.h>

int portflg =0;
int doZip = 0;
char* str_portno=NULL;
int m_socket = -1;
z_stream zstram_in;
z_stream zstram_out;
int sockfd, new_fd;

void fatal_error(char *error_msg) {
    perror(error_msg);
    exit(1);
}

int server_connect(unsigned int port_num)
/* port_num: which port to listen, return socket for subsequent communication*/
{
     /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr; /* my address */
    struct sockaddr_in their_addr; /* connector addr */
    unsigned int sin_size;
    /* create a socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // ...
    /* set the address info */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port_num); /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY;
    /* INADDR_ANY allows clients to connect to any one of the host’s IP address.*/
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero)); //padding zeros
    /* bind the socket to the IP address and port number */
    if( bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0 )
        fatal_error( "Error Occured When Binding");
   
    // ...
    listen(sockfd, 5); /* maximum 5 pending connections */
    sin_size = sizeof(struct sockaddr_in);
    /* wait for client’s connection, their_addr stores client’s address */
    new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
    if (new_fd < 0) {
         fatal_error(" Error Occured When Accepting the Client");
    }
   
    return new_fd; /* new_fd is returned not sock_fd*/

}



/*
    The termios functions describe a general terminal interface that
    is provided to control asynchronous communications ports.
*/
struct termios default_termio; 
int shflag = 0;
pid_t child_pid = 0;
const char CRLF[2]={'\r','\n'};

const ssize_t BUFFSIZE = 256;
// two pipes each from the shell and to shell
// 0 -> read end
// 1 -> write end
int pipe_to_shell[2];
int pipe_from_shell[2];


static struct option args[] = 
{
    // support only --shell
    {"port", required_argument,NULL, 'p'},
    {"compresss", no_argument, &doZip, 1},
    {      0,           0,       0,    0}
};

void report_shell_exit()
{
    int status;
    {
        if (waitpid(child_pid, &status, 0) < 0)
            fatal_error("Error with system call waitpid");
        if (WIFEXITED(status))
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
    }
    
}

void Clean_pipe()
{
    dup2(pipe_from_shell[1], 1);
    close(pipe_from_shell[1]);
}

void cleanup()
{
    close(pipe_from_shell[0]);
    close(pipe_to_shell[1]);
    close(sockfd);
    close(m_socket);

}

void handle_sig(int sig) {
    if (sig == SIGPIPE) {
        exit(1);
    }
    if(sig == SIGINT)
        kill(child_pid, SIGINT);
}
void restore_terminal_mode() {
    // fprintf(stderr,"Im restored  \n");
    cleanup();
    report_shell_exit();
    exit(0);
}

    
void terminal_setup()
{
    // // get the current termianl mode
    // tcgetattr(STDIN_FILENO, &default_termio);
    // // Clone the default terminal state
    // struct termios new_termio =default_termio;
    // new_termio.c_iflag = ISTRIP;    /* only lower 7 bits    */
    // new_termio.c_oflag = 0;         /* no processing        */
    // new_termio.c_lflag = 0;         /* no processing        */
    
    // // set new terminal state
    // if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termio) < 0)
    //     fatal_error("Failure using tcsetattr to setup  the new terminal mode");
    
    // Ensure the termainl gets restore when ever the prgram exits 
    signal(SIGINT, handle_sig);
    signal(SIGPIPE, handle_sig);
    atexit(restore_terminal_mode);
}


//PART_A HELPERS 
/**
    1. Set the stdin into no echo mode, non-canonical input
    2. Read from stdin, write to stdout
    3. If read '\r' or '\n', write '\r' and '\n' to stdout.
    4. When receive EOF (ctrl + D), restore the terminal to normal mode and exit
*/

void create_pipe( int* m_pipe )
{
    if( pipe(m_pipe) == -1)
    fatal_error(" Error Occured When Constructing the pipe");
}


void read_write_shell();
void create_shell_process()
{

    // settup the 2 pipes for both termio and the shell 
    create_pipe(pipe_to_shell);
    create_pipe(pipe_from_shell);

    // create the process with fork 
    child_pid = fork();

    if( child_pid == 0) // child process
    {
        /*
          read stdin from termio to shell 
          fwrd stdout from shell to termio
        */
        // close the write end of to shell 
        close(pipe_to_shell[1]);
        close(0);
        dup(pipe_to_shell[0]);
        close(pipe_to_shell[0]);

        Clean_pipe();
        // run the shell 
        char *execvp_filename = "/bin/bash";
        char *execvp_argv[] = {execvp_filename, NULL};
        if (execvp(execvp_filename, execvp_argv) < 0)
            fatal_error("Failed replace child process with shell process image");

    }else // parent process
    {
        /*
         forwrad the stdout from shell to termio  
        */ 
       // termio writes to shell 
       read_write_shell();
    }

}


/**
 * read (ASCII) input from the keyboard, echo it to stdout, and forward it to the shell. <cr> or <lf> should echo as <cr><lf> but go to shell as <lf>.
 * 
 * read input from the shell pipe and write it to stdout. If it receives an <lf> from the shell, it should print it to the screen as <cr><lf>.
 * 
*/
void read_write_shell()
{
    // we need to polling setup one fir termior and for the shell 

    close( pipe_to_shell[0]);
    close( pipe_from_shell[1]);


    struct pollfd pollfds[2];
    // poll client fd
    pollfds[0].fd = m_socket;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;
    // poll shell
    pollfds[1].fd = pipe_from_shell[0];
    pollfds[1].events = POLLIN | POLLHUP | POLLERR;

    unsigned char read_buff[BUFFSIZE];
    unsigned char zip_buff[BUFFSIZE];
    // doubled size in case of <lf> and <cr>
    
    while(1)
    {
        if (poll(pollfds, 2, -1 ) < 0 )// -1 menas no time out
        {
            fatal_error(" Error Occured when polling");
        }

        //check events and gives the corsseponding actions needed 

        if( pollfds[0].revents & POLLIN) // input from client fwd it to the shell
        {

            ssize_t readin = read(m_socket, read_buff, BUFFSIZE);
            // int doEcho=0;
            if (readin < 0)
                fatal_error("Error reading from STDIN");
            char write_termio[BUFFSIZE];
            ssize_t write_termio_size = 0;

            /* run deflate() 
            refrence https://www.zlib.net/zlib_how.html
            on input until output buffer not full, finish
            compression if all of source has been read in 
            */

            if( doZip)
            {

                // we need to to inflate the alrady compress 
                zstram_out.zalloc = Z_NULL;
                zstram_out.zfree = Z_NULL;
                zstram_out.opaque = Z_NULL;

                if (inflateInit(&zstram_out) != Z_OK){
                   fatal_error(" InflateInit Faliure !");
                }

                zstram_out.avail_in = readin; 
                zstram_out.next_in = read_buff;
                zstram_out.avail_out = BUFFSIZE;
                zstram_out.next_out = zip_buff;

                //inflate
                do{
                    if (inflate(&zstram_out, Z_SYNC_FLUSH) != Z_OK){
                        fatal_error(" Infating Faliure !");
                    }
                } while (zstram_out.avail_in > 0);
                inflateEnd(&zstram_out);

                ssize_t compressed_bytes = BUFFSIZE - zstram_out.avail_out; //calculate size of compressed data

                 for( int i = 0 ; i < compressed_bytes ; i++)
                {
                    char curr = zip_buff[i];
                    switch (curr) 
                    { 
                        case '\r':
                        case '\n':
                            write_termio[write_termio_size++] = '\n';
                            break;
                        case 0x03: 
                        case 0x04:
                            // simply close down the server, service is no longer need it 
                            restore_terminal_mode();
                            break;
                        default:
                            write_termio[write_termio_size++] = curr;
                            break;
                    } //end switch 

                    // fwd the char to the termio if there is any 
                    if (write(pipe_to_shell[1], write_termio, write_termio_size*sizeof(char)) < 0)
                        fatal_error("Error writing to termio");

                }


            }else  // end do zip 
            {
            //    fprintf(stderr,"I here things from clinet2 withoutzip   \n");
                for( int i =0 ; i < readin ; i++)
                {
                    char write_shell[BUFFSIZE];
                    ssize_t write_shell_size = 0;
                    char curr = read_buff[i];
                    // fprintf(stderr," I here things from clinet2 withoutzip : %c \n",curr);
                    switch (curr) 
                    {
                        case '\r':
                        case '\n':
                            write_shell[write_shell_size++] = '\n';
                            break;
                        case 0x03:      // ^C
                            if (kill(child_pid, SIGINT) < 0)
                                fatal_error("Failed to kill child shell process");
                            break;
                        case 0x04:      // ^D
                            if (close(pipe_to_shell[1]) < 0)
                                fatal_error("Failed to close pipe to shell process on exit");
                            break;
                        default:
                            write_shell[write_shell_size++] = curr;
                    } //end switch 
                    // fwd the char to the shell if there is any 
                    if (write(pipe_to_shell[1], write_shell, write_shell_size*sizeof(char)) < 0)
                        fatal_error("Error writing to shell");
                
                    // we need to echo too !
                }

            }
            memset(read_buff, 0, readin);
            
        }
        if(pollfds[1].revents & POLLIN ) // recieved stdout from the shell
        {
            // now wee need to reverse 
            
            ssize_t readin = read( pipe_from_shell[0], read_buff, BUFFSIZE);
            if (readin < 0)
            fatal_error("Error reading from STDIN");


            /* run deflate() 
            refrence https://www.zlib.net/zlib_how.html
            on input until output buffer not full, finish
            compression if all of source has been read in 
            */

           if( !doZip)  // if no compress just fwd what8ever to the client 
           {
                // do normal fwrding as usual 
                for( int i =0 ; i < readin ; i++)
                {
                    char curr = read_buff[i];
            
                    // fwd the char to the termio if there is any 
                    if (write(m_socket, &curr, sizeof(char)) < 0)
                        fatal_error("Error writing to termio");
        
                }
                memset(read_buff, 0, readin);

           }else{ // if we really need to compress then we need to zip it now 
                // compress it 

                // first init pre-deflate  
                zstram_in.zalloc = Z_NULL;
                zstram_in.zfree = Z_NULL;
                zstram_in.opaque = Z_NULL;
                if (deflateInit(&zstram_in, Z_DEFAULT_COMPRESSION) != Z_OK){
                    fatal_error("deflate inition faliure");
                }

                // do zip
                zstram_in.avail_in = readin;
                zstram_in.next_in = read_buff;
                zstram_in.avail_out = BUFFSIZE;
                zstram_in.next_out = zip_buff;

                do{
                    if (deflate(&zstram_in, Z_SYNC_FLUSH) != Z_OK){
                        fatal_error(" Deafating Faliure !");
                    }
                } while (zstram_in.avail_in > 0);

                // done 
                ssize_t compressed_bytes = BUFFSIZE - zstram_in.avail_out; //calculate size of compressed data

                if( write(m_socket,zip_buff, compressed_bytes) < 0)
                {
                    fprintf(stderr, " Error Occured when Writing ");
                    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
                    exit(1);
                }
                // wrap zstream up 
                deflateEnd(&zstram_in);

            } 

        }

         if (pollfds[1].revents & POLLERR || pollfds[1].revents & POLLHUP) { 
            exit(0);
        }
    }

}

int main(int argc, char **argv) {
    
    char op;
    while( (op = getopt_long(argc, argv, "", args, NULL) ) != -1) 
    { 
        if (op == 0)        // getopt 
            continue;
        if (op == 'p')
        {
            portflg = 1;
            str_portno = optarg;
            // fprintf(stderr,"what is portno %s \n", str_portno);
        }
        if (op == '?') {    // invalid option
            fprintf(stderr,"invalid option --%c \n", op);
            fprintf(stderr, "Usage: lab1a [--port=portnumber][--compress][--shell]\n");
            exit(1);
        }
    }
    if( !portflg)
     {
        fprintf(stderr,"Port Number must be provided in order to build connections \n");
        exit(1);
     }
     
    // set the correct termio mode 
    terminal_setup();
    // convert port number from string to int
    int portno = atoi(str_portno);
    // make connection 
    m_socket = server_connect(portno);
    // atexit(report_shell_exit);
    create_shell_process();
    exit(0);
}


