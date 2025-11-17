#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include "blocks.h"

int set_interface_attribs(int fd, int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) return -1;
    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB; tty.c_cflag &= ~CSTOPB;
    tty.c_iflag = tty.c_oflag = tty.c_lflag = 0;
    tty.c_cc[VMIN] = 1; tty.c_cc[VTIME] = 5;
    return tcsetattr(fd, TCSANOW, &tty) == 0 ? 0 : -1;
}

int count_H_frames(const char *s) {
    int count = 0;
    const char *p = s;
    while ((p = strstr(p, "[H")) != NULL) {
        count++;
        p += 2; // Move past this occurrence
    }
    return count;
}


int main() {
    int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening /dev/ttyUSB0: %s\n", strerror(errno));
        return 1;
    }
    set_interface_attribs(fd, B9600);

    
    int total_frames = 0;
    for (int i = 10; i < 40; ++i) {
        size_t len = strlen(blocks[i]);
        int frames_in_block = count_H_frames(blocks[i]);
        total_frames += frames_in_block;
        printf("\n=== Sending burst/block %d (%zu bytes, %d frames): ===\n%s\n",
            i+1, len, frames_in_block, blocks[i]);
        ssize_t written = write(fd, blocks[i], len);
        tcdrain(fd);
        if (written < 0) {
            printf("Write error on block %d: %s\n", i+1, strerror(errno));
        } else {
            printf("Sent block %d (%zd bytes)\n", i+1, written);
        }
        sleep(1);
    }
    printf("\n==== TOTAL FRAMES SENT: %d ====\n", total_frames);

    close(fd);
    return 0;
}
