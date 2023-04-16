#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: %s <serial_port>\n", argv[0]);
        exit(1);
    }

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

    char buffer[255];
    ssize_t num_bytes;

    while (1) {

        gets(buffer);
        printf("%s\n", buffer);
        num_bytes = write(fd, buffer, strlen(buffer));
        if (num_bytes == -1) {
            perror("Error writing to serial port");
            close(fd);
            exit(1);
        }
        printf("Wrote %ld bytes to serial port.\n", num_bytes);
    }

    close(fd);
    return 0;
}
