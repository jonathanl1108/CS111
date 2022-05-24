#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

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

void fatal_error(char *error_msg) {
    perror(error_msg);
    exit(1);
}

static struct option args[] = 
{
    // support only --shell
    {"shell", no_argument, &shflag, 1},
    {      0,           0,       0, 0}
};

void report_shell_exit()
{
    int status;
    if( shflag)
    {
        if (waitpid(child_pid, &status, 0) < 0)
            fatal_error("Error with system call waitpid");
        if (WIFEXITED(status))
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
    }
    
}


void restore_terminal_mode() {
    // fprintf(stderr,"Im restored  \n");
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



void handle_sig(int sig) {
    if (sig == SIGPIPE) {
        exit(0);
    }
}

void fatal_usage() {
    fprintf(stderr, "Usage: lab1a [--shell]\n");
    exit(1);
}


//PART_A HELPERS 
/**
    1. Set the stdin into no echo mode, non-canonical input
    2. Read from stdin, write to stdout
    3. If read '\r' or '\n', write '\r' and '\n' to stdout.
    4. When receive EOF (ctrl + D), restore the terminal to normal mode and exit
*/

void No_Echo_Read_Write()
{
    char read_buff[BUFFSIZE];
    char write_buff[BUFFSIZE];
    
    while (1) {
        ssize_t readin = read(STDIN_FILENO, read_buff, BUFFSIZE);
        ssize_t write_buff_size = 0;
        if (readin < 0)
            fatal_error("Error reading from STDIN");
        int i;
        for (i = 0; i < readin; i++) {
            char curr = read_buff[i];
            switch (curr) {
                case '\r':
                    /* FALLTHRU */
                case '\n':
                    write_buff[write_buff_size++] = '\r';
                    write_buff[write_buff_size++] = '\n';
                    break;
                case 0x04:      // ^D
                    exit(0);
                default:
                    write_buff[write_buff_size++] = curr;
            }
        }
        if (write(STDOUT_FILENO, write_buff, write_buff_size*sizeof(char)) < 0)
            fatal_error("Error writing to STDOUT");
        memset(read_buff, 0, BUFFSIZE);
    }
}

void Clean_pipe()
{
	dup2(pipe_from_shell[1], 1);
    close(pipe_from_shell[1]);
}


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
       close( pipe_to_shell[0]);
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
    struct pollfd pollfds[2];
    // poll keyboard/terminal
    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;
    // poll shell
    pollfds[1].fd = pipe_from_shell[0];
    pollfds[1].events = POLLIN | POLLHUP | POLLERR;

    char read_buff[BUFFSIZE];
    // doubled size in case of <lf> and <cr>
    
    while(1)
    {
        poll(pollfds, 2, -1 );// -1 menas no time out

        //check events and gives the corsseponding actions needed 

        if( pollfds[0].revents & POLLIN) // input from keybord fwd it to the shell
        {
            ssize_t readin = read(STDIN_FILENO, read_buff, BUFFSIZE);
            // int doEcho=0;
            if (readin < 0)
            fatal_error("Error reading from STDIN");

            for( int i =0 ; i < readin ; i++)
            {
                char write_echo[BUFFSIZE];
                char write_shell[BUFFSIZE];
                ssize_t write_shell_size = 0;
                ssize_t write_echo_size = 0;
                char curr = read_buff[i];
                switch (curr) 
                {
                    case '\r':
                    case '\n':
                        write_shell[write_shell_size++] = '\n';
                        write_echo[write_echo_size++] ='\r';
                        write_echo[write_echo_size++] ='\n';
                        break;
                    case 0x03:      // ^C
                        write_echo[write_echo_size++]='^';
                        write_echo[write_echo_size++]='C';
                        if (write(STDOUT_FILENO, write_echo, 2*sizeof(char)) < 0)
                            fatal_error("Error Echoling");
                        if (kill(child_pid, SIGINT) < 0)
                            fatal_error("Failed to kill child shell process");
                        break;
                    case 0x04:      // ^D
                        if (close(pipe_to_shell[1]) < 0)
                            fatal_error("Failed to close pipe to shell process on exit");
                        write_echo[write_echo_size++]='^';
                        write_echo[write_echo_size++]='D';
                        break;
                    default:
                        write_shell[write_shell_size++] = curr;
                        write_echo[write_echo_size++]   = curr;
                } //end switch 

                if (write(STDOUT_FILENO, write_echo, write_echo_size*sizeof(char)) < 0)
                    fatal_error("Error Echoling");
                // fwd the char to the shell if there is any 
                if (write(pipe_to_shell[1], write_shell, write_shell_size*sizeof(char)) < 0)
                    fatal_error("Error writing to shell");
              
                // we need to echo too !
            }
            memset(read_buff, 0, readin);
            
        }
        if(pollfds[1].revents & POLLIN ) // recieved stdout from the shell
        {
            // now wee need to reverse 
            
            ssize_t readin = read( pipe_from_shell[0], read_buff, BUFFSIZE);
            if (readin < 0)
            fatal_error("Error reading from STDIN");

            for( int i =0 ; i < readin ; i++)
            {
                char write_termio[BUFFSIZE];
                ssize_t write_termio_size = 0;
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

                // fwd the char to the termio if there is any 
                if (write(STDOUT_FILENO, write_termio, write_termio_size*sizeof(char)) < 0)
                    fatal_error("Error writing to termio");
    
            }
            memset(read_buff, 0, readin);
        }

         if (pollfds[1].revents & POLLERR || pollfds[1].revents & POLLHUP) { 
            exit(0);
        }
    }

}

int main(int argc, char **argv) {
    
    // set the correct termio mode 
    terminal_setup();

    char op;
    while( (op = getopt_long(argc, argv, "", args, NULL) ) != -1) 
    { 
        if (op == 0)        // getopt 
            continue;
        if (op == '?') {    // invalid option
            fprintf(stderr,"invalid option --%c \n", op);
            fprintf(stderr, "Usage: lab1a [--shell]\n");
            exit(1);
        }
    }

    // fprintf(stderr,"Im a happy person  \n");
    // fprintf(stderr,"shellflg %d \n",shflag);

    if (shflag) {
        atexit(report_shell_exit);
        create_shell_process();
    }else 
    {
        No_Echo_Read_Write();
    }
    exit(0);
}

