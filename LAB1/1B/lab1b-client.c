#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef POLLRDHUP
#define POLLRDHUP 0x2000
#endif
/**
 * • Options
         --port=portnum (mandatory): port number used to set up connection
         --log=filename: maintains a record of data sent over the socket
         --compress (second part of project)
*/

// member field 
/*
    The termios functions describe a general terminal interface that
    is provided to control asynchronous communications ports.
*/
struct termios default_termio; 
// flgs 
int doZip = 0;
int portflg = 0;
int logflg = 0;
// fds 
char* f_log=NULL;
int   log_fd = -1;
int m_sockfd = -1;
pid_t child_pid = 0;
const char* HOST_NAME = "localhost";
char lf ='\n';
char* str_portno = NULL;
// zlib assit 
z_stream zstram_in;
z_stream zstram_out;

const ssize_t BUFFSIZE = 256;
// two pipes each from the shell and to shell
// 0 -> read end
// 1 -> write end
int pipe_to_shell[2];
int pipe_from_shell[2];

void fatal_error(char *error_msg) {
    perror(error_msg);
    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
    exit(1);
}
int client_connect( const char* hostname, unsigned int port )
{
    int sockfd;
    struct sockaddr_in serv_addr; /* server addr and port info */
    struct hostent* server;
    //1. create socket
    // Ipv4 and TCP , 0 by deafult 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // set up socket info 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons (port);
    server = gethostbyname(hostname); /* convert host_name to IP addr */
    if (server == NULL) {
        fprintf(stderr, "Server at %d is Not Found", port);
        fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
        exit(1);
    }
    memcpy((char*)&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);/* copy ip address from server to serv_addr */
    memset(serv_addr.sin_zero, '\0', sizeof serv_addr.sin_zero); /* padding zeros*/
    // connect 
    if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0 )
    {
        fatal_error("Error Occured When Connecting");
    } 

    return sockfd;
}

void checkFile( int fd, char* f_name)
{
    if (fd < 0)
    {
        fprintf(stderr, "Error occured while creating file: %s", f_name);
        fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
        exit(1);
    }
    
}

void Write( int fd_d, char* buf, long len)
{
    if( write(fd_d, buf, len) < 0)
    {
        fprintf(stderr, " Error Occured when Writing ");
        fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
        exit(1);
    }
}
void loging( int isS, int log_fd, int readin,char* input_buf)
{
    char* input="RECEIVED ";
    if( isS)
        input= "SENT ";
    char logbuf[BUFFSIZE];
    int byte = sprintf(logbuf, "%s%d bytes: ",input, readin);
    Write(log_fd, logbuf, byte);
    Write(log_fd, input_buf, sizeof(char));
    Write(log_fd, &lf, sizeof(char));
}

static struct option args[] = 
{
    // support only --shell
    {"port", required_argument,NULL , 'p'},
    {"log" , required_argument, NULL, 'l'},
    {"compress", no_argument  , NULL, 'c'},
    {      0,           0,       0,    0 }
};


void restore_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &default_termio);
}
    
void terminal_setup()
{
    // get the current termianl mode
    tcgetattr(STDIN_FILENO, &default_termio);
    // Clone the default terminal state
    struct termios new_termio =default_termio;
    new_termio.c_iflag = ISTRIP;    /* only lower 7 bits    */
    new_termio.c_oflag = 0;         /* no processing        */
    new_termio.c_lflag = 0;         /* no processing        */
    
    // set new terminal state
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termio) < 0)
        fatal_error("Failure using tcsetattr to setup  the new terminal mode");
    
    // Ensure the termainl gets restore when ever the prgram exits 
    atexit(restore_terminal_mode);
}


void read_write_shell()
{
    // we need to polling setup one fir termior and for the shell 
    struct pollfd pollfds[2];
    // poll keyboard/terminal
    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR|POLLRDHUP;
    // poll shell
    pollfds[1].fd =m_sockfd;
    pollfds[1].events = POLLIN | POLLHUP | POLLERR|POLLRDHUP;

    unsigned char read_buff[BUFFSIZE];
    unsigned char zip_buff[BUFFSIZE];
    // doubled size in case of <lf> and <cr>
    
    while(1)
    {
        poll(pollfds, 2, -1 );// -1 menas no time out

        //check events and gives the corsseponding actions needed 

        if( pollfds[0].revents & POLLIN) // input from keybord fwd it to the shell
        {
            // fprintf(stderr,"I here things from stdin~client ");
            ssize_t readin = read(STDIN_FILENO, read_buff, BUFFSIZE);

            if (readin < 0)
            fatal_error("Error reading from STDIN");
            char write_echo[BUFFSIZE];
            char write_socket[BUFFSIZE];

            for( int i =0 ; i < readin ; i++)
            {
                    ssize_t write_shell_size = 0;
                    ssize_t write_echo_size = 0;
                    char curr = read_buff[i];
                    // fprintf(stderr,"I here things from stdin~client CURR : %c \n",curr);
                    switch (curr) 
                    {
                        case '\r':
                        case '\n':
                            write_socket[write_shell_size++] = curr;
                            write_echo[write_echo_size++] ='\r';
                            write_echo[write_echo_size++] ='\n';
                            break;
                        case 0x03:
                            write_echo[write_echo_size++]='^';
                            write_echo[write_echo_size++]='C';
                            write_socket[write_shell_size++] = curr;
                            break;
                        case 0x04:
                            write_echo[write_echo_size++]='^';
                            write_echo[write_echo_size++]='D';
                            write_socket[write_shell_size++] = curr;
                            break;
                        default:
                            write_socket[write_shell_size++] = curr;
                            write_echo[write_echo_size++]   = curr;
                            break;
                    } //end switch 
                    // fwd the char to the shell if there is any 
                    if(!doZip)
                    {
                        // then write it into socket 
                    // fprintf(stderr,"I here things from stdin~client sending ");
                        if(write(m_sockfd, write_socket, write_shell_size*sizeof(char)) < 0)
                            fatal_error("Error writing to socket");
                        if(logflg)
                            loging(1,log_fd,write_shell_size, write_socket); 
                    }

                    // write to display like usual lab1a

                    if (write(STDOUT_FILENO, write_echo, write_echo_size*sizeof(char)) < 0)
                        fatal_error("Error writing to socket");
                    
            }

            /* run deflate() 
            refrence https://www.zlib.net/zlib_how.html
            on input until output buffer not full, finish
            compression if all of source has been read in 
            */

             
            if( doZip)  // handle zip 
            {
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
                    if (deflate(&zstram_in, Z_SYNC_FLUSH) != Z_OK)
                    {
                        fatal_error(" Deflating Faliure !");
                    }
                } while (zstram_in.avail_in > 0);

                // done 
                ssize_t compressed_bytes = BUFFSIZE - zstram_in.avail_out; //calculate size of compressed data


                if( write(m_sockfd,&zip_buff, compressed_bytes) < 0)
                {
                    fprintf(stderr, " Error Occured when Writing ");
                    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
                    exit(1);
                }
                //TODO should check 
                // when passing the whole buffer at onece to the logger a race-condtion has caused the log file to be messey 
                // if the fwding the zipbuff successfully then we willl recoreded it to our logger if apply
                if(logflg)
                {
                    for( int i =0 ; i < compressed_bytes  ;i++) 
                    {
                        char c = zip_buff[i];
                        loging(1,log_fd, 1, &c);
                    }
        
                }

                // wrap up the z-stream 
                deflateEnd(&zstram_in);

            }// end doZip 
            
            memset(read_buff, 0, readin);
            
        } // end hearing from stdin 

        if(pollfds[1].revents & POLLIN ) // recieved stdout from the shell
        {
            // now wee need to reverse 
            
            ssize_t readin = read( m_sockfd, read_buff, BUFFSIZE);
            if (readin < 0)
            fatal_error("Error reading from STDIN");
            if (readin== 0){
					exit(0);
			}

             char write_termio[BUFFSIZE];
             ssize_t write_termio_size = 0;

            // log whatever we received 
            if(logflg)
            {
                for( int i =0 ; i < readin  ;i++) 
                {
                    char curr = read_buff[i];
                    loging(0,log_fd,1, &curr);
                }
        
            }


                /* run deflate() 
                refrence https://www.zlib.net/zlib_how.html
                on input until output buffer not full, finish
                compression if all of source has been read in 
                */

            if(doZip)
            {
               
                zstram_out.zalloc = Z_NULL;
                zstram_out.zfree = Z_NULL;
                zstram_out.opaque = Z_NULL;

                if (inflateInit(&zstram_out) != Z_OK){
                   fatal_error(" In2fating Faliure !");
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
                        case '\n':
                            write_termio[write_termio_size++] = '\r';
                            write_termio[write_termio_size++] = '\n';
                            break;
                        default:
                            write_termio[write_termio_size++] = curr;
                            break;
                    } //end switch 

                }//end for 


            }else  // end do zip 
            {
                for( int i =0 ; i < readin ; i++)
                {
                    char curr = read_buff[i];
                    switch (curr) 
                    { 
                        case '\n':
                            write_termio[write_termio_size++] = '\r';
                            write_termio[write_termio_size++] = '\n';
                            break;
                        default:
                            write_termio[write_termio_size++] = curr;
                            break;
                    } //end switch 
                }
            }

                     // fwd the char to the termio if there is any 
            if (write(STDOUT_FILENO, write_termio, write_termio_size*sizeof(char)) < 0)
                        fatal_error("Error writing to termio");

            memset(read_buff, 0, readin);
            memset(zip_buff, 0, BUFFSIZE);
        } // end revieve from server 

        if ( (pollfds[1].revents)&(POLLERR | POLLHUP| POLLRDHUP )) 
        {
            exit(0);
        }
    }

}

int main(int argc, char **argv) {
    

    char op;
    while( (op = getopt_long(argc, argv, "", args, NULL) ) != -1) 
    { 
        switch( op )
        {
            case 'p':
            {
                portflg = 1;
                str_portno = optarg;
                break;
            }
            case 'l':
            {
                logflg = 1;
                f_log = optarg;
                // fprintf(stderr,"what is log %s \n", f_log);
                log_fd =  open(f_log, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                checkFile(log_fd, f_log);
                break;
            }
            case 'c':
            {
                doZip = 1;
                break;
            }
            default:
                fprintf(stderr,"invalid option --%c \n", op);
                fprintf( stderr,"Usage: lab1b-client --port= command,--log=filename, --compress\n");
                exit(1);
        }
    } //end while 

    // chekc if the port number is provided
     if( !portflg)
     {
        fprintf(stderr,"Port Number must be provided in order to build connections \n");
        exit(1);
     }
     
    // set the correct termio mode 
    terminal_setup();
    // convert port number from string to int
    int portno = atoi(str_portno);
    // fprintf(stderr,"port %d \n", portno);
    m_sockfd = client_connect(HOST_NAME, portno);
    read_write_shell();
    // char test[BUFFSIZE]={'H','e','l','l','p','\n'};
    // loging(1,log_fd,6,test);
    exit(0);
}
