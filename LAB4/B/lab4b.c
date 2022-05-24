//
//  main.c
//  lab4b
//
//  Created by Jonathan
//  Copyright © 2019-2020 Jonathan. All rights reserved.
//

#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mraa.h>
#include <sys/poll.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// gobal
/* flags */
int m_period =0;
int m_scale = 1; // DEFAULT 1 for F
int islogSet =0;
int fd_log;
char* log_name=NULL;
char strend='\n';
const int MAX_BYTE =32;
int doReport =1;

// Component
/*
 Attach your Grove Temperature Sensor to the
 Analog A0/A1 connector on your Grove cape,
 where it will be addressed as I/O pin #1.
 Attach your Grove Button to the GPIO_50
 connector on your Grove cape, where
 it will be addressed as I/O pin #60.
 */
mraa_aio_context m_sensor;
mraa_gpio_context m_button;

// Analog A0/A1 address 1
// GPIO_50 address 60
#define TEMP_SENSOR_IO_PIN 1
#define BUTTON_IO_PIN 60

#define B 4275
#define R0 100000.0
float convert_temper_reading(int reading)
{
    float R = 1023.0/((float) reading) - 1.0;
    R = R0*R;
    //C is the temperature in Celcious
    float C = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;
    //F is the temperature in Fahrenheit
    if( m_scale)// return F  if  it is 1
    {
        return (C * 9)/5 + 32;
    }else
        return C;
}

void error(char *msg)
{
    perror(msg);
    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
    exit(1);
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
//////////////////////////////////
// Button Helper
//////////////////////////////////

// print a time stemp then exit with shutdown message
void do_when_button_pressed()
{
    struct timespec ts;
    struct tm * tm;
    // get the exact time frame now
    clock_gettime(CLOCK_REALTIME, &ts);
    // convert the time to human readable
    tm = localtime(&(ts.tv_sec));
    char report[64];
    sprintf(report, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour,
            tm->tm_min, tm->tm_sec);
    size_t m_size = strlen(report);
    // output to the councel
    Write(STDOUT_FILENO, report, m_size);
    // if we do have a log file then log it
    if (islogSet)
        Write(fd_log, report, m_size);
    exit(0);
}
void initialize_the_sensors()
{
    // Analog A0/A1 address 1
    m_sensor = mraa_aio_init(TEMP_SENSOR_IO_PIN);
    if (m_sensor == NULL)
        error( "Error Occured when initializing the temperture sensor with MARR pin num 1");
    // GPIO_50 address 60
    m_button = mraa_gpio_init(BUTTON_IO_PIN);
    if (m_button == NULL)
    {
        error("Error Occured when initializing the button sensor with MARR pin num 60" );
    }
    // deafult set ststus with success as Expected \Response
    mraa_result_t status = MRAA_SUCCESS;
    status = mraa_gpio_dir(m_button, MRAA_GPIO_IN);
    if ( status != MRAA_SUCCESS)
        error("Error occured when setting the button input direction");
    // let button send out signals
    status = mraa_gpio_isr(m_button, MRAA_GPIO_EDGE_RISING, &do_when_button_pressed, NULL);
    if ( status != MRAA_SUCCESS)
        error("Error occured when setting the button interrupt");
}

void safeExit()
{
    mraa_aio_close(m_sensor);
    mraa_gpio_close(m_button);
}
/*
 SCALE=F
 This command should cause all subsequent reports to be generated in degrees Fahrenheit.
 SCALE=C
 This command should cause all subsequent reports to be generated in degrees Centegrade
 PERIOD=seconds
 This command should change the number of seconds between reporting intervals. It is acceptable if this command does not take effect until after the next report.
 STOP
 This command should cause the program to stop generating reports, but continue processing input commands. If the program is not generating reports, merely log receipt of the command.
 START
 This command should cause the program to, if stopped, resume generating reports. If the program is not stopped, merely log receipt of the command.
 LOG line of text
 This command requires no action beyond logging its receipt (the entire command, including the LOG). In project 4C, the server may send such commands to help you debug problems with your reports.
 OFF
 This command should, like pressing the button, output (and log) a time-stamped SHUTDOWN message and exit.

 */
void processInput( char* command)
{
    int isValid =1;
    // int isOff=0;
    // check command
    // check stop first to save meaninless check time
    if( !strcmp(command, "STOP"))
        doReport = 0;
    else if (!strcmp(command, "START"))
        doReport = 1;
    else if(!strcmp(command, "SCALE=F"))
        m_scale = 1;
    else if(!strcmp(command, "SCALE=C"))
        m_scale = 0;
    else if (!strncmp(command, "PERIOD=",7))
        m_period = atoi(command + 7);
    else if(!strncmp(command, "LOG", 3)){}
    else if (!strcmp(command, "OFF")){
        if(islogSet)
        {
            size_t size = strlen(command);
            Write(fd_log, command, size);
            Write(fd_log, &strend, 1);
        }
        do_when_button_pressed();
    }
    else{
        isValid = 0;
    }
    
    
    if(islogSet && isValid)
    {
        size_t size = strlen(command);
        Write(fd_log, command, size);
        Write(fd_log, &strend, 1);
        // if(isOff)
        //     do_when_button_pressed();
    }
    
}

// options
static struct option long_options[] =
{
    { "period",    required_argument, NULL, 'p' },
    { "scale",     required_argument, NULL, 's' },
    { "log",       required_argument, NULL, 'l' },
    {     0,             0,           0,     0  }
};

int main(int argc, char *argv[])
{
    int opt;
    while( (opt = getopt_long(argc, argv, "i:o:sc", long_options, NULL) ) != -1) 
    {
        switch( opt )
        {
            case 'p':
            {
                m_period = atoi(optarg);
                break;
            }
            case 's':
            {
                char scale= optarg[0];
                if( scale != 'F' && scale != 'C')
                    error("Invliad Argument !! Usage: --scale=[C, F]");

                if(scale == 'C')
                    m_scale = 0;

                break;
            }
            case 'l':
            {
                islogSet = 1;
                log_name = optarg;
                fd_log = creat(log_name, 0666);
                if (fd_log < 0)
                {
                    fprintf(stderr,"Error Occured when creating the file: %s\n",log_name);
                    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
                    exit(1);
                }
                break;
            }
            default:
            {
                fprintf( stderr, " Unrecongized Option %s\n", argv[optind-1]);
                fprintf( stderr,
                        "Usage: lab4b --period= #number,--scale= [C,F], --log=filename");
                exit(1);  
            }
      
        } // end switch 
        
    } // end opt 
    
    initialize_the_sensors();
    atexit(safeExit); // make sure that the sensor is close properly when before any exit 


    // using poll to detect if there is any input from stdin 
    struct pollfd pollfds;
    int timeout = 100;
    pollfds.fd = STDIN_FILENO;
    pollfds.events = POLLIN; // waititing for a input event
    time_t currentTime;
    time_t nextTimesUP = 0 ;
    // buff for input
    char input_buf[MAX_BYTE];
    char commands[MAX_BYTE];
    int pos = 0;
    while(1)
    {
        currentTime = time(NULL); // get my time now

        // check if it is time to report the temperture if the flag is sert 
        if( doReport && (currentTime >= nextTimesUP ))
        {
            // get the temperture reading and convert to human readable 
            float temp_read = convert_temper_reading(mraa_aio_read(m_sensor));
            struct timespec ts;
            struct tm * tm;
            clock_gettime(CLOCK_REALTIME, &ts);
            // convert the time to human readable
            tm = localtime(&(ts.tv_sec));
            char report[100];
            sprintf(report, "%02d:%02d:%02d %.1f\n", tm->tm_hour,
                    tm->tm_min, tm->tm_sec, temp_read);
            size_t m_size = strlen(report);
            // output to stdout 
            Write(STDOUT_FILENO, report, m_size);
            // if we do have a log file then log it
            if (islogSet)
                Write(fd_log, report, m_size);
            // update my next times up
            nextTimesUP = currentTime+m_period;
        } // end reporting  

       // expand the code to accept insturctions from STDIN 
        int pollret = poll(&pollfds, 1 , timeout);
        if( pollret < 0)
        {
            error(" Error Occured when calling poll()");
        }else if ( pollret > 0) // event detected 
        {
            // read input
            long byte_read = read( STDIN_FILENO,input_buf,MAX_BYTE);
            // process the input until hits an \n
            int i =0;
            for( ; i < byte_read ; i++)
            {
                // locality
                char arg = input_buf[i];
                if( arg != '\n')
                {
                    commands[pos]=arg;
                    pos++;
                    if(pos > MAX_BYTE){
                        error("BAD ACCESS");
                    }
                }else
                {
                    ////////////
                    commands[pos]='\0';
                    // process input
                    processInput(commands);
                    // reset pos
                    pos = 0;
                }
            }
        }
    }
}// end main


