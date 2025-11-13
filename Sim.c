#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include "blocks.h"

int set_interface_attribs(int fd, int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) { return -1; }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;

    tty.c_iflag = 0;    
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 5;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) { return -1; }
    return 0;
}

int main() {
    int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening /dev/ttyUSB0: %s\n", strerror(errno));
        return 1;
    }
    set_interface_attribs(fd, B9600);

    for(int i = 0; i < NUM_BLOCKS; i++){
        ssize_t written = write(fd, blocks[i], strlen(blocks[i]));
        if (written < 0) {
            printf("Write error on block %d: %s\n", i+1, strerror(errno));
        } else {
            printf("Sent block %d (%zd bytes)\n", i+1, written);
        }
        sleep(1); // Wait 1 second before sending next block
    }

    close(fd);
    return 0;
}
