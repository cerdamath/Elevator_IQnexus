#include <stdio.h>
#include <string.h>     
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

int set_interface_attribs(int fd, int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) return -1;

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

    return tcsetattr(fd, TCSANOW, &tty) == 0 ? 0 : -1;
}

int main() {
    int fd = open("/dev/ttyUSB1", O_RDONLY | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening /dev/ttyUSB1: %s\n", strerror(errno));
        return 1;
    }
    set_interface_attribs(fd, B9600);

    char buf[4096];
    ssize_t bytes;
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, bytes, stdout); // Print directly to terminal
        fflush(stdout); // Flush for immediate display
    }

    close(fd);
    return 0;
}
