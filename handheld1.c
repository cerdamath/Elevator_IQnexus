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

#define FRAME_LENGTH 36
#define BRACKET_POS 18
#define H_POS 19

#define MOVE_BUFFER_BY 1

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

// Structure to encapsulate all global state
typedef struct {
    int serial_fd;
    char buffer[BUFFER_SIZE];
    int buffer_pos;
    int frame_errors;
    elevator_obj_t elevator;
    menu_field_t fields[2];
    bool new_line;
    bool escape;
    bool debug;
    bool h_b_bool;
    char h_pos[1];
    char b_pos[1];
} metro_state_t;

typedef struct {
    const char* key;
    const char* value;
} LookupEntry;

static const LookupEntry ocss_lookup[] = {
    {"SHO", "Shabat Operation"},
    {"WCO", "Wild Car Operation"},
    {"WCS", "Wheel Chair Service"},
    {"ACP", "Anti Crime Protection"},
    {"ANS", "Anti Nuisance Service"},
    {"ARD", "Automatic Return Device"},
    {"ATT", "ATTended service"},
    {"CBP", "Car Button Protection"},
    {"CHC", "Cut off Hall Call"},
    {"COR", "Correction Run"},
    {"CTL", "Car To Landing"},
    {"DBF", "Drive / Break Fault"},
    {"DCP", "Delayed Car Protection"},
    {"DCS", "Door Check Sequence"},
    {"DHB", "Door Hold Button mode"},
    {"DLM", "Door Lock Monitoring"},
    {"DTC", "Door Time protection Close"},
    {"DTO", "Door Time protection Open"},
    {"EFO", "Emergency Fireman's Operation"},
    {"EFS", "Emergency Fireman's Service"},
    {"EHS", "Emergency Hospital Service"},
    {"EMT", "Emergency Medical Transport"},
    {"EPC", "Emergency Power wait for Correction run"},
    {"EPR", "Emergency Power Rescue run"},
    {"EPW", "Emergency Power Wait for normal"},
    {"EQR", "Earth Quake automatic Recovery"},
    {"EQO", "Earth Quake Operation"},
    {"ESB", "Emergency Stop Button resp. J-Relay fault"},
    {"GCB", "General Control of Buttons"},
    {"HAD", "Hoistway Access Detection"},
    {"HBP", "Hall Button Protection"},
    {"IDL", "IDLe"},
    {"INI", "INItialize"},
    {"INS", "INspection"},
    {"ISC", "Independent ServICe"},
    {"LNS", "Load Non Stop service"},
    {"MIT", "Moderate Incoming Traffic"},
    {"NAV", "Not Available"},
    {"NOR", "NORmal"},
    {"OLD", "OverLoad Device"},
    {"PKS", "ParKing Switch"},
    {"PRK", "PaRKing"},
    {"REI", "Remote Elevator Inspection"},
    {"ROT", "car RIOT operation"}
};

static const LookupEntry mcss_map[] = {
    {"CR", "Correction Run"}, {"EF", "Emergency Fast Run"}, {"ES", "Emergency Stop"}, 
    {"EW", "Emergency during Wait"}, {"FR", "Fast Run"}, {"ID", "Idle"}, 
    {"IN", "Inspection Run"}, {"NR", "Not Ready"}, {"RL", "Relevel"}, 
    {"RS", "Rescue Run"}, {"SR", "Slow Run"}, {"ST", "Stop"}
};

static const LookupEntry system_input_map[] = {
    {"ES", "Emergency Stop switch"},
    {"DW", "Door open contact"},
    {"DFC", "Door Fully Closed contact"},
    {"GDS", "Gate Door Switch"},
    {"EN-ABL1", "Set when EN-ABL1 and DRV-TYP1"},
    {"SE", "Start Enable"},
    {"1TH", "Thermal contact 1"},
    {"2TH", "Thermal contact 2"},
    {"2SE", "2nd Start Enable"},
    {"TCI", "Top of Car Inspection switch"},
    {"UIB", "Up Inspection Button"},
    {"DIB", "Down Inspection Button"},
    {"ERO", "Emergency Recall Operation switch"},
    {"TDO", "Top of Car Door Open Button"},
    {"TDC", "Top of Car Door Close Button"},
    {"^TDO", "Rear Top of Car Door Open Button"},
    {"^TDC", "Rear Top of Car Door Close Button"},
    {"TCB", "Top of Car Inspection Button"},
    {"DZ", "Door Zone"},
    {"1LV", "Door Zone switch 1LV"},
    {"2LV", "Door Zone switch 2LV"},
    {"1LS", "Limit Switch 1"},
    {"2LS", "Limit Switch 2"},
    {"ETS", "Emergency Terminal Slow Down contact"},
    {"BY", "BY-Relay (GECB-EN)"},
    {"BRK", "BR-Relay (BRK-TYP = 1)"},
    {"LWO", "Overload signal LWO"},
    {"LWX", "Load weighing bypass LWX"},
    {"LNS", "Load Non Stop"},
    {"L30", "30% load in car (LW-TYP = 2)"},
    {"L50", "50% load in car (LW-TYP = 2)"},
    {"DOL", "Door Open Limit switch"},
    {"DCL", "Door Close Limit switch"},
    {"DOB", "Door Open Button"},
    {"DCB", "Door Close Button"},
    {"EDP", "Electronic Door Protection"},
    {"LRD", "Light Ray Device"},
    {"DOS", "Door Open Signal"},
    {"GSM", "Gate Switch Monitor (EN-ABL=1, DRV-TYP=1)"},
    {"MDD", "Moving at front of Door Detection"},
    {"DHB", "Door Hold Button"},
    {"SDB", "Special Door Open Button"},
    {"SGS", "Secondary Safety Gate Shoe (IO1155)"},
    {"WDO", "Wheel Chair Door Open Button"},
    {"WDC", "Wheel Chair Door Close Button"},
    {"^DOL", "Rear Door Open Limit switch"},
    {"^DCL", "Rear Door Close Limit switch"},
    {"^DOB", "Rear Door Open Button"},
    {"^DCB", "Rear Door Close Button"},
    {"^EDP", "Rear Electronic Door Protection"},
    {"^LRD", "Rear Light Ray Device"},
    {"^DOS", "Rear Door Open Signal"},
    {"^GSM", "Rear Gate Switch Monitor"},
    {"^MDD", "Moving at rear of Door Detection"},
    {"^DHB", "Rear Door Hold Button"},
    {"^SDB", "Rear Special Door Open Button"},
    {"^SGS", "Secondary Rear Safety Gate Shoe"},
    {"^WDO", "Rear Wheel Chair Door Open Button"},
    {"^WDC", "Rear Wheel Chair Door Close Button"},
    {"CCT", "Car Call to Top"},
    {"CCB", "Car Call to Bottom"},
    {"CHC", "Cut off Hall Call"},
    {"DDO", "Disable Door Operation"},
    {"RTB", "Remote Tripping Button"},
    {"RRB", "Remote Resetting Button"},
    {"EFO", "Emergency Firemen Operation"},
    {"HTS", "Hall Temperature Sensor from SPB"},
    {"AEF", "Alternative EFO (AEFO)"},
    {"EFK", "Emergency Fireman Key"},
    {"ASL", "Alternative Service Landing"},
    {"ESK", "Emergency Service Key switch"},
    {"ESH", "Emergency Service Hold switch"},
    {"CFS", "Car Fireman Service switch"},
    {"CS", "Car fireman service Start switch"},
    {"XEF", "Override EFO"},
    {"EFB", "Emergency Firemen Key Bypass"},
    {"ADB", "Alternative Door Open Button"},
    {"EDB", "EFO Door Open Button"},
    {"1EF", "Taiwan Fireman Service Key switch 1"},
    {"2EF", "Taiwan Fireman Service Key switch 2"},
    {"DDS", "Disable Door Switch Relay"},
    {"DES", "Disable EEC Relay"},
    {"NU", "Emergency power operation signal"},
    {"NUD", "Emergency power operation signal"},
    {"NUG", "Emergency power operation signal"},
    {"NRF", "NURF"},
    {"EQ1", "Earthquake Contact Grade 1"},
    {"EQ2", "Earthquake Contact Grade 2"},
    {"EQS", "Earth Quake Switch"},
    {"EQW", "Earthquake Counterweight Switch"},
    {"EQR", "Earthquake Reset Switch"},
    {"ISS", "Independent Service Switch"},
    {"^ISS", "Rear Independent Service Switch"},
    {"ISP", "Independent Service Parking switch"},
    {"PDD", "Partition Door Device switch"},
    {"FAN", "Fan"},
    {"^FAN", "Rear Fan"},
    {"HFA", "Handicapped COP Fan"},
    {"CTL", "Car To Lobby"},
    {"CTC", "Car To Landing Park with doors Closed switch"},
    {"CTO", "Car To Landing Park with doors Open switch"},
    {"PKS", "Parking Switch"},
    {"PKG", "PKS Group switch"},
    {"CFB", "RSL Car FeedBack"},
    {"IST", "Intermittent Stop switch"},
    {"ROT", "Riot Operation"},
    {"ACC", "Anti Crime Car switch"},
    {"ACH", "Anti Crime Hall switch"},
    {"GSI", "Group Successive Starting In"},
    {"COC", "Car-call cut Off (Car)"},
    {"COH", "Car-call cut Off (Hall)"},
    {"HCO", "Hall-call Cut Off"},
    {"HCH", "Hall Call Cut off from Car"},
    {"GCO", "Hall call Cut Off (Group)"},
    {"CHC", "Cut Hall Call switch latch"},
    {"DFD", "Disable Front Door"},
    {"DRD", "Disable Rear Door"},
    {"GCB", "General Control Button"},
    {"^GCB", "Rear General Control Button"},
    {"CRC", "Card Reader Contact"}
};



// Global state encapsulation
static metro_state_t g_state = {0};

// Function prototypes
// At top of metro.c or in metro.h
int init(int argc, char* argv[]);
void serial_read(void);
void find_frame_boundaries(int* frame_start);
void process_frame(int frame_start);
void buffer_shift(int shift_amount);
void sm_menu(void);

int setup_serial(const char* port_path, int baud_rate, int databits, int parity, int stopbits);
void enable_raw_mode(void);
void disable_raw_mode(void);
void clear_screen();
void print_lcd_screen(int error_count);
int validate_frame(const char* frame);
void extract_frame_parts(const char* frame);
void principal_menu_parse();
void menu_parse();
void input_menu_parse();
void print_menu_position();
void dispatch_menu();
void debug_message(void);

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
    if (is_input_menu(g_state.elevator.top_screen)) {
        g_state.elevator.position = MENU_INPUT;
        input_menu_parse();
        return;
    }

    if (contains(g_state.elevator.top_screen, "Menu")) {
        if (contains(g_state.elevator.top_screen, "SYSTEM")) {
            g_state.elevator.position = MENU_SYSTEM;
            menu_parse();
            return;
        }
        if (contains(g_state.elevator.top_screen, "STATUS")) {
            g_state.elevator.position = MENU_STATUS;
            menu_parse();
            return;
        }
        if (contains(g_state.elevator.top_screen, "TCBC")) {
            g_state.elevator.position = MENU_TCBC;
            menu_parse();
            return;
        }
        // If "Menu" is in top_screen but none of above, fallback:
        printf("Unknown Menu Screen (Menu keyword, not SYSTEM/STATUS/TCBC)\n");
        return;
    }
    if (contains(g_state.elevator.top_screen, "TCBC"))
    {
        // If no keywords above, principal menu by default
        g_state.elevator.position = MENU_PRINCIPAL;
        principal_menu_parse();
    }
    else {
        g_state.elevator.position = NA;
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


static speed_t baud_to_constant(int baud)
{
    switch (baud) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
#ifdef B57600
        case 57600: return B57600;
#endif
#ifdef B115200
        case 115200: return B115200;
#endif
#ifdef B230400
        case 230400: return B230400;
#endif
        default:
            return 0;   // invalid
    }
}


int main(int argc, char* argv[])
{
    int status = init(argc, argv);
    
    if (status != 0) {
        return 1;
    }
    
    else {
        g_state.elevator.position = NA;
        while (1) {
            // Read serial data into b
            serial_read();
            // Find and process complete frames
            int frame_start = -1;
            find_frame_boundaries(&frame_start);
            if (frame_start != -1) {
                process_frame(frame_start);
                
                // Update display and menu state
                clear_screen();
                print_lcd_screen(g_state.frame_errors);
                print_menu_position();
                dispatch_menu();
                if(g_state.debug){
                    debug_message();
                }
            }
        }
        disable_raw_mode();
        close(g_state.serial_fd);
        return 0;
    }
}

int init(int argc, char* argv[])
{
     if (argc < 6 || argc > 7) {
        fprintf(stderr,
                "Usage: %s <serial_port> <baud_rate> <databits> <parity> <stop_bits> optional[<D>]\n",
                argv[0]);
        return 1;
    }

    const char* port_path = argv[1];
    int baud_rate = atoi(argv[2]);
    int databits  = atoi(argv[3]);
    int parity    = atoi(argv[4]);   // 0 = none, 1 = odd, 2 = even
    int stop_bits = atoi(argv[5]);   // 1 or 2
    if (argc > 6 && strcmp(argv[6], "D") == 0) {
        g_state.debug = true;
    } 

    printf("Opening device: %s\n", port_path);
    printf("Port: %s Baud: %d Databits: %d Parity: %d Stop_bits: %d ",port_path, baud_rate, databits, parity, stop_bits);

    // Setup serial port
    g_state.serial_fd = setup_serial(port_path, baud_rate, databits, parity, stop_bits);
    if (g_state.serial_fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", port_path);
        return 1;
    }
    
    enable_raw_mode();

    // Initialize buffer and variables
    g_state.buffer_pos = 0;
    g_state.frame_errors = 0;
    
    // Clear screen and start display
    clear_screen();
    
    printf("Reading from %s at %ld baud\n", port_path, (long)baud_rate);
    printf("Press Ctrl+C to exit\n");
    return 0;
}

void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    term.c_cc[VMIN] = 0;               // Non-blocking read
    term.c_cc[VTIME] = 0;              // No timeout
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);   // Re-enable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}


void serial_read(void) {
    // Wait for data to be available
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(g_state.serial_fd, &readfds);
    FD_SET(STDIN_FILENO, &readfds);

    int max_fd = (g_state.serial_fd > STDIN_FILENO ? g_state.serial_fd : STDIN_FILENO) + 1;
    
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(max_fd, &readfds, NULL, NULL, &timeout); // Wait for input on any FDs in the set, up to 1s.

    
    if (activity < 0) {
        perror("select error");
        return;
    }
    
    if (activity == 0) {
        // Timeout, continue loop
        return;
    }

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
        char single_char;
        ssize_t read_count = read(STDIN_FILENO, &single_char, 1);

        if (read_count > 0) {
            // Send immediately to serial port
            ssize_t written = write(g_state.serial_fd, &single_char, 1);
            if (written < 1) {
                printf("Error writing to serial port\n");
                fflush(stdout);
            }
            // Echo the character locally (optional)
            printf("%c", single_char);
            fflush(stdout);
        } else if (read_count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read from stdin");
        }
    }
    
    if (FD_ISSET(g_state.serial_fd, &readfds)) {
        // Read data from serial port
        char byte;
        int bytes_read = read(g_state.serial_fd, &byte, 1);
        
        if (bytes_read < 0) {
            perror("read error");
            return;
        }
        
        if (bytes_read == 0) {
            // No data available
            return;
        }
        
        // Add byte to buffer
        g_state.buffer[g_state.buffer_pos] = byte;
        g_state.buffer_pos++;
        
        // If buffer is full, reset it
        if (g_state.buffer_pos >= BUFFER_SIZE) {
            g_state.buffer_pos = 0;
        }
    }
}

void find_frame_boundaries(int* frame_start) {
    *frame_start = -1;
    
    // Find first newline character
    for (int i = 0; i < g_state.buffer_pos; i++) {
        if (g_state.buffer[i] == '\n' && g_state.buffer[i + 1] == '\n') {
            *frame_start = i;
            g_state.new_line = true;
            break;
        }
        if (g_state.buffer[i] == '\n') {
            *frame_start = i;
            break;
        }
    }

    for (int i = 0; i < g_state.buffer_pos; i++) {
        if (g_state.buffer[i] == '\x1B') {
            g_state.escape = true;
            g_state.h_b_bool = true;
        }
    }
    
    // If we found a newline, check if we have enough data for a complete frame
    if (*frame_start != -1) {
        if (*frame_start + FRAME_LENGTH <= g_state.buffer_pos) {
            // Complete frame found
            return;
        }
    }
    
    // Incomplete frame or no frame start found
    *frame_start = -1;
}


void debug_message() {
    g_state.new_line ? printf("New line chars : 2\nIncrease MOVE_BUFFER_BY\n") : printf("New line chars : 1\n");
    g_state.escape ? printf("Escape Character Found\n") : 0;
    g_state.h_b_bool ? printf("BRACKET_CHAR: %s \nH_CHAR: %s\n", g_state.b_pos, g_state.h_pos) : printf("Found '[H'\n");
}

void process_frame(int frame_start) {
    // Extract potential frame
    char frame[FRAME_LENGTH + 1];
    memcpy(frame, g_state.buffer + frame_start, FRAME_LENGTH);
    frame[FRAME_LENGTH] = '\0';

    // Validate frame
    if (validate_frame(frame)) {
        // Extract parts for LCD display
        extract_frame_parts(frame);
        
        // Shift buffer to remove processed data
        int shift_amount = frame_start + FRAME_LENGTH;
        buffer_shift(shift_amount);
    } else {
        // Frame validation failed
        g_state.frame_errors++;
        
        // Find next potential frame start
        int next_newline = -1;
        for (int i = frame_start + 1; i < g_state.buffer_pos; i++) {
            if (g_state.buffer[i] == '\n') {
                next_newline = i;
                break;
            }
        }
        
        // If we found a next newline, shift buffer to that position
        if (next_newline != -1) {
            buffer_shift(next_newline);
        } else {
            // No next newline found, clear buffer
            g_state.buffer_pos = 0;
        }
    }
}

void buffer_shift(int shift_amount) {
    memmove(g_state.buffer, g_state.buffer + shift_amount, g_state.buffer_pos - shift_amount);
    g_state.buffer_pos -= shift_amount;
}


int setup_serial(const char* port_path, int baud_rate, int databits, int parity, int stopbits) {
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

    // Map baud int -> termios speed_t
    speed_t speed = baud_to_constant(baud_rate);
    if (speed == 0) {
        fprintf(stderr, "Unsupported baud rate: %d\n", baud_rate);
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag |= (CLOCAL | CREAD);

    // Data bits
    tty.c_cflag &= ~CSIZE;
    switch(databits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8:
        default:
            tty.c_cflag |= CS8; break;
    }

    // Parity: 0 = none, 1 = odd, 2 = even
    tty.c_cflag &= ~(PARENB | PARODD);
    if (parity == 1) {
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
    } else if (parity == 2) {
        tty.c_cflag |= PARENB;
        // PARODD already cleared above => even
    }

    // Stop bits
    if (stopbits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    // Raw-ish mode
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK);
    tty.c_oflag &= ~OPOST;

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
void print_lcd_screen(int error_count) {
    // Print top border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\n");

    // Print top line content (always print top_screen)
    printf("%-*s\n", LCD_WIDTH, g_state.elevator.top_screen);

    // Print middle border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\n");

    // Print bottom line content (always print bottom_screen)
    printf("%-*s\n", LCD_WIDTH, g_state.elevator.bottom_screen);

    // Print bottom border
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\n");

    // Print error count
    printf("Frame errors = %d\n", error_count);

}


void print_menu_position() {
  switch (g_state.elevator.position) {
    case NA:
        printf("Unknown Menu\n");
        break;
    case MENU_PRINCIPAL:
        printf("Principal Menu\n");
        break;
    case MENU_TCBC:
        printf("TCBC Menu\n");
        break;
    case MENU_SYSTEM:
        printf("System Menu\n");
        break;
    case MENU_STATUS:
        printf("Status Menu\n");
        break;
    case MENU_INPUT:
        printf("Input Menu\n");
        break;
    }
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
    if (frame[BRACKET_POS] != '[' || frame[H_POS] != 'H') {
        g_state.h_pos[0] = frame[H_POS];
        g_state.h_pos[1] = '\0';
        g_state.b_pos[0] = frame[BRACKET_POS];
        g_state.b_pos[1] = '\0';
        return 0;
    }
    return 1;
}

void extract_frame_parts(const char* frame) {
    // Extract the part after the newline and before "[H"
    const char* top_frame = frame + MOVE_BUFFER_BY;  // Skip the newline
    const char* bottom_frame = strstr(top_frame, "[H");
    
    if (bottom_frame == NULL) {
        // If no "[H" found, return empty strings
        g_state.elevator.top_screen[0] = '\0';
        g_state.elevator.bottom_screen[0] = '\0';
        return;
    }

    // Split data into two parts
    int half_length = LCD_WIDTH;
    
    // Copy first half to bottom line
    if (half_length > 0) {
        memcpy(g_state.elevator.bottom_screen, top_frame, half_length);
        g_state.elevator.bottom_screen[half_length] = '\0';
    } else {
        g_state.elevator.bottom_screen[0] = '\0';
    }
    
    // Copy second half to top line
    if (half_length > 0) {
        memcpy(g_state.elevator.top_screen, bottom_frame + 2, half_length);
        g_state.elevator.top_screen[half_length] = '\0';
    } else {
        g_state.elevator.top_screen[0] = '\0';
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
    int field_count = parse_menu_fields(g_state.elevator.bottom_screen, '=', g_state.fields, MAX_FIELDS);
    for (int j = 0; j < field_count; j++) {
        printf("To enter %s, press %s\n", g_state.fields[j].label, g_state.fields[j].value);
    }
}

void principal_menu_parse(void) {
    int field_count;
    // Top screen parsing
    field_count = parse_menu_fields(g_state.elevator.top_screen, ':', g_state.fields, MAX_FIELDS);
    for (int j = 0; j < field_count; j++) {
        printf("To enter %s press %s\n", g_state.fields[j].value, g_state.fields[j].label);
    }
    // Bottom screen parsing
    field_count = parse_menu_fields(g_state.elevator.bottom_screen, ':', g_state.fields, MAX_FIELDS);
    for (int j = 0; j < field_count; j++) {
        printf("To enter %s press %s\n", g_state.fields[j].value, g_state.fields[j].label);
    }
}

void input_menu_parse() {
    // Top Screen Parsing

    memcpy(g_state.elevator.car_id,     g_state.elevator.top_screen + 0, 1);  g_state.elevator.car_id[1] = '\0';
    memcpy(g_state.elevator.direction,  g_state.elevator.top_screen + 1, 1);  g_state.elevator.direction[1] = '\0';
    memcpy(g_state.elevator.level,      g_state.elevator.top_screen + 2, 2);  g_state.elevator.level[2] = '\0';
    memcpy(g_state.elevator.ocss,       g_state.elevator.top_screen + 5, 3);  g_state.elevator.ocss[3] = '\0';
    memcpy(g_state.elevator.mcss,       g_state.elevator.top_screen + 9, 2);  g_state.elevator.mcss[2] = '\0';
    memcpy(g_state.elevator.door,       g_state.elevator.top_screen + 12, 2); g_state.elevator.door[2] = '\0';
    memcpy(g_state.elevator.rear_door,  g_state.elevator.top_screen + 14, 2); g_state.elevator.rear_door[2] = '\0';
    // Bottom Screen Parsing
    memcpy(g_state.elevator.var1,     g_state.elevator.bottom_screen + 0, 4);  g_state.elevator.var1[4] = '\0';
    memcpy(g_state.elevator.var2,  g_state.elevator.bottom_screen + 4, 4);  g_state.elevator.var2[4] = '\0';
    memcpy(g_state.elevator.var3,      g_state.elevator.bottom_screen + 8, 4);  g_state.elevator.var3[4] = '\0';
    memcpy(g_state.elevator.var4,       g_state.elevator.bottom_screen + 12, 4);  g_state.elevator.var4[4] = '\0';

    printf("\n+----------------+\n");
    printf(" Elevator Status \n");
    printf("+----------------+\n");

    printf(" Car ID    : %-3s \n", g_state.elevator.car_id);
    printf(" Direction : %-3s \n", g_state.elevator.direction);
    printf(" Level     : %-3s \n", g_state.elevator.level);
    printf(" OCSS      : %-3s \n", g_state.elevator.ocss);
    printf(" MCSS      : %-3s \n", g_state.elevator.mcss);
    printf(" Front Door: %-3s \n", g_state.elevator.door);
    printf(" Rear Door : %-3s \n", g_state.elevator.rear_door);
    printf(" Var 1 : %-3s \n", g_state.elevator.var1);
    printf(" Var 2 : %-3s \n", g_state.elevator.var2);
    printf(" Var 3 : %-3s \n", g_state.elevator.var3);
    printf(" Var 4 : %-3s \n", g_state.elevator.var4);
    printf("+----------------+\n");
}