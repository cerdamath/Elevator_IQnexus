#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include "elevator_blocks.h"
#include <stdbool.h>

int count_H_frames(const char *s) {
    int count = 0;
    const char *p = s;
    while ((p = strstr(p, "[H")) != NULL) {
        count++;
        p += 2; // Move past this occurrence
    }
    return count;
}


static speed_t baud_to_constant(int baud)
{
    switch (baud)
    {
    case 50:
        return B50;
    case 75:
        return B75;
    case 110:
        return B110;
    case 134:
        return B134;
    case 150:
        return B150;
    case 200:
        return B200;
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 1800:
        return B1800;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
#ifdef B57600
    case 57600:
        return B57600;
#endif
#ifdef B115200
    case 115200:
        return B115200;
#endif
#ifdef B230400
    case 230400:
        return B230400;
#endif
    default:
        return 0; // invalid
    }
}

// Read and collapse multiple bytes into at most one logical command per call.
bool validateCommand(char expectedCommand, int fd)
{
    char buf[64];
    bool match = false;

    ssize_t n = read(fd, buf, sizeof buf);
    if (n <= 0)
    {
        return false; // nothing this cycle
    }

    // Debug: see what arrived this frame
    printf("Received %zd byte(s): ", n);
    for (ssize_t i = 0; i < n; ++i)
    {
        printf("%c", buf[i]);
        if (buf[i] == expectedCommand && !match)
        {
            match = true; // only the *first* matching command counts
        }
    }
    printf("Received %zd byte(s): ", n);
    for (ssize_t i = 0; i < n; ++i)
    {
        printf("      %02X ", (unsigned char)buf[i]);
    }
    printf("\n");
    printf("        \n");

    return match;
}

int setup_serial(const char *port_path, int baud_rate, int databits, int parity, int stopbits)
{
    int fd = open(port_path, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0)
    {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    // Map baud int -> termios speed_t
    speed_t speed = baud_to_constant(baud_rate);
    if (speed == 0)
    {
        fprintf(stderr, "Unsupported baud rate: %d\n", baud_rate);
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag |= (CLOCAL | CREAD);

    // Data bits
    tty.c_cflag &= ~CSIZE;
    switch (databits)
    {
    case 5:
        tty.c_cflag |= CS5;
        break;
    case 6:
        tty.c_cflag |= CS6;
        break;
    case 7:
        tty.c_cflag |= CS7;
        break;
    case 8:
    default:
        tty.c_cflag |= CS8;
        break;
    }

    // Parity: 0 = none, 1 = odd, 2 = even
    tty.c_cflag &= ~(PARENB | PARODD);
    if (parity == 1)
    {
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
    }
    else if (parity == 2)
    {
        tty.c_cflag |= PARENB;
        // PARODD already cleared above => even
    }

    // Stop bits
    if (stopbits == 2)
    {
        tty.c_cflag |= CSTOPB;
    }
    else
    {
        tty.c_cflag &= ~CSTOPB;
    }

    // Raw-ish mode
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK);
    tty.c_oflag &= ~OPOST;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        fprintf(stderr,
                "Usage: %s <serial_port> <baud_rate> <databits> <parity> <stop_bits>\n",
                argv[0]);
        return 1;
    }

    const char *port_path = argv[1];
    int baud_rate = atoi(argv[2]);
    int databits = atoi(argv[3]);
    int parity = atoi(argv[4]);    // 0 = none, 1 = odd, 2 = even
    int stop_bits = atoi(argv[5]); // 1 or 2

    printf("Opening device: %s\n", port_path);
    printf("Port: %s Baud: %d Databits: %d Parity: %d Stop_bits: %d ", port_path, baud_rate, databits, parity, stop_bits);

    // Setup serial port
    int fd = setup_serial(port_path, baud_rate, databits, parity, stop_bits);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        return 1;
    }

    int total_frames = 0;
    for (int i = 0; i < 62; ++i) {
         printf("\n==== FRAME SENT: %s ====\n", blocks[i]);
        ssize_t written = write(fd, blocks[i], strlen(blocks[i]));
        if (written < 0)
        {
            printf("ERROR WRITING");
        }
        tcdrain(fd);
        usleep(1000*100);
    }

    close(fd);
    return 0;
}