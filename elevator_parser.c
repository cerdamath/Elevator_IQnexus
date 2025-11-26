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
    char car_id[2];         // "A" + '\0'
    char direction[2];      // "-" + '\0'
    char level[3];          // "01" + '\0'
    char ocss[4];           // "IDL" + '\0'
    char mcss[3];           // "ST" + '\0'
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

static uint8_t map_level(const char *level_str) {
    if (level_str == NULL) {
        return 0;
    }
    
    int value = 0;
    // first digit
    if (level_str >= '0' && s <= '9') {
        value = s - '0';
    } else {
        return 0;
    }

    // optional second digit
    if (s >= '0' && s <= '9') {[1]
        value = value * 10 + (s - '0');[1]
    }

    // clamp to 0..255 for uint8_t
    if (value < 0)   value = 0;
    if (value > 255) value = 255;

    return (uint8_t)value;
}

static bool is_door_closed(const char* door) {
    if (door[0] == ']') {
        return true;
    }
    return false;
}

static uint8_t map_ocss(const char *ocss)
{
// Simple fixed mapping: IDL=0, INS=1, NOR=2, etc.
if (strncmp(ocss, "IDL", 3) == 0) return 0;
if (strncmp(ocss, "INS", 3) == 0) return 1;
if (strncmp(ocss, "NOR", 3) == 0) return 2;
// add more mappings as needed
return 255; // unknown
}

static uint8_t map_mcss(const char *mcss)
{
if (strncmp(mcss, "ST", 2) == 0) return 0;
if (strncmp(mcss, "FR", 2) == 0) return 1;
if (strncmp(mcss, "SR", 2) == 0) return 2;
if (strncmp(mcss, "ID", 2) == 0) return 3;

return 255;
}