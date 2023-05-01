#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

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
#define BCC_UA A_tx ^ C_UA

typedef enum{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} UAstateMachineStates;

UAstateMachineStates currentStateUA = START;

void stateMachineUA(unsigned char block){
    unsigned char A, C;

    switch (currentStateUA)
    {
    case START:
        if(block == FLAG){
            currentStateUA = FLAG_RCV;
            break;
        }else{
            break;
        }
    case FLAG_RCV:
        if(block == A_tx){
            A = block;
            currentStateUA = A_RCV;
            break;
        }else if(block == FLAG){
            break;
        }else{
            currentStateUA = START;
            break;
        }
    case A_RCV:
        if(block == C_UA){
            C = block;
            currentStateUA = C_RCV;
            break;
        }else if(block == FLAG){
            currentStateUA = FLAG_RCV;
            break;
        }else{
            currentStateUA = START;
            break;
        }
    case C_RCV:
        if((A ^ C) == BCC_UA){
            currentStateUA = BCC_OK;
            break;
        }else if(block == FLAG){
            currentStateUA = FLAG_RCV;
            break;
        }else{
            currentStateUA = START;
            break;
        }
    case BCC_OK:
        printf("UA RECEBIDO\n");
        if(block == FLAG){
            currentStateUA = STOP;
            break;
        }else{
            currentStateUA = START;
            break;
        }
    case STOP:
        break;
    }
    
}


int main(int argc, char** argv)
{
    /* if (argc != 2) {
        printf("Usage: %s <serial_port>\n", argv[0]);
        exit(1);
    } */

    argv[1] = "/dev/ttyS11";

    int fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening serial port");
        exit(1);
    }

    struct termios tio;
    if (tcgetattr(fd, &tio) == -1) {
        perror("Error getting serial port attributes");
        close(fd);
        exit(1);
    }

    tio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VTIME] = 0;  // Set VTIME to 0 for non-canonical mode
    tio.c_cc[VMIN] = 1;   // Set VMIN to 1 for non-canonical mode

    if (tcsetattr(fd, TCSANOW, &tio) == -1) {
        perror("Error setting serial port attributes");
        close(fd);
        exit(1);
    }

    unsigned char frame[5];
    char buffer[1];
    ssize_t num_bytes, bytes_read;
    
    frame[0] = FLAG;
    frame[1] = A_tx;
    frame[2] = C_SET;
    frame[3] = BCC_SET;
    frame[4] = FLAG;


    while (1) {

        gets(buffer);
        /* printf("%s\n", buffer); */
        printf("SENDING FRAME\n");
        num_bytes = write(fd, frame, sizeof(frame));
        if (num_bytes == -1) {
            perror("Error writing to serial port");
            close(fd);
            exit(1);
        }
        printf("Wrote %ld bytes to serial port.\n", num_bytes);
        while (1){
            bytes_read = read(fd, buffer, 1);
            printf("Received: %x\n", buffer[0]);
            if(buffer[0] == FLAG){
                printf("FLAG RECEBIDA\n");
                printf("%x\n", buffer[0]);
            }
            stateMachineUA(buffer[0]);
        }
    }

    close(fd);
    return 0;
}
