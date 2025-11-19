#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>
#include <stdint.h>
#include <stdbool.h>

#define FRAME_LENGTH 35
#define BUFFER_SIZE 1024
#define LCD_WIDTH 16

typedef enum
{
    test_pass = 0,
    test_fail = 1
} test_status_e;

typedef enum {
    MENU_PRINCIPAL,
    MENU_TCBC,
    MENU_SYSTEM,
    MENU_STATUS,
    MENU_INPUT
} sm_menu_e;

typedef struct
{
    void (*callback)();
    sm_menu_e position;
    char top_screen[LCD_WIDTH + 1];
    char bottom_screen[LCD_WIDTH + 1];
    char car_id[2];         // "A" + '\0'
    char direction[2];      // "-" + '\0'
    char level[3];          // "01" + '\0'
    char ocss[4];           // "IDL" + '\0'
    char mcss[3];           // "ST" + '\0'
    char door[3];           // "][" + '\0'
    char rear_door[3];      // "[]" + '\0'
    int frame_error_count;
} elevator_obj_t;

// Global variables
int serial_fd;
char buffer[BUFFER_SIZE];
int buffer_pos;
int frame_errors;
elevator_obj_t elevator = {0};

// Function prototypes
// At top of metro.c or in metro.h
int init(int argc, char* argv[]);
void read_screen(void);
void parse_screen();
void sm_menu(void);

int setup_serial(const char* port_path, speed_t baud_rate);
void clear_screen();
void print_lcd_screen(const char* top_screen, const char* bottom_screen, int error_count);
int validate_frame(const char* frame);
void extract_frame_parts(const char* frame, char* top_screen, char* bottom_screen);
void tcbc_menu_parse();
void principal_menu_parse();
void system_menu_parse();
void status_menu_parse();
void input_menu_parse();
void print_menu_position();

// Helpers to check substrings and input menu
bool contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}
bool is_input_menu(const char *top_screen) {
    char ch = top_screen[0];
    return (ch == 'A' || ch == 'B' || ch == 'C');
}

// Main logic
void dispatch_menu(const char *top_screen) {
    if (is_input_menu(top_screen)) {
        elevator.position = MENU_INPUT;
        input_menu_parse(top_screen);
        return;
    }

    if (contains(top_screen, "Menu")) {
        if (contains(top_screen, "SYSTEM")) {
            elevator.position = MENU_SYSTEM;
            system_menu_parse(top_screen);
            return;
        }
        if (contains(top_screen, "STATUS")) {
            elevator.position = MENU_STATUS;
            status_menu_parse(top_screen);
            return;
        }
        if (contains(top_screen, "TCBC")) {
            elevator.position = MENU_TCBC;
            tcbc_menu_parse(top_screen);
            return;
        }
        // If "Menu" is in top_screen but none of above, fallback:
        printf("Unknown Menu Screen (Menu keyword, not SYSTEM/STATUS/TCBC)\n");
        return;
    }

    // If no keywords above, principal menu by default
    elevator.position = MENU_PRINCIPAL;
    principal_menu_parse(top_screen);
}

void print_frame( char *buffer, int buffer_len )
{
    printf("\n\r");
    printf("BF:%u=",buffer_len);
    for(int j = 0; j < buffer_len; j++) {
        printf("%c", buffer[j]);
    }
    printf("\n\r");
}


int main(int argc, char* argv[]) 
{
    int status = init(argc, argv);   
    
    if (status != 0) {
        return 1;               
    }
    
    else {
        elevator.position = MENU_PRINCIPAL;
        read_screen();
        sm_menu();
        close(serial_fd);
        return 0; 
    }
}


int init(int argc, char* argv[]) 
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <serial_port>\n", argv[0]);
        return 1;
    }

    const char* port_path = argv[1];
    speed_t baud_rate = B9600;
    
    printf("Opening device: %s\n", port_path);
    printf("Baud: %u Port:%s", baud_rate, port_path);

    // Setup serial port
    serial_fd = setup_serial(port_path, baud_rate);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        return 1;
    }

    // Initialize buffer and variables
    // buffer[BUFFER_SIZE];
    buffer_pos = 0;
    frame_errors = 0;
    
    // Clear screen and start display
    clear_screen();
    
    printf("Reading from %s at %ld baud\n", port_path, (long)baud_rate);
    printf("Press Ctrl+C to exit\n");
    return 0;   
}


void read_screen()
{
     while (1) {
        // Wait for data to be available
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(serial_fd, &readfds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(serial_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select error");
            break;
        }
        
        if (activity == 0) {
            // Timeout, continue loop
            continue;
        }
        
        if (FD_ISSET(serial_fd, &readfds)) {
            // Read data from serial port
            char byte;
            int bytes_read = read(serial_fd, &byte, 1);
            
            if (bytes_read < 0) {
                perror("read error");
                break;
            }
            
            if (bytes_read == 0) {
                // No data available
                continue;
            }
            
            // Add byte to buffer
            buffer[buffer_pos] = byte;
            buffer_pos++;
            
            // If buffer is full, reset it
            if (buffer_pos >= BUFFER_SIZE) {
                buffer_pos = 0;
            }
            
            // Look for frame boundaries
            int frame_start = -1;
            
            // Find first newline character
            for (int i = 0; i < buffer_pos; i++) {
                if (buffer[i] == '\n') {
                    frame_start = i;
                    break;
                }
            }
            
            // If we found a newline, look for the end of the frame
            if (frame_start != -1) {
                // Check if we have enough data for a complete frame
                if (frame_start + FRAME_LENGTH <= buffer_pos) {
                    // Extract potential frame
                    char frame[FRAME_LENGTH + 1];
                    memcpy(frame, buffer + frame_start, FRAME_LENGTH);
                    frame[FRAME_LENGTH] = '\0';
                    
                    // Validate frame
                    if (validate_frame(frame)) {
                        // Extract parts for LCD display
                        extract_frame_parts(frame, elevator.top_screen, elevator.bottom_screen);
                        
                        // Print to screen
                        clear_screen();
                        print_menu_position();
                        print_lcd_screen(elevator.top_screen, elevator.bottom_screen, frame_errors);
                        dispatch_menu(elevator.top_screen);
                        
                        
                        
                        // Shift buffer to remove processed data
                        int shift_amount = frame_start + FRAME_LENGTH;
                        memmove(buffer, buffer + shift_amount, buffer_pos - shift_amount);
                        buffer_pos -= shift_amount;
                    } else {
                        // Frame validation failed
                        frame_errors++;
                        
                        // Find next potential frame start
                        int next_newline = -1;
                        for (int i = frame_start + 1; i < buffer_pos; i++) {
                            if (buffer[i] == '\n') {
                                next_newline = i;
                                break;
                            }
                        }
                        
                        // If we found a next newline, shift buffer to that position
                        if (next_newline != -1) {
                            memmove(buffer, buffer + next_newline, buffer_pos - next_newline);
                            buffer_pos -= next_newline;
                        } else {
                            // No next newline found, clear buffer
                            buffer_pos = 0;
                        }
                    }
                }
            }
        }
    }
}


int setup_serial(const char* port_path, speed_t baud_rate) {
    (void)baud_rate;
    int fd = open(port_path, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }
    
    // Set baud rate
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    
    // Configure for raw mode
    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // 1 stop bit
    tty.c_cflag &= ~CSIZE;   // Clear data size bits
    tty.c_cflag |= CS8;     // 8 data bits
    tty.c_cflag &= ~CLOCAL; // Disable hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines
    
    tty.c_lflag &= ~ICANON;  // No canonical input processing
    tty.c_lflag &= ~ECHO;    // No echo
    tty.c_lflag &= ~ECHOE;   // No echo erase
    tty.c_lflag &= ~ISIG;    // No signal chars
    tty.c_iflag &= ~IXON;    // No software flow control
    tty.c_iflag &= ~IXOFF;   // No software flow control
    tty.c_iflag &= ~IXANY;   // No software flow control
    tty.c_iflag &= ~IGNBRK;  // No ignore break
    tty.c_oflag &= ~OPOST;   // No output processing
    
    // Set timeouts
    tty.c_cc[VMIN] = 0;     // Non-blocking read
    tty.c_cc[VTIME] = 1;    // 0.1 second timeout
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    //usleep(1000*1000);
    
    return fd;
}

void clear_screen() {
    printf("\033[2J\033[H");  // VT100 escape codes to clear screen and move cursor to home
}

void print_lcd_screen(const char* top_screen, const char* bottom_screen, int error_count) {
    // Print top border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) {
        printf("-");
    }
    printf("+\n");
    
    // Print top line content
    if (top_screen != NULL) {
        printf("%-*s", LCD_WIDTH, top_screen);
    } else {
        for (int i = 0; i < LCD_WIDTH; i++) {
            printf(" ");
        }
    }
    printf("\n");
    
    // Print middle border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) {
        printf("-");
    }
    printf("+\n");
    
    // Print bottom line content
    if (bottom_screen != NULL) {
        printf("%-*s", LCD_WIDTH, bottom_screen);
    } else {
        for (int i = 0; i < LCD_WIDTH; i++) {
            printf(" ");
        }
    }
    printf("\n");
    
    // Print bottom border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) {
        printf("-");
    }
    printf("+\n");
    
    // Print error count
    printf("Frame errors = %d\n", error_count);
}


void print_menu_position() {
  printf("+----------------+\n");
  switch (elevator.position) {
    case MENU_PRINCIPAL:
        printf("  MENU_PRINCIPAL \n");    
        break;
    case MENU_TCBC:
        printf("    MENU_TCBC    \n");   
        break;
    case MENU_SYSTEM:
        printf("   MENU_SYSTEM   \n");   
        break;
    case MENU_STATUS:
        printf("   MENU_STATUS   \n");     
        break;
    case MENU_INPUT:
        printf("    MENU_INPUT   \n");     
        break;
    default:
    }
    printf("+----------------+\n");
}

int validate_frame(const char* frame) {
    // Check frame length
    if (strlen(frame) != FRAME_LENGTH) {
        return 0;
    }
    
    // Check if frame starts with newline
    if (frame[0] != '\n') {
        return 0;
    }
    
    // Check if 17th and 18th characters are "[H"
    if (frame[17] != '[' || frame[18] != 'H') {
        return 0;
    }
    return 1;
}

void extract_frame_parts(const char* frame, char* top_screen, char* bottom_screen) {
    // Extract the part after the newline and before "[H"
    const char* data_start = frame + 1;  // Skip the newline
    const char* marker_pos = strstr(data_start, "[H");
    
    if (marker_pos == NULL) {
        // If no "[H" found, return empty strings
        top_screen[0] = '\0';
        bottom_screen[0] = '\0';
        return;
    }
    
    // Calculate length of data before "[H"
    int data_length = marker_pos - data_start;
    
    // Split data into two parts
    int half_length = data_length;
    
    // Copy first half to bottom line
    if (half_length > 0) {
        strncpy(bottom_screen, data_start, half_length);
        bottom_screen[half_length] = '\0';
    } else {
        bottom_screen[0] = '\0';
    }
    
    // Copy second half to top line
    if (half_length > 0) {
        strncpy(top_screen, data_start + half_length + 2, half_length);
        top_screen[half_length] = '\0';
    } else {
        top_screen[0] = '\0';
    }

}

    void input_menu_parse() {
    memcpy(elevator.car_id,     elevator.top_screen + 0, 1);  elevator.car_id[1] = '\0';
    memcpy(elevator.direction,  elevator.top_screen + 1, 1);  elevator.direction[1] = '\0';
    memcpy(elevator.level,      elevator.top_screen + 2, 2);  elevator.level[2] = '\0';
    memcpy(elevator.ocss,       elevator.top_screen + 5, 3);  elevator.ocss[3] = '\0';
    memcpy(elevator.mcss,       elevator.top_screen + 9, 2);  elevator.mcss[2] = '\0';
    memcpy(elevator.door,       elevator.top_screen + 12, 2); elevator.door[2] = '\0';
    memcpy(elevator.rear_door,  elevator.top_screen + 14, 2); elevator.rear_door[2] = '\0';

    printf("\n+----------------+\n");
    printf(" Elevator Status \n");
    printf("+----------------+\n");

    printf(" Car ID    : %-3s \n", elevator.car_id);
    printf(" Direction : %-3s \n", elevator.direction);
    printf(" Level     : %-3s \n", elevator.level);
    printf(" OCSS      : %-3s \n", elevator.ocss);
    printf(" MCSS      : %-3s \n", elevator.mcss);
    printf(" Door      : %-3s \n", elevator.door);
    printf(" Rear Door : %-3s \n", elevator.rear_door);
    printf("+----------------+\n");

    }


void system_menu_parse () {

}

void status_menu_parse() {
    
}

void tcbc_menu_parse () {
    
}

void principal_menu_parse () {

}

void sm_menu()
{

}