#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <stdbool.h>

#define SVT_DATA "[H  local SVT     \n  disconnected  "
#define PRINCIPAL_DATA "[H1:TCBC  2:DRIVE \n5:SPBC  6:RMH   "
#define TCBC_DATA "[H  TCBC  - Menu  \nSystem=1 Tools=2"
#define SYSTEM_DATA "[H SYSTEM - Menu >\nStatus=1  Test=2"
#define STATUS_DATA "[H STATUS - Menu >\n Calls=1 Input=2"
#define INPUT_DATA "[HA-01 IDL ST ][][\n lwo LWX lns    "

typedef enum {
    NA,
    MENU_PRINCIPAL,
    MENU_TCBC,
    MENU_SYSTEM,
    MENU_STATUS,
    MENU_INPUT
} sm_menu_e;

int set_interface_attribs(int fd, int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) return -1;
    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB; tty.c_cflag &= ~CSTOPB;
    tty.c_iflag = tty.c_oflag = tty.c_lflag = 0;
    tty.c_cc[VMIN] = 0; tty.c_cc[VTIME] = 5; // read blocks for up to 0.5s
    return tcsetattr(fd, TCSANOW, &tty) == 0 ? 0 : -1;
}

// Reads one byte, checks if it matches expected (character, not string!)
bool validateCommand(char expectedCommand, int fd) {
    char c = 0;
    int n = read(fd, &c, 1);
    if (n == 1) {
        printf("Received: %c\n", c);
        if (c == expectedCommand) return true;
    }

    return false;
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <serial_port>\n", argv[0]);
        return 1;
    }

    const char* port_path = argv[1];
    int fd = open(port_path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening /dev/ttyUSB1: %s\n", strerror(errno));
        return 1;
    }
    set_interface_attribs(fd, B9600);

    sm_menu_e currentState = NA;
    char *currentData = SVT_DATA;
    while (1){
        
        printf("\n==== FRAME SENT: %s ====\n", currentData);
        ssize_t written = write(fd, currentData, strlen(currentData));
        if(written < 0) {
            printf("ERROR WRITING");
        }
        tcdrain(fd);
        // Attempt to read/validate a command
        bool changed = false;
        switch (currentState) {
        case NA:
            currentData = SVT_DATA;
            if (validateCommand('0', fd)) {
                currentState = MENU_PRINCIPAL;
                currentData = PRINCIPAL_DATA;
                changed = true;
            }
            break;
        case MENU_PRINCIPAL:
            currentData = PRINCIPAL_DATA;
            if (validateCommand('1', fd)) {
                currentState = MENU_TCBC;
                currentData = TCBC_DATA;
                changed = true;
            }
            break;
        case MENU_TCBC:
            currentData = TCBC_DATA;
            if (validateCommand('1', fd)) {
                currentState = MENU_SYSTEM;
                currentData = SYSTEM_DATA;
                changed = true;
            }
            break;
        case MENU_SYSTEM:
            currentData = SYSTEM_DATA;
            if (validateCommand('1', fd)) {
                currentState = MENU_STATUS;
                currentData = STATUS_DATA;
                changed = true;
            }
            break;
        case MENU_STATUS:
            currentData = STATUS_DATA;
            if (validateCommand('2', fd)) {
                currentState = MENU_INPUT;
                currentData = INPUT_DATA;
                changed = true;
            }
            break;
        case MENU_INPUT:
            currentData = INPUT_DATA;
            // Terminal/idle state – loop till reset
            break;
        default:
            currentState = NA;
            currentData = SVT_DATA;
            break;
        }

        // Optional: add 3 second pause if state changed.
        if (changed) {
            printf("Menu changed! Pausing for 1 seconds...\n");
            sleep(1);
        } else {
            usleep(1000 * 100); // 100 ms between prints/frames
        }
    }
    close(fd);
    return 0;
}
