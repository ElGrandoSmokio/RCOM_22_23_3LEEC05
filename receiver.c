#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char** argv) {

    int fd;
    char buf[255];
    ssize_t bytesRead;


    if (argc != 2) {
        printf("Usage: %s <serial_port>\n", argv[0]);
        exit(1);
    }

    
    // Open the serial port in non-canonical mode
    fd = open("/dev/ttyS10", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Failed to open serial port");
        return 1;
    }

    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 1;
    tcsetattr(fd, TCSANOW, &options);

    printf("Reading from serial port...\n");
    while (1) {
        bytesRead = read(fd, buf, sizeof(buf));
        if (bytesRead > 0) {
            printf("Received: %.*s\n", (int)bytesRead, buf);
        } else if (bytesRead == 0) {
            printf("No data received\n");
        } else {
            perror("Failed to read from serial port");
            return 1;
        }
    }

    close(fd);
    return 0;
}