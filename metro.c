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

#define FRAME_LENGTH 35
#define BUFFER_SIZE 1024
#define LCD_WIDTH 16


// Global variables
char frame[FRAME_LENGTH + 1];
char top_line[LCD_WIDTH + 1];
char bottom_line[LCD_WIDTH + 1];
int serial_fd;
char buffer[BUFFER_SIZE];
int buffer_pos;
int frame_errors;

// Function prototypes
// At top of metro.c or in metro.h
int init(int argc, char* argv[]);
void read_screen(void);
void parse_screen();
void sm_menu(void);

int setup_serial(const char* port_path, speed_t baud_rate);
void clear_screen();
void print_lcd_screen(const char* top_line, const char* bottom_line, int error_count);
int validate_frame(const char* frame);
void extract_frame_parts(const char* frame, char* top_line, char* bottom_line);


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

        read_screen();
        parse_screen();
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
                    memcpy(frame, buffer + frame_start, FRAME_LENGTH);
                    frame[FRAME_LENGTH] = '\0';
                    
                    print_frame(frame,FRAME_LENGTH);
    
                    // Validate frame
                    if (validate_frame(frame)) {
                        // Extract parts for LCD display
                        extract_frame_parts(frame, top_line, bottom_line);
                        
                        // Print to screen
                        clear_screen();
                        print_lcd_screen(top_line, bottom_line, frame_errors);
                        parse_screen();
                        
                        // Shift buffer to remove processed data
                        int shift_amount = frame_start + FRAME_LENGTH;
                        memmove(buffer, buffer + shift_amount, buffer_pos - shift_amount);
                        buffer_pos -= shift_amount;
                    } else {
                        printf("rejected\n");
    
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
    
    return fd;
}

void clear_screen() {
    printf("\033[2J\033[H");  // VT100 escape codes to clear screen and move cursor to home
}

void print_lcd_screen(const char* top_line, const char* bottom_line, int error_count) {
    // Print top border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) {
        printf("-");
    }
    printf("+\n");
    
    // Print top line content
    if (top_line != NULL) {
        printf("%-*s", LCD_WIDTH, top_line);
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
    if (bottom_line != NULL) {
        printf("%-*s", LCD_WIDTH, bottom_line);
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

void extract_frame_parts(const char* frame, char* top_line, char* bottom_line) {
    // Extract the part after the newline and before "[H"
    const char* data_start = frame + 1;  // Skip the newline
    const char* marker_pos = strstr(data_start, "[H");
    
    if (marker_pos == NULL) {
        // If no "[H" found, return empty strings
        top_line[0] = '\0';
        bottom_line[0] = '\0';
        return;
    }
    
    // Calculate length of data before "[H"
    int data_length = marker_pos - data_start;
    
    // Split data into two parts
    int half_length = data_length;
    
    // Copy first half to bottom line
    if (half_length > 0) {
        strncpy(bottom_line, data_start, half_length);
        bottom_line[half_length] = '\0';
    } else {
        bottom_line[0] = '\0';
    }
    
    // Copy second half to top line
    if (half_length > 0) {
        strncpy(top_line, data_start + half_length + 2, half_length);
        top_line[half_length] = '\0';
    } else {
        top_line[0] = '\0';
    }
    
    // Remove any trailing newlines or spaces
    // int i;
    // for (i = strlen(top_line) - 1; i >= 0 && (top_line[i] == '\n' || top_line[i] == ' '); i--) {
    //     top_line[i] = '\0';
    // }
    
    // for (i = strlen(bottom_line) - 1; i >= 0 && (bottom_line[i] == '\n' || bottom_line[i] == ' '); i--) {
    //     bottom_line[i] = '\0';
    // }



}



void parse_screen() {

    // Field variables
    char car_id[2]      = {0};
    char direction[2]   = {0};
    char level[3]       = {0};
    char ocss[4]        = {0};
    char mcss[3]        = {0};
    char front_door[3]  = {0};
    char rear_door[3]   = {0};

    // Parsing by table mapping
    car_id[0]        = top_line[0];
    direction[0]     = top_line[1];
    memcpy(level,      top_line + 2, 2);
    memcpy(ocss,       top_line + 5, 3);
    memcpy(mcss,       top_line + 9, 2);
    memcpy(front_door, top_line + 12, 2);
    memcpy(rear_door,  top_line + 14, 2);

    // Null-terminate all multi-char variables
    level[2]      = '\0';
    ocss[3]       = '\0';
    mcss[2]       = '\0';
    front_door[2] = '\0';
    rear_door[2]  = '\0';

    // Print results
    printf("Car ID      : %s\n", car_id);
    printf("Direction   : %s\n", direction);
    printf("Level       : %s\n", level);
    printf("OCSS        : %s\n", ocss);
    printf("MCSS        : %s\n", mcss);
    printf("Front Door  : %s\n", front_door);
    printf("Rear Door   : %s\n", rear_door);

}


// void s_menu_handler () {

// }

// void tcbc_menu_handler () {
    
// }

// void principal_menu_handler () {
    
// }

void sm_menu()
{

}