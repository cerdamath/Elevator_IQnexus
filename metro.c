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
#include <ctype.h>

#define FRAME_LENGTH 35
#define BUFFER_SIZE 1024
#define LCD_WIDTH 16
#define MAX_LABEL_LEN 9  
#define MAX_VALUE_LEN 6   
#define MAX_FIELDS 4
typedef struct {
    char label[MAX_LABEL_LEN];
    char value[MAX_VALUE_LEN];
} menu_field_t;

typedef enum
{
    test_pass = 0,
    test_fail = 1
} test_status_e;

typedef enum {
    NA,
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
    char var1[5];            // "dcb" + '\0'
    char var2[5];            // "dcb" + '\0'
    char var3[5];            // "dcb" + '\0'
    char var4[5];            // "dcb" + '\0'
    int frame_error_count;
} elevator_obj_t;

typedef struct {
    const char* key;
    const char* value;
} LookupEntry;

// static const LookupEntry system_input_map[] = {
//     {"ES", "Emergency Stop switch"},
//     {"DW", "Door open contact"},
//     {"DFC", "Door Fully Closed contact"},
//     {"GDS", "Gate Door Switch"},
//     {"EN-ABL1", "Set when EN-ABL1 and DRV-TYP1"},
//     {"SE", "Start Enable"},
//     {"1TH", "Thermal contact 1"},
//     {"2TH", "Thermal contact 2"},
//     {"2SE", "2nd Start Enable"},
//     {"TCI", "Top of Car Inspection switch"},
//     {"UIB", "Up Inspection Button"},
//     {"DIB", "Down Inspection Button"},
//     {"ERO", "Emergency Recall Operation switch"},
//     {"TDO", "Top of Car Door Open Button"},
//     {"TDC", "Top of Car Door Close Button"},
//     {"^TDO", "Rear Top of Car Door Open Button"},
//     {"^TDC", "Rear Top of Car Door Close Button"},
//     {"TCB", "Top of Car Inspection Button"},
//     {"DZ", "Door Zone"},
//     {"1LV", "Door Zone switch 1LV"},
//     {"2LV", "Door Zone switch 2LV"},
//     {"1LS", "Limit Switch 1"},
//     {"2LS", "Limit Switch 2"},
//     {"ETS", "Emergency Terminal Slow Down contact"},
//     {"BY", "BY-Relay (GECB-EN)"},
//     {"BRK", "BR-Relay (BRK-TYP = 1)"},
//     {"LWO", "Overload signal LWO"},
//     {"LWX", "Load weighing bypass LWX"},
//     {"LNS", "Load Non Stop"},
//     {"L30", "30% load in car (LW-TYP = 2)"},
//     {"L50", "50% load in car (LW-TYP = 2)"},
//     {"DOL", "Door Open Limit switch"},
//     {"DCL", "Door Close Limit switch"},
//     {"DOB", "Door Open Button"},
//     {"DCB", "Door Close Button"},
//     {"EDP", "Electronic Door Protection"},
//     {"LRD", "Light Ray Device"},
//     {"DOS", "Door Open Signal"},
//     {"GSM", "Gate Switch Monitor (EN-ABL=1, DRV-TYP=1)"},
//     {"MDD", "Moving at front of Door Detection"},
//     {"DHB", "Door Hold Button"},
//     {"SDB", "Special Door Open Button"},
//     {"SGS", "Secondary Safety Gate Shoe (IO1155)"},
//     {"WDO", "Wheel Chair Door Open Button"},
//     {"WDC", "Wheel Chair Door Close Button"},
//     {"^DOL", "Rear Door Open Limit switch"},
//     {"^DCL", "Rear Door Close Limit switch"},
//     {"^DOB", "Rear Door Open Button"},
//     {"^DCB", "Rear Door Close Button"},
//     {"^EDP", "Rear Electronic Door Protection"},
//     {"^LRD", "Rear Light Ray Device"},
//     {"^DOS", "Rear Door Open Signal"},
//     {"^GSM", "Rear Gate Switch Monitor"},
//     {"^MDD", "Moving at rear of Door Detection"},
//     {"^DHB", "Rear Door Hold Button"},
//     {"^SDB", "Rear Special Door Open Button"},
//     {"^SGS", "Secondary Rear Safety Gate Shoe"},
//     {"^WDO", "Rear Wheel Chair Door Open Button"},
//     {"^WDC", "Rear Wheel Chair Door Close Button"},
//     {"CCT", "Car Call to Top"},
//     {"CCB", "Car Call to Bottom"},
//     {"CHC", "Cut off Hall Call"},
//     {"DDO", "Disable Door Operation"},
//     {"RTB", "Remote Tripping Button"},
//     {"RRB", "Remote Resetting Button"},
//     {"EFO", "Emergency Firemen Operation"},
//     {"HTS", "Hall Temperature Sensor from SPB"},
//     {"AEF", "Alternative EFO (AEFO)"},
//     {"EFK", "Emergency Fireman Key"},
//     {"ASL", "Alternative Service Landing"},
//     {"ESK", "Emergency Service Key switch"},
//     {"ESH", "Emergency Service Hold switch"},
//     {"CFS", "Car Fireman Service switch"},
//     {"CS", "Car fireman service Start switch"},
//     {"XEF", "Override EFO"},
//     {"EFB", "Emergency Firemen Key Bypass"},
//     {"ADB", "Alternative Door Open Button"},
//     {"EDB", "EFO Door Open Button"},
//     {"1EF", "Taiwan Fireman Service Key switch 1"},
//     {"2EF", "Taiwan Fireman Service Key switch 2"},
//     {"DDS", "Disable Door Switch Relay"},
//     {"DES", "Disable EEC Relay"},
//     {"NU", "Emergency power operation signal"},
//     {"NUD", "Emergency power operation signal"},
//     {"NUG", "Emergency power operation signal"},
//     {"NRF", "NURF"},
//     {"EQ1", "Earthquake Contact Grade 1"},
//     {"EQ2", "Earthquake Contact Grade 2"},
//     {"EQS", "Earth Quake Switch"},
//     {"EQW", "Earthquake Counterweight Switch"},
//     {"EQR", "Earthquake Reset Switch"},
//     {"ISS", "Independent Service Switch"},
//     {"^ISS", "Rear Independent Service Switch"},
//     {"ISP", "Independent Service Parking switch"},
//     {"PDD", "Partition Door Device switch"},
//     {"FAN", "Fan"},
//     {"^FAN", "Rear Fan"},
//     {"HFA", "Handicapped COP Fan"},
//     {"CTL", "Car To Lobby"},
//     {"CTC", "Car To Landing Park with doors Closed switch"},
//     {"CTO", "Car To Landing Park with doors Open switch"},
//     {"PKS", "Parking Switch"},
//     {"PKG", "PKS Group switch"},
//     {"CFB", "RSL Car FeedBack"},
//     {"IST", "Intermittent Stop switch"},
//     {"ROT", "Riot Operation"},
//     {"ACC", "Anti Crime Car switch"},
//     {"ACH", "Anti Crime Hall switch"},
//     {"GSI", "Group Successive Starting In"},
//     {"COC", "Car-call cut Off (Car)"},
//     {"COH", "Car-call cut Off (Hall)"},
//     {"HCO", "Hall-call Cut Off"},
//     {"HCH", "Hall Call Cut off from Car"},
//     {"GCO", "Hall call Cut Off (Group)"},
//     {"CHC", "Cut Hall Call switch latch"},
//     {"DFD", "Disable Front Door"},
//     {"DRD", "Disable Rear Door"},
//     {"GCB", "General Control Button"},
//     {"^GCB", "Rear General Control Button"},
//     {"CRC", "Card Reader Contact"}
// };


// Global variables
int serial_fd;
char buffer[BUFFER_SIZE];
int buffer_pos;
int frame_errors;
elevator_obj_t elevator = {0};
menu_field_t fields[2];

// Function prototypes
// At top of metro.c or in metro.h
int init(int argc, char* argv[]);
void read_screen();
void parse_screen();
void sm_menu();

int setup_serial(const char* port_path, speed_t baud_rate);
void clear_screen();
void print_lcd_screen(int error_count);
int validate_frame(const char* frame);
void extract_frame_parts(const char* frame);
void principal_menu_parse();
void menu_parse();
void input_menu_parse();
void print_menu_position();
void dispatch_menu();

// Helpers to check substrings and input menu
bool contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}
bool is_input_menu(const char *top_screen) {
    char ch = top_screen[0];
    return (ch == 'A' || ch == 'B' || ch == 'C');
}

// Main logic
void dispatch_menu() {
    if (is_input_menu(elevator.top_screen)) {
        elevator.position = MENU_INPUT;
        input_menu_parse();
        return;
    }

    if (contains(elevator.top_screen, "Menu")) {
        if (contains(elevator.top_screen, "SYSTEM")) {
            elevator.position = MENU_SYSTEM;
            menu_parse();
            return;
        }
        if (contains(elevator.top_screen, "STATUS")) {
            elevator.position = MENU_STATUS;
            menu_parse();
            return;
        }
        if (contains(elevator.top_screen, "TCBC")) {
            elevator.position = MENU_TCBC;
            menu_parse();
            return;
        }
        // If "Menu" is in top_screen but none of above, fallback:
        printf("Unknown Menu Screen (Menu keyword, not SYSTEM/STATUS/TCBC)\n");
        return;
    }
    if (contains(elevator.top_screen, "TCBC"))
    {
        // If no keywords above, principal menu by default
        elevator.position = MENU_PRINCIPAL;
        principal_menu_parse();        
    }
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
        elevator.position = NA;
        read_screen();
        printf("I am here");
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
                        extract_frame_parts(frame);
                        
                        // Print to screen
                        clear_screen();
                        print_menu_position();
                        print_lcd_screen(frame_errors);
                        dispatch_menu();
                        sm_menu();
                        
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
void print_lcd_screen(int error_count) {
    // Print top border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\n");

    // Print top line content (always print top_screen)
    printf("%-*s\n", LCD_WIDTH, elevator.top_screen);

    // Print middle border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\n");

    // Print bottom line content (always print bottom_screen)
    printf("%-*s\n", LCD_WIDTH, elevator.bottom_screen);

    // Print bottom border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\n");

    // Print error count
    printf("Frame errors = %d\n", error_count);
}


void print_menu_position() {
  printf("+----------------+\n");
  switch (elevator.position) {
    case NA:
        printf("  Unknown Menu \n");    
        break;
    case MENU_PRINCIPAL:
        printf("  Principal Menu \n");    
        break;
    case MENU_TCBC:
        printf("    TCBC Menu    \n");   
        break;
    case MENU_SYSTEM:
        printf("   System Menu   \n");   
        break;
    case MENU_STATUS:
        printf("   Status Menu   \n");     
        break;
    case MENU_INPUT:
        printf("    Input Menu   \n");     
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

void extract_frame_parts(const char* frame) {
    // Extract the part after the newline and before "[H"
    const char* data_start = frame + 1;  // Skip the newline
    const char* marker_pos = strstr(data_start, "[H");
    
    if (marker_pos == NULL) {
        // If no "[H" found, return empty strings
        elevator.top_screen[0] = '\0';
        elevator.bottom_screen[0] = '\0';
        return;
    }
    
    // Calculate length of data before "[H"
    int data_length = marker_pos - data_start;
    
    // Split data into two parts
    int half_length = data_length;
    
    // Copy first half to bottom line
    if (half_length > 0) {
        strncpy(elevator.bottom_screen, data_start, half_length);
        elevator.bottom_screen[half_length] = '\0';
    } else {
        elevator.bottom_screen[0] = '\0';
    }
    
    // Copy second half to top line
    if (half_length > 0) {
        strncpy(elevator.top_screen, data_start + half_length + 2, half_length);
        elevator.top_screen[half_length] = '\0';
    } else {
        elevator.top_screen[0] = '\0';
    }

}
int parse_menu_fields(const char* src, char separator, menu_field_t* dest, int max_fields) {
    int i = 0, f = 0, len = strlen(src);

    while (i < len && f < max_fields) {
        memset(dest[f].label, 0, MAX_LABEL_LEN);
        memset(dest[f].value, 0, MAX_VALUE_LEN);

        // Skip leading spaces
        while (i < len && isspace(src[i])) i++;

        // Parse label (option number/string before separator)
        int k = 0;
        while (i < len && src[i] != separator && k < MAX_LABEL_LEN - 1) {
            dest[f].label[k++] = src[i++];
        }
        dest[f].label[k] = '\0';

        // Skip separator
        if (i < len && src[i] == separator) i++;

        // Parse value (menu name, etc.)
        k = 0;
        while (i < len && !isspace(src[i]) && k < MAX_VALUE_LEN - 1) {
            dest[f].value[k++] = src[i++];
        }
        dest[f].value[k] = '\0';

        // If both parts are empty, stop parsing (robustness)
        if (dest[f].label[0] == '\0' && dest[f].value[0] == '\0') break;

        // Skip whitespace before the next field
        while (i < len && isspace(src[i])) i++;

        f++;
    }

    return f; // Return the count of fields actually parsed
}

void menu_parse(void) {
    int field_count = parse_menu_fields(elevator.bottom_screen, '=', fields, MAX_FIELDS);
    for (int j = 0; j < field_count; j++) {
        printf("To enter %s, press %s\n", fields[j].label, fields[j].value);
    }
}

void principal_menu_parse(void) {
    int field_count;
    // Top screen parsing
    field_count = parse_menu_fields(elevator.top_screen, ':', fields, MAX_FIELDS);
    for (int j = 0; j < field_count; j++) {
        printf("To enter %s press %s\n", fields[j].value, fields[j].label);
    }
    // Bottom screen parsing
    field_count = parse_menu_fields(elevator.bottom_screen, ':', fields, MAX_FIELDS);
    for (int j = 0; j < field_count; j++) {
        printf("To enter %s press %s\n", fields[j].value, fields[j].label);
    }
}

void input_menu_parse() {
    // Top Screen Parsing


    memcpy(elevator.car_id,     elevator.top_screen + 0, 1);  elevator.car_id[1] = '\0';
    memcpy(elevator.direction,  elevator.top_screen + 1, 1);  elevator.direction[1] = '\0';
    memcpy(elevator.level,      elevator.top_screen + 2, 2);  elevator.level[2] = '\0';
    memcpy(elevator.ocss,       elevator.top_screen + 5, 3);  elevator.ocss[3] = '\0';
    memcpy(elevator.mcss,       elevator.top_screen + 9, 2);  elevator.mcss[2] = '\0';
    memcpy(elevator.door,       elevator.top_screen + 12, 2); elevator.door[2] = '\0';
    memcpy(elevator.rear_door,  elevator.top_screen + 14, 2); elevator.rear_door[2] = '\0';
    // Bottom Screen Parsing
    memcpy(elevator.var1,     elevator.bottom_screen + 0, 4);  elevator.var1[4] = '\0';
    memcpy(elevator.var2,  elevator.bottom_screen + 4, 4);  elevator.var2[4] = '\0';
    memcpy(elevator.var3,      elevator.bottom_screen + 8, 4);  elevator.var3[4] = '\0';
    memcpy(elevator.var4,       elevator.bottom_screen + 12, 4);  elevator.var4[4] = '\0';

    printf("\n+----------------+\n");
    printf(" Elevator Status \n");
    printf("+----------------+\n");

    printf(" Car ID    : %-3s \n", elevator.car_id);
    printf(" Direction : %-3s \n", elevator.direction);
    printf(" Level     : %-3s \n", elevator.level);
    printf(" OCSS      : %-3s \n", elevator.ocss);
    printf(" MCSS      : %-3s \n", elevator.mcss);
    printf(" Front Door: %-3s \n", elevator.door);
    printf(" Rear Door : %-3s \n", elevator.rear_door);
    printf(" Var 1 : %-3s \n", elevator.var1);
    printf(" Var 2 : %-3s \n", elevator.var2);
    printf(" Var 3 : %-3s \n", elevator.var3);
    printf(" Var 4 : %-3s \n", elevator.var4);
    printf("+----------------+\n");
}

static void send(const char *s) {
    printf("\n%s\n", s);   // just print 0/1/2 for now
}

// placeholder failure logic: every call succeeds for now
static int send_command(const char *s) {
    send(s);
    // TODO: implement real failure detection
    return 1; // 1 = success, 0 = fail
}

void sm_menu(void) {
    sm_menu_e currentState = elevator.position;
    sm_menu_e previousState = currentState;
    sm_menu_e nextState     = currentState;

    switch (currentState) {

    case NA:
        nextState = MENU_PRINCIPAL;
        if (!send_command("0")) {
            // failure: stay/rollback
            nextState = previousState;
        }
        break;

    case MENU_PRINCIPAL:
        nextState = MENU_TCBC;
        if (!send_command("1")) {
            nextState = previousState;
        }
        break;

    case MENU_TCBC:
        // you said: "If I am in TCBC menu send 1"
        // Assuming it goes to SYSTEM (add/change target if needed)
        nextState = MENU_SYSTEM;
        if (!send_command("1")) {
            nextState = previousState;
        }
        break;

    case MENU_SYSTEM:
        // go to STATUS with "1"
        nextState = MENU_STATUS;
        if (!send_command("1")) {
            nextState = previousState;
        }
        break;

    case MENU_STATUS:
        // go to INPUT with "2"
        nextState = MENU_INPUT;
        if (!send_command("2")) {
            nextState = previousState;
        }
        break;

    case MENU_INPUT:
        // terminal state for now, or define your own transitions
        nextState = currentState;
        break;

    default:
        nextState = currentState;
        break;
    }
    // commit transition
    elevator.position = nextState;
}

