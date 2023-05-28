#include "linklayer.h"

#define FLAG 0x5C
#define A_tx 0x01  // Tx -> Rx -> Tx
#define A_rx 0x03  // Rx -> Tx -> Rx
#define C_SET 0x03
#define C_DISC 0x11
#define C_UA 0x07
#define BCC_SET A_tx ^ C_SET
#define BCC_UA A_tx ^ C_UA
#define BCC_DISC A_tx ^ C_DISC

#define RR0 0b00000001
#define RR1 0b00100001
#define REJ0 0b00000101
#define REJ1 0b00100101
#define I0   0x00
#define I1   0x02

#define escapeByte 0x5d

#define BAUDRATE B38400

#include <sys/signal.h>

struct termios oldtio,newtio;

int fd;
int numS = 0;
int numR = 1;
int END = 0;
int role;
int payloadTracker = 0, payloadSize = 1;
int rejeitado = 0;

int messBCC2 = 0;
int countMess = 0;

//int fileTracker = 0, fdTrack;

int flagAlarme = 1, countAlarme = 0;
int numTries, timeOut;
int parada = 0;

void alarmHandler(){
    flagAlarme = 1;
    countAlarme++;
    printf("ALARME: %d\n", countAlarme);
}

int get_baud(int baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
    case 2500000:
        return B2500000;
    case 3000000:
        return B3000000;
    case 3500000:
        return B3500000;
    case 4000000:
        return B4000000;
    default: 
        return -1;
    }
}

char *payloadSetup(char *buf, int bufSize, unsigned char* payload){
    unsigned char pouch[2], temp;
    payload = (unsigned char *)malloc((1000) * sizeof(unsigned char));
    payload[0] = buf[0];
    int count = 0;
    payloadSize = 1;

    unsigned char array[1500];
    bzero(&array, 1500);


    for(int i = 1 + payloadTracker; i < bufSize; i++){
        if(payloadSize == 2345){
            payloadTracker += count;
            return payload;
            break;
        }else{
            count++;
            if(buf[i] == FLAG){
                payload = (unsigned char *)realloc(payload, (bufSize + 1) * sizeof(unsigned char));
                payload[count] = 0x5d;
                /* printf("payload[%d]:%x\n", count, payload[count]) */;
                memcpy(array, payload, payloadSize + 2);
                count++;
                payload[count] = 0x7c;
                /* printf("payload[%d]:%x\n", count, payload[count]); */
                memcpy(array, payload, payloadSize + 2);
                payloadSize += 2;
            }else if(buf[i] == escapeByte){
                payload = (unsigned char *)realloc(payload, (bufSize + 1) * sizeof(unsigned char));
                payload[count] = 0x5d;
                /* printf("payload[%d]:%x\n", count, payload[count]); */
                memcpy(array, payload, payloadSize + 2);
                count++;
                payload[count] = 0x7d;
                /* printf("payload[%d]:%x\n", count, payload[count]); */ 
                memcpy(array, payload, payloadSize + 2);
                /* memcpy(payload + payloadSize, pouch, 2); */
                payloadSize += 2;
            }else{
                unsigned char temp = buf[i];
                payload[count] = buf[i];
                /* printf("payload[%d]:%x\n", count, payload[count]); */
                memcpy(array, payload, payloadSize);
                /* payload = (char *)realloc(payload, 1);
                payload[payloadSize] = buf[i]; */
                /* bzero(&array, 1000);
                memcpy(array, payload, payloadSize); */
                payloadSize++;
            }
        }
    }
    payloadTracker += count;
    return payload;

}

void IframeSender(int fd, unsigned char *payload, int payloadSize, unsigned char BCC2){
    unsigned char *frameI = (unsigned char *)malloc((1000 + 6) * sizeof(unsigned char));
    unsigned char control, BCC1;
    unsigned char array[1500];
    bzero(&array, 1500);
    memcpy(array, payload, payloadSize);
    if(numS == 0){
        control = I0;
        BCC1 = A_tx ^ control;   
    }else if(numS == 1){
        control = I1;
        BCC1 = A_tx ^ control;
    }
    
    if(messBCC2 == 1){
        printf("BCC2 ERRADO");
        BCC2 ^= 1;
        messBCC2 = 0;
        countMess++;
    }    
    
    unsigned char header[4] = {FLAG, A_tx, control, BCC1};
    unsigned char tail[2] = {BCC2, FLAG};
    memcpy(frameI, header, 4);
    memcpy(array, payload, payloadSize + 4);

    memcpy(frameI + 4, payload, payloadSize);
    memcpy(array, frameI, 4+payloadSize);

    memcpy(frameI + 4 + payloadSize, tail, 2);
    memcpy(array, frameI, 4+payloadSize+2);

    printf("\n");
   /*  for(int i=0; i < payloadSize + 6; i++){
        printf("frameI[%d]: %x\n", i, frameI[i]);
    } */
    write(fd, frameI, payloadSize + 6);
    free(frameI);

}

void responseControlFrame(int fd, char controlField){
    char frame[5];
    frame[0] = FLAG;
    frame[1] = A_tx;
    frame[2] = controlField;
    frame[3] = frame[1] ^ frame[2];
    frame[4] = FLAG;
    write(fd, frame, 5);
}

char byteDestuffer(int fd){

    char buf;
    read(fd, &buf, 1);
    if(buf == 0x7c){
        return FLAG;
    }else if(buf == 0x7d){
        return escapeByte;
    }else{
        printf("CARACTERE INVALIDO APOS ESCAPE BYTE");
        exit(-1);
    }
}

int checkBCC2(unsigned char *message, int messageSize){
    unsigned char array[1200];
    bzero(&array, 1200);
    memcpy(array, message, messageSize + 3);
    unsigned char tailBCC2 = message[messageSize - 1];
    unsigned char localBCC2 = message[0];
    for(int i=1; i < messageSize - 1; i++){
        /* printf("BCC2[%d]: %d   ", i, localBCC2);
        printf("%x\n", message[i]); */
        localBCC2 ^= message[i]; 
        if(i > 995){
            /* printf("BCC2: %d  ", localBCC2); */
            /* printf("%x\n", message[i]); */
        } 
    }
    /* printf("BCC2: %d  \n", localBCC2); */
    message[messageSize - 1] = 0;

    if(tailBCC2 == localBCC2){
        return 1;
    }
    else{
        return -1;
    }

}

typedef enum{
    START_UA,
    FLAG_RCV_UA,
    A_RCV_UA,
    C_RCV_UA,
    BCC_OK_UA,
    STOP_UA
} UAstateMachineStates;
void stateMachineUA(int fd){
    UAstateMachineStates currentStateUA = START_UA;
    char A, C, block;
    while((currentStateUA != STOP_UA) && (flagAlarme != 1)){
        read(fd, &block, 1);
        switch (currentStateUA)
        {
        case START_UA:
            if(block == FLAG){
                currentStateUA = FLAG_RCV_UA;
                break;
            }else{
                break;
            }
        case FLAG_RCV_UA:
            if(block == A_tx){
                A = block;
                currentStateUA = A_RCV_UA;
                break;
            }else if(block == FLAG){
                break;
            }else{
                currentStateUA = START_UA;
                break;
            }
        case A_RCV_UA:
            if(block == C_UA){
                C = block;
                currentStateUA = C_RCV_UA;
                break;
            }else if(block == FLAG){
                currentStateUA = FLAG_RCV_UA;
                break;
            }else{
                currentStateUA = START_UA;
                break;
            }
        case C_RCV_UA:
            if((A ^ C) == BCC_UA){
                currentStateUA = BCC_OK_UA;
                break;
            }else if(block == FLAG){
                currentStateUA = FLAG_RCV_UA;
                break;
            }else{
                currentStateUA = START_UA;
                break;
            }
        case BCC_OK_UA:
            printf("UA RECEBIDO\n");
            if(block == FLAG){
                currentStateUA = STOP_UA;
                alarm(0);
                parada = 1;
                break;
            }else{
                currentStateUA = START_UA;
                break;
            }
        case STOP_UA:
            break;
        }
    }
}

typedef enum{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} SETstateMachineStates;

void stateMachineSET(int fd){
    SETstateMachineStates currentStateSET = START;
    char A, C, block;
    while(currentStateSET != STOP){
        read(fd, &block, 1);
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
}

typedef enum{
    START_R,
    FLAG_RCV_R,
    A_RCV_R,
    C_RCV_R,
    BCC_OK_R,
    STOP_R
} RstateMachineStates;
void stateMachineRR(int fd){
    RstateMachineStates currentStateR = START_R;
    char A, C, block;
    while((currentStateR != STOP_R) && (flagAlarme != 1)){
        read(fd, &block, 1);
        block = (char)block;
        switch (currentStateR)
        {
        case START_R:
            if(block == FLAG){
                currentStateR = FLAG_RCV_R;
                break;
            }else{
                break;
            }
        case FLAG_RCV_R:
            if(block == A_tx){
                A = block;
                currentStateR = A_RCV_R;
                break;
            }else if(block == FLAG){
                break;
            }else{
                currentStateR = START_R;
                break;
            }
        case A_RCV_R:
        printf("block RR: %x\n", block);
            if((block == RR1) && (numS == 0)){
                numS = 1;
                numR = 0;
                C = block;
                currentStateR = C_RCV_R;
                rejeitado = 0;
                break;
            }else if((block == RR0) && (numS == 1)){
                numS = 0;
                numR = 1;
                C = block;
                currentStateR = C_RCV_R;
                rejeitado = 0;
                break;
            }else if((block == REJ1) && (numS == 0)){
                numS = 1;
                numR = 0;
                rejeitado = 1;
                C = block;
                currentStateR = C_RCV_R;
                
                break;
            }else if((block == REJ0) && (numS == 1)){
                numS = 0;
                numR = 1;
                rejeitado = 1;
                C = block;
                currentStateR = C_RCV_R;
                break;
            }else if(block == FLAG){
                currentStateR = FLAG_RCV_R;
                break;
            }else{
                currentStateR = START_R;
                break;
            }
        case C_RCV_R:
            if((A ^ C) == block){
                currentStateR = BCC_OK_R;
                break;
            }else if(block == FLAG){
                currentStateR = FLAG_RCV_R;
                break;
            }else{
                currentStateR = START_R;
                break;
            }
        case BCC_OK_R:
            printf("RESPOSTA RECEBIDA\n");
            if(block == FLAG){
                currentStateR = STOP_R;
                alarm(0);
                parada = 1;
                break;
            }else{
                currentStateR = START_R;
                break;
            }
        case STOP_R:
            break;
        }
    }
}

typedef enum{
    START_I,
    FLAG_RCV_I,
    A_RCV_I,
    C_RCV_I,
    BCC1_OK_I,
    BCC2_OK_I,
    STOP_I,
    DISC,
    ESCAPE
} IstateMachineStates;
int stateMachineI(int fd, char *buffer){
    IstateMachineStates currentStateI = START_I;
    unsigned char *message = (unsigned char *)malloc(0);
    char A, C, block;
    int S, i=0;
    int messageSize = 0;
    while(currentStateI != STOP_I){
        read(fd, &block, 1);
        switch (currentStateI)
        {
        case START_I:
            if(block == FLAG){
                currentStateI = FLAG_RCV_I;
                break;
            }else{
                break;
            }
        case FLAG_RCV_I:
            if(block == A_tx){
                A = block;
                currentStateI = A_RCV_I;
                break;
            }else if(block == FLAG){
                break;
            }else{
                currentStateI = START_I;
                break;
            }
        case A_RCV_I:
            if((block == 0b00000000) && (numS == 0)){
                C = block;
                currentStateI = C_RCV_I;
                numS = 1;
                S = 0;
                break;
            }else if((block == 0b00000010) && (numS == 1)){
                C = block;
                currentStateI = C_RCV_I;
                numS = 0;
                S = 2;
                break;
            }else if(block == FLAG){
                currentStateI = FLAG_RCV_I;
                break;
            }else if(block == C_DISC){
                currentStateI = DISC;
                break;
            }else{
                currentStateI = START_I;
                break;
            }
        case C_RCV_I:
            if(block == A_tx ^ C){
                currentStateI = BCC1_OK_I;
                break;
            }else if(block == FLAG){
                currentStateI = FLAG_RCV_I;
                break;
            }else{
                currentStateI = START_I;
                break;
            }
        case BCC1_OK_I:
            //printf("BCC1 OK\n");
            if(block == FLAG){
                if(checkBCC2(message, messageSize) == 1){
                    if(S == 0){
                        numR = 0;
                        numS = 1;
                        memcpy(buffer, message, messageSize);
                        responseControlFrame(fd, RR1);
                        rejeitado = 0;
                    }else if(S == 2){
                        numR = 1;
                        numS = 0;
                        memcpy(buffer, message, messageSize);
                        responseControlFrame(fd, RR0);
                        rejeitado = 0;
                    }
                }
                else{
                    if(S == 0){
                        numR = 0;
                        numS = 1;
                        responseControlFrame(fd, REJ1);
                        rejeitado = 1;
                        stateMachineI(fd, buffer);
                    }else if(S == 2){
                        numR = 1;
                        numS = 0;
                        responseControlFrame(fd, REJ0);
                        rejeitado = 1;
                        stateMachineI(fd, buffer);
                    }
                }
                currentStateI = STOP_I;
                return messageSize - 1;
                break;
            }else if(block == escapeByte){
                unsigned char pouch = byteDestuffer(fd);

                //TESTE
                /* if(messageSize > 0){
                    unsigned char testBuf[1];
                    lseek(fdTrack, fileTracker, SEEK_SET);
                    read(fdTrack, testBuf, 1);
                    
                    if(testBuf != pouch){
                        printf("BYTE DIFERENTE:\n POUCH[%d]: %x E FILE[%d]: %x\n", fileTracker, pouch, fileTracker, testBuf);
                    }
                    fileTracker++;
                } */
                //TESTE

                messageSize ++;
                message = (unsigned char *)realloc(message, (messageSize) * sizeof(unsigned char));
                message[messageSize - 1] = pouch;
                i++;
                break;
            }else{

                //TESTE
                /* if(messageSize > 0){
                    unsigned char testBuf[1];
                    lseek(fdTrack, fileTracker, SEEK_SET);
                    read(fdTrack, testBuf, 1);
                    
                    if(testBuf != block){
                        printf("BYTE DIFERENTE:\n BLOCK[%d]: %x E FILE[%d]: %x\n", fileTracker, block, fileTracker, testBuf);
                    }
                    fileTracker++;
                } */
                //TESTE

                messageSize++;
                message = (unsigned char *)realloc(message, (messageSize) * sizeof(unsigned char));
                message[messageSize - 1] = block;
                i++;
                break;
            }
        case STOP_I:
            break;
        case DISC:
            printf("DISCONNECTING\n");
            read(fd, &block, 1);
            responseControlFrame(fd, C_DISC);
            if(role == 1){
                stateMachineUA(fd);
            }else if(role == 0){
                responseControlFrame(fd, C_UA);
            }
            return -1;
            currentStateI = STOP_I;
            break;
        }
    }
    
}

typedef enum{
    START_DISC,
    FLAG_RCV_DISC,
    A_RCV_DISC,
    C_RCV_DISC,
    BCC_OK_DISC,
    STOP_DISC
} DISCstateMachineStates;

void stateMachineDISC(int fd){
    DISCstateMachineStates currentStateDISC = START_DISC;
    char A, C, block;
    while(currentStateDISC != STOP_DISC){
        read(fd, &block, 1);
        switch (currentStateDISC)
        {
        case START_DISC:
            if(block == FLAG){
                currentStateDISC = FLAG_RCV_DISC;
                break;
            }else{
                break;
            }
        case FLAG_RCV_DISC:
            if(block == A_tx){
                A = block;
                currentStateDISC = A_RCV_DISC;
                break;
            }else if(block == FLAG){
                break;
            }else{
                currentStateDISC = START_DISC;
                break;
            }
        case A_RCV_DISC:
            if(block == C_DISC){
                C = block;
                currentStateDISC = C_RCV_DISC;
                break;
            }else if(block == FLAG){
                currentStateDISC = FLAG_RCV_DISC;
                break;
            }else{
                currentStateDISC = START_DISC;
                break;
            }
        case C_RCV_DISC:
            if((A ^ C) == BCC_DISC){
                currentStateDISC = BCC_OK_DISC;
                break;
            }else if(block == FLAG){
                currentStateDISC = FLAG_RCV_DISC;
                break;
            }else{
                currentStateDISC = START_DISC;
                break;
            }
        case BCC_OK_DISC:
            if(block == FLAG){
                currentStateDISC = STOP_DISC;
                break;
            }else{
                currentStateDISC = START_DISC;
                break;
            }
        case STOP_DISC:
            break;
        }
    }
}

int llopen(linkLayer connectionParameters){

    numTries = connectionParameters.numTries;
    timeOut = connectionParameters.timeOut;
    role = connectionParameters.role;
    if((role != 0) && (role != 1)){
        printf("INVALID ROLE\n");
        return -1;
    }

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0) { return -1;}
    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        return -1;
    }

    int baud = get_baud(connectionParameters.baudRate);

    struct sigaction saio;

    /* install the signal handler before making the device asynchronous */
    saio.sa_handler = alarmHandler;/*signal handler address or SI_IGN ou SIG_DFL*/
    saio.sa_flags = 0;          /*gives  a  mask of signals which should */
                                /*be blocked during execution of the */
                                /*signal handler.  In addition,  the signal*/
                                /*which triggered the handler will be blocked,*/
                                /*unless the SA_NODEFER or SA_NOMASK flags */
                                /*are used.*/
    sigemptyset(&saio.sa_mask);
    if (sigaction(SIGALRM, &saio, NULL) == -1) {
        printf("Error setting up signal handler.\n");
        return -1;
    }

    //fcntl(fd, F_SETOWN, getpid());
    //fcntl(fd, F_SETFL, FASYNC);

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = baud | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        return -1;
    }

    printf("New termios structure set\n");

    if(role == 1){
        role = 1;
        stateMachineSET(fd);
        responseControlFrame(fd, C_UA);
        printf("RECEIVER READY\n");
    }else if(role == 0){
        role = 0;
        //(void) signal(SIGALRM, alarmHandler);  // instala rotina que atende interrupcao
        
        do{
            //printf("ALARM1\n");
            responseControlFrame(fd, C_SET);
            alarm(timeOut);
            flagAlarme = 0;
            //printf("ALARM2\n");
            while((parada == 0) && (flagAlarme == 0)){
                //printf("ALARM IN 1\n");
                stateMachineUA(fd);
                //printf("ALARM IN 2\n");
            }
            //printf("ALARM3\n");
            
        }while((flagAlarme == 1) && (countAlarme < numTries));
        if((flagAlarme == 1) && (countAlarme == numTries)){
            return -1;
        }
        else{
            flagAlarme = 0;
            countAlarme = 0;
            printf("TRANSMITTER READY\n");
            messBCC2 = 1;//MESS BCC2
            return fd;
        }
        
    }
}

int llclose(linkLayer connectionParameters, int showStatistics){
    if(role == 0){
        responseControlFrame(fd, C_DISC);
        stateMachineDISC(fd);
        responseControlFrame(fd, C_UA);
        tcsetattr(fd, TCSANOW, &oldtio);
        printf("CONNECTION TERMINATED\n");
        close(fd);
    }
    else if(role == 1){
        stateMachineDISC(fd);
        responseControlFrame(fd, C_DISC);
        stateMachineUA(fd);
        tcsetattr(fd, TCSANOW, &oldtio);
        printf("CONNECTION TERMINATED\n");
        close(fd);
    }
}

int llread(char* packet){
    //fdTrack = open("penguinFull.gif", O_RDONLY);
    int bytesRead = stateMachineI(fd, packet);
    if(bytesRead == -1){
        return -1;
    }
    
    return bytesRead;
}

int llwrite(char* buf, int bufSize){
    unsigned char *payload, BCC2;

    unsigned char testBuf[1000];

    payload = payloadSetup(buf, bufSize, payload);

    memcpy(testBuf, payload, 1000);

    if(countMess == 3){
        messBCC2 = 0;
    }else{
        messBCC2 = 1;
    }

    BCC2 = (unsigned char)buf[0];
    for(int i=1; i<bufSize; i++){
        BCC2 ^= (unsigned char)buf[i];
        if(i > 995){
            printf("%x\n", buf[i]);
        }  
    }
    parada = 0;
    do{     
            IframeSender(fd, payload, payloadSize, BCC2);
            
            alarm(timeOut);
            flagAlarme = 0;
            stateMachineRR(fd);
            while(rejeitado == 1){
                printf("REJEITADO");
                IframeSender(fd, payload, payloadSize, BCC2);
                stateMachineRR(fd);
                while((parada == 0) && (flagAlarme == 0)){
                    stateMachineRR(fd);
                }
            }   
            
            while((parada == 0) && (flagAlarme == 0)){
                stateMachineRR(fd);
            }
            
        }while((flagAlarme == 1) && (countAlarme < numTries));
        if((flagAlarme == 1) && (countAlarme == numTries)){
            return -1;
        }

    

    
    free(payload);
    countAlarme = 0; //CORRECAO 1
    rejeitado = 0;
    payloadTracker = 0;
    return payloadSize + 6;
}

