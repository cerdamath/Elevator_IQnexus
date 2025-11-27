#include <stdint.h>
#include <stdbool.h> (Keil MDK for STM32L0 supports C99; if not, you can typedef a BOOL enum instead).
#include <string.h>
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

typedef enum {
    NA,
    MENU_PRINCIPAL,
    MENU_TCBC,
    MENU_SYSTEM,
    MENU_STATUS,
    MENU_INPUT
} sm_menu_e;

typedef union 
{
    elevator_payload;
    struct 
    {
        uint8_t
        car_id      :2,     
        dir         :2,     
        level       :4;      
        
        uint8_t
        f_door      :1,
        r_door      :1,
        ocss        :6;
        
        uint8_t
        mcss        :4,
        reserved    :4;
    
        uint8_t
        var_status  :1,
        var1        :8;
        
        uint8_t
        var_status  :1,
        var2        :8;

        uint8_t
        var_status  :1,
        var3        :8;

        uint8_t
        var_status  :1,
        var4        :8;
    };
};





//simple bitmaps?
typedef struct
{
    void (*callback)();
    char top_screen[LCD_WIDTH + 1];
    char bottom_screen[LCD_WIDTH + 1];
    sm_menu_e position;
    char car_id[2];         // "A" + '\0'
    char direction[2];      // "-" + '\0'
    char level[3];          // "01" + '\0'
    char ocss[4];           // "IDL" + '\0'
    //char mcss[3];           // "ST" + '\0'
    char front_door[3];     // "][" + '\0'
    char rear_door[3];      // "[]" + '\0'
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


static uint8_t map_car_id(const char *car_id) {
    if (car_id >= 'A' && car_id <= 'C') {
        return (uint8_t)(car_id - 'A');
    }
    return 0;
}

static uint8_t map_direction(const char *dir) {
    if (dir == 'U'){
        return 1;
    } 
    if (dir == '-'){
        return 2;
    }
    return 3;
}

static uint8_t map_level(const char *level) {
    level += 1;
    if (level == NULL) {
        return 0;
    }
    
    uint8_t value = 0;
    if (level >= '0' && level <= '9') {
        value = level - '0';
    } 
    
    else {
        return 255;
    }

    return value;
}

static bool is_door_closed(const char* door) {
    if (door[0] == ']') {
        return true;
    }
    return false;
}

static uint8_t map_ocss(const char *ocss)
{
// Simple fixed mapping: NOR=0, PRK=1, IDL=2, etc.
if (strncmp(ocss, "NOR", 3) == 0){
    return 1;
} 
if (strncmp(ocss, "IDL", 3) == 0){
    return 2;
} 
if (strncmp(ocss, "PRK", 3) == 0) {
    return 3;
}
// add more mappings as needed
return 0; // unknown
}



// Helpers to check substrings and input menu
bool contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}
bool is_input_menu(const char *top_screen) {
    char ch = top_screen[0];
    return (ch == 'A' || ch == 'B' || ch == 'C');
}

// Global state encapsulation
static metro_state_t g_state = {0};


int main(int argc, char* argv[])
{
    int status = init(argc, argv);
    
    if (status != 0) {
        return 1;
    }
    
    else {
        g_state.elevator.position = NA;
        while (1) {
            // Read serial data into buffer
            serial_read();

            // Find and process complete frames
            int frame_start = -1;
            find_frame_boundaries(&frame_start);
            if (frame_start != -1) {
                process_frame(frame_start);
                
                // Update display and menu state
                //clear_screen();
                //print_lcd_screen(g_state.frame_errors);
                //print_menu_position();
                dispatch_menu();
                //printf("Last Keyboard Input: %s", keyboard_buffer);
                //sm_menu();

            }
        }
        close(g_state.serial_fd);
        return 0;
    }
}

int init(int argc, char* argv[])
{
    // if (argc != 2) {
    //     fprintf(stderr, "Usage: %s <serial_port>\n", argv[0]);
    //     return 1;
    // }

    // const char* port_path = argv[1];
    // speed_t baud_rate = B9600;
    
    // printf("Opening device: %s\n", port_path);
    // printf("Baud: %u Port:%s", baud_rate, port_path);

    // // Setup serial port
    // g_state.serial_fd = setup_serial(port_path);
    // if (g_state.serial_fd < 0) {
    //     fprintf(stderr, "Failed to open serial port %s\n", port_path);
    //     return 1;
    // }

    // Initialize buffer and variables
    g_state.buffer_pos = 0;
    g_state.frame_errors = 0;
    
    // // Clear screen and start display
     clear_screen();
    
    // printf("Reading from %s at %ld baud\n", port_path, (long)baud_rate);
    // printf("Press Ctrl+C to exit\n");
     return 0;
}


void find_frame_boundaries(int* frame_start) {
    *frame_start = -1;
    
    // Find first newline character
    for (int i = 0; i < g_state.buffer_pos; i++) {
        if (g_state.buffer[i] == '\n') {
            *frame_start = i;
            break;
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
            return;
        }
        if (contains(g_state.elevator.top_screen, "STATUS")) {
            g_state.elevator.position = MENU_STATUS;
            return;
        }
        if (contains(g_state.elevator.top_screen, "TCBC")) {
            g_state.elevator.position = MENU_TCBC;
            return;
        }
        return;
    }
    if (contains(g_state.elevator.top_screen, "TCBC"))
    {
        // If no keywords above, principal menu by default
        g_state.elevator.position = MENU_PRINCIPAL;
    }
}

/**
 * parse_level_to_4bit - Convert "00"-"09" to 0-9 (4 bits)
 */
static uint8_t parse_level_to_4bit(const char *level)
{
    if (!level || level == '\0' || level == '\0') {
        return 0;
    }
    
    uint8_t tens = (uint8_t)(level - '0');
    uint8_t ones = (uint8_t)(level - '0');
    
    if (tens > 9 || ones > 9) {
        return 0;
    }
    
    uint8_t result = tens * 10 + ones;
    return (result > 9) ? 0 : result;
}


/**
 * parse_car_id - Convert "A"/"B"/"C" to 2-bit code
 * A=0, B=1, C=2
 */
static uint8_t parse_car_id(const char *car_id)
{
    if (!car_id || car_id == '\0') {
        return 0;
    }
    
    switch (car_id) {
        case 'A': return 0;
        case 'B': return 1;
        case 'C': return 2;
        default: return 3;  // Reserved/error
    }
}


/**
 * is_variable_uppercase - Check if variable is uppercase
 * Since variables are either all upper or all lower, check first char
 */
static bool is_variable_uppercase(const char *var_str)
{
    if (!var_str || var_str == '\0') return false;
    return isupper((unsigned char)var_str) ? true : false;
}


/**
 * build_minimal_payload - 2-byte ultra-compact payload
 * 
 * Byte 0: [Reserved(2b) | Status(2b) | RearDoor(1b) | FrontDoor(1b) | Direction(2b)]
 * Byte 1: [Reserved(4b) | Level(4b)]
 * 
 * Total: 2 bytes (ultra-compact, most efficient)
 * Airtime: ~7ms (SF7)
 * Use: Send every 30-60 seconds
 */
void build_minimal_payload(uint8_t *payload_buffer, int *payload_len)
{
    if (!payload_buffer || !payload_len) {
        return;
    }
    
    elevator_obj_t *e = &g_elevator_state.elevator;
    
    // ===== BYTE 0: Direction, Doors, Status =====
    uint8_t byte0 = 0;
    
    // Bits [0:1] - Direction (Up/Down/Stationary/Error)
    pack_bits(&byte0, 0, 2, map_direction(e->direction));
    
    // Bit  - Front Door (0=Closed, 1=Open)
    pack_bits(&byte0, 2, 1, map_door_state(e->front_door));
    
    // Bit  - Rear Door (0=Closed, 1=Open)
    pack_bits(&byte0, 3, 1, map_door_state(e->rear_door));
    
    // Bits [4:5] - Status (OutOfService/Normal/Idle/Parked)
    pack_bits(&byte0, 4, 2, map_operational_status(e->ocss));
    
    // Bits [6:7] - Reserved (leave as 0)
    
    payload_buffer = byte0;
    
    // ===== BYTE 1: Level (4 bits only) =====
    uint8_t byte1 = 0;
    uint8_t level_4bit = parse_level_to_4bit(e->level);
    
    // Bits [0:3] - Level (0-9, fits in 4 bits)
    pack_bits(&byte1, 0, 4, level_4bit);
    
    // Bits [4:7] - Reserved (leave as 0)
    
    payload_buffer = byte1;
    
    *payload_len = 2;
    
    #ifdef DEBUG_MODE
    printf("[LORA] Minimal 2-byte: 0x%02X 0x%02X (Dir=%u Doors=%u/%u Status=%u Level=%u)\r\n",
           byte0, byte1,
           map_direction(e->direction),
           map_door_state(e->door),
           map_door_state(e->rear_door),
           map_operational_status(e->ocss),
           level_4bit);
    #endif
}

/**
 * build_extended_payload - 4-byte detailed payload
 * 
 * Byte 0: [Reserved(2b) | Status(2b) | RearDoor(1b) | FrontDoor(1b) | Direction(2b)]
 * Byte 1: [Reserved(2b) | CarId(2b) | Level(4b)]
 * Byte 2: [OCSS_Lower(8b)]
 * Byte 3: [Var4(1b) | Var3(1b) | Var2(1b) | Var1(1b) | OCSS_Upper(4b)]
 * 
 * Total: 4 bytes (detailed, efficient)
 * Airtime: ~9ms (SF7)
 * Use: Send every 5-10 minutes for detailed diagnostics
 */
void build_extended_payload(uint8_t *payload_buffer, int *payload_len)
{
    if (!payload_buffer || !payload_len) {
        return;
    }
    
    elevator_obj_t *e = &g_elevator_state.elevator;
    
    // ===== BYTE 0: Same as minimal =====
    uint8_t byte0 = 0;
    pack_bits(&byte0, 0, 2, map_direction(e->direction));
    pack_bits(&byte0, 2, 1, map_door_state(e->front_door));
    pack_bits(&byte0, 3, 1, map_door_state(e->rear_door));
    pack_bits(&byte0, 4, 2, map_operational_status(e->ocss));
    payload_buffer = byte0;
    
    // ===== BYTE 1: Level (4 bits) + CarId (2 bits) =====
    uint8_t byte1 = 0;
    uint8_t level_4bit = parse_level_to_4bit(e->level);
    uint8_t car_id_2bit = parse_car_id(e->car_id);
    
    pack_bits(&byte1, 0, 4, level_4bit);      // Bits [0:3] - Level
    pack_bits(&byte1, 4, 2, car_id_2bit);     // Bits [4:5] - CarId
    // Bits [6:7] - Reserved
    
    payload_buffer = byte1;
    
    // ===== BYTE 2 & 3: OCSS Code (12 bits total = 4096 codes max, but we only use 44) =====
    uint8_t ocss_code = encode_ocss_compact(e->ocss);
    if (ocss_code == 0xFF) ocss_code = 0x00;  // Default on error
    
    // Split OCSS code across bytes 2 and 3
    // OCSS code range: 0x00-0x2B (0-43, fits in 6 bits)
    // We use 12 bits total (8 in byte2, 4 in byte3) for future expansion
    
    uint8_t byte2 = ocss_code & 0xFF;         // Lower 8 bits
    payload_buffer = byte2;
    
    // ===== BYTE 3: OCSS Upper + Variables =====
    uint8_t byte3 = 0;
    
    // Bits [0:3] - Upper 4 bits of OCSS code (for full 12-bit support)
    // Even though we only use 44 codes (0x2B max), this allows future expansion
    uint8_t ocss_upper = (ocss_code >> 8) & 0x0F;
    pack_bits(&byte3, 0, 4, ocss_upper);
    
    // Bits [4:7] - Variable uppercase status (1 bit each)
    pack_bits(&byte3, 4, 1, is_variable_uppercase(e->var1) ? 1 : 0);
    pack_bits(&byte3, 5, 1, is_variable_uppercase(e->var2) ? 1 : 0);
    pack_bits(&byte3, 6, 1, is_variable_uppercase(e->var3) ? 1 : 0);
    pack_bits(&byte3, 7, 1, is_variable_uppercase(e->var4) ? 1 : 0);
    
    payload_buffer = byte3;
    
    *payload_len = 4;
    
    #ifdef DEBUG_MODE
    printf("[LORA] Extended 4-byte: CarId=%c Level=%u OCSS=0x%02X Vars=%u%u%u%u\r\n",
           e->car_id, level_4bit, ocss_code,
           is_variable_uppercase(e->var1),
           is_variable_uppercase(e->var2),
           is_variable_uppercase(e->var3),
           is_variable_uppercase(e->var4));
    #endif
}

static void pack_bits(uint8_t *byte, uint8_t start_bit, uint8_t num_bits, uint8_t value)
{
    // Defensive bounds checking for production code
    if (start_bit + num_bits > 8) {
        #ifdef DEBUG_MODE
        printf("[ERROR] pack_bits: start_bit(%u) + num_bits(%u) > 8\r\n", 
               start_bit, num_bits);
        #endif
        return;  // Or assert() in debug builds
    }
    
    uint8_t mask = (1 << num_bits) - 1;
    uint8_t shift_val = value & mask;
    *byte |= (shift_val << start_bit);
}

