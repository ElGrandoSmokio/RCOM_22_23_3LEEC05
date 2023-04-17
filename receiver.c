#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

int main(int argc, char** argv) {

    char buf[255];
    ssize_t bytesRead;
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

    printf("Reading from serial port...\n");
    while (1) {

        bzero(buf, sizeof(buf));
        bytesRead = read(fd, buf, sizeof(buf));
        if (bytesRead > 0) {
            printf("Received: %s\n", buf);
        } else if (bytesRead == 0) {
            printf("No data received\n");
        } else {
            perror("Failed to read from serial port");
            return 1;
        }
        
        printf("BUFFER: %s\n", buf);
        write(fd, buf, strlen(buf));
        printf("Returned: %s\n", buf);
        sleep(0.3);
    }
    

    close(fd);
    return 0;
}