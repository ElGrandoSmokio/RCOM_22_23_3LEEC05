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

#define FLAG 0x5C
#define A_tx 0x01  // Tx -> Rx -> Tx
#define A_rx 0x03  // Rx -> Tx -> Rx
#define C_SET 0x03
#define C_DISC 0x11
#define C_UA 0x07
#define BCC_SET A_tx ^ C_SET

unsigned char frameUA[5];

typedef enum{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} SETstateMachineStates;

SETstateMachineStates currentStateSET = START;

void stateMachineSET(unsigned char block){
    unsigned char A, C;

    switch (currentStateSET)
    {
    case START:
        if(block == FLAG){
            currentStateSET = FLAG_RCV;
            break;
        }else{
            break;
        }
    case FLAG_RCV:
        if(block == A_tx){
            A = block;
            currentStateSET = A_RCV;
            break;
        }else if(block == FLAG){
            break;
        }else{
            currentStateSET = START;
            break;
        }
    case A_RCV:
        if(block == C_SET){
            C = block;
            currentStateSET = C_RCV;
            break;
        }else if(block == FLAG){
            currentStateSET = FLAG_RCV;
            break;
        }else{
            currentStateSET = START;
            break;
        }
    case C_RCV:
        if((A ^ C) == BCC_SET){
            currentStateSET = BCC_OK;
            break;
        }else if(block == FLAG){
            currentStateSET = FLAG_RCV;
            break;
        }else{
            currentStateSET = START;
            break;
        }
    case BCC_OK:
        printf("SET RECEBIDO\n");
        if(block == FLAG){
            currentStateSET = STOP;
            break;
        }else{
            currentStateSET = START;
            break;
        }
    case STOP:
        break;
    }
    
}

int main(int argc, char** argv) {

    unsigned char buf[1];
    ssize_t bytesRead, bytesWritten;
    struct termios oldtio,newtio;

    /* if (argc < 2) {
        printf("Usage: %s <serial_port>\n", argv[0]);
        exit(1);
    } */

    argv[1] = "/dev/ttyS10";

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
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



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

    
    while (1) {
        printf("Reading from serial port...\n");
        //bzero(buf, sizeof(buf));
        bytesRead = read(fd, buf, 1);
        if (bytesRead > 0) {
            printf("Received: %x\n", buf[0]);
        } else if (bytesRead == 0) {
            printf("No data received\n");
        } else {
            perror("Failed to read from serial port");
            return 1;
        }

        if(buf[0] == FLAG){
            printf("FLAG RECEBIDA\n");
            printf("%x\n", buf[0]);
        }
        stateMachineSET(buf[0]);

        if(currentStateSET == STOP){
            frameUA[0] = FLAG;
            frameUA[1] = A_tx;
            frameUA[2] = C_UA;
            frameUA[3] = frameUA[1] ^ frameUA[2];
            frameUA[4] = FLAG;

            bytesWritten = write(fd, frameUA, 5);
        }
        
        //printf("BUFFER: %x\n", buf);
        /* write(fd, buf, strlen(buf));
        printf("Returned: %s\n", buf); */
        //sleep(0.3);
    }
    

    close(fd);
    return 0;
}



