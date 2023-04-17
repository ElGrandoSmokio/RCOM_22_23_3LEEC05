#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    struct termios oldtio,newtio;

    if (argc < 2) {
        printf("Usage: %s <serial_port>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */



    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */


    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    
    
    
    char bufferWrite[255];
    char bufferRead[255];
    ssize_t num_bytes, bytesRead;
    int i=0;

    while (1) {
    
        gets(bufferWrite);
        printf("Sent: %s\n", bufferWrite);
        num_bytes = write(fd, bufferWrite, strlen(bufferWrite));
        if (num_bytes == -1) {
            perror("Error writing to serial port");
            close(fd);
            exit(1);
        }
        printf("Wrote %ld bytes to serial port.\n", num_bytes);
        bzero(bufferRead, sizeof(bufferRead));
        printf("Reading from serial port...\n");
        read(fd, bufferRead, sizeof(bufferRead));
        printf("Read:%s\n", bufferRead);
  
    }

    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 0;
}