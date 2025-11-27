#ifndef ELEVATOR_PARSER_STM32_H
#define ELEVATOR_PARSER_STM32_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// STM32 Generated Headers (from STM32CubeIDE)
#include "main.h"
#include "usart.h"

/* ============================================
   CONFIGURATION CONSTANTS
   ============================================ */
#define FRAME_LENGTH        35
#define RX_BUFFER_SIZE      256      // Ring buffer size
#define LCD_WIDTH           16
#define MAX_LABEL_LEN       9
#define MAX_VALUE_LEN       6
#define MAX_FIELDS          4

/* ============================================
   ENUMERATIONS
   ============================================ */
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1
} test_status_e;

typedef enum {
    NA,
    MENU_PRINCIPAL,
    MENU_TCBC,
    MENU_SYSTEM,
    MENU_STATUS,
    MENU_INPUT
} sm_menu_e;

/* ============================================
   DATA STRUCTURES
   ============================================ */

typedef struct {
    char label[MAX_LABEL_LEN];
    char value[MAX_VALUE_LEN];
} menu_field_t;

typedef struct {
    char top_screen[LCD_WIDTH + 1];
    char bottom_screen[LCD_WIDTH + 1];
    char car_id[2];         // "A" + '\0'
    char direction[2];      // "-" + '\0' or "U"/"D"
    char level[3];          // "01" + '\0'
    char ocss[4];           // "IDL" + '\0'
    char mcss[3];           // "ST" + '\0'
    char door[3];           // "][" or "[]" + '\0'
    char rear_door[3];      // "[]" + '\0'
    char var1[5];           // Variable data
    char var2[5];
    char var3[5];
    char var4[5];
    uint16_t frame_error_count;
    sm_menu_e position;
} elevator_obj_t;

/**
 * Ring Buffer Structure
 * 
 * UART ISR fills bytes starting at head pointer.
 * Main loop reads from tail pointer.
 * Prevents data loss and decouples ISR from processing.
 * 
 * ISR:      head → buffer[0,1,2,3,...]
 * Main:           ← tail
 * 
 * Invariant: (head - tail) % SIZE = count
 */
typedef struct {
    uint8_t buffer[RX_BUFFER_SIZE];
    uint16_t head;          // ISR writes here (ISR increments)
    uint16_t tail;          // Main loop reads here (main increments)
    uint16_t count;         // Number of bytes available
} ring_buffer_t;

/**
 * Global Elevator State
 * 
 * This structure encapsulates all mutable state for the elevator parser.
 * All fields are accessed safely from main loop and ISR context.
 * 
 * WARNING: The ring_buffer is accessed from both ISR and main context.
 * In a real design with multiple ISRs, this would need atomic operations
 * or mutex protection. For single UART ISR (typical embedded), it's safe.
 */
typedef struct {
    ring_buffer_t rx_buffer;        // Circular buffer for UART RX data
    elevator_obj_t elevator;        // Parsed elevator state
    menu_field_t fields[MAX_FIELDS];
    uint16_t frame_errors;          // Count of malformed frames
    bool frame_ready;               // Flag: complete frame in buffer
} elevator_state_t;

/* ============================================
   GLOBAL STATE
   ============================================ */
extern elevator_state_t g_elevator_state;

// Single-byte storage for HAL UART interrupt mode
extern uint8_t huart_rx_byte;

/* ============================================
   PUBLIC API
   ============================================ */

/**
 * Initialize the elevator parser and UART
 * 
 * Call from main() after HAL_Init() and all system initialization.
 * Sets up ring buffer pointers and starts UART receive interrupt.
 */
void elevator_parser_init(void);

/**
 * Process any available frames in the ring buffer
 * 
 * Call repeatedly from main loop (or timer interrupt).
 * Non-blocking; returns immediately if no complete frame available.
 * 
 * Does:
 *   1. Check if frame_ready flag is set
 *   2. Extract frame from ring buffer
 *   3. Validate frame structure
 *   4. Parse elevator parameters
 *   5. Dispatch menu logic
 *   6. Clear frame_ready flag
 * 
 * Returns: nothing; updates g_elevator_state
 */
void elevator_parser_process(void);

/**
 * UART ISR handler (called from interrupt context)
 * 
 * Invoked by HAL when a byte is received on UART.
 * Pushes byte into ring buffer; sets frame_ready flag if '\n' detected.
 * Must execute in < 100 µs for real-time safety.
 * 
 * Args:
 *   byte: single received byte
 * 
 * Returns: nothing
 * 
 * Thread-safe: yes (single ISR only)
 * Context: ISR
 */
void uart_rx_isr_handler(uint8_t byte);

/**
 * Validate frame structure
 * 
 * Checks:
 *   - Frame is exactly FRAME_LENGTH bytes
 *   - Starts with '\n'
 *   - Contains "[H" at positions 17-18
 * 
 * Args:
 *   frame: null-terminated string of FRAME_LENGTH bytes
 * 
 * Returns:
 *   true if frame is valid, false otherwise
 */
bool validate_frame(const char* frame);

/**
 * Extract elevator parameters from valid frame
 * 
 * Parses LCD display data into top_screen and bottom_screen fields.
 * Data is split into two halves at the "[H" marker.
 * 
 * Args:
 *   frame: validated frame (FRAME_LENGTH bytes)
 * 
 * Returns: nothing; updates g_elevator_state.elevator
 */
void extract_frame_parts(const char* frame);

/**
 * Main menu dispatch logic
 * 
 * Determines which menu screen is displayed and parses accordingly.
 * Sets g_elevator_state.elevator.position to indicate current menu.
 * 
 * If position == MENU_INPUT, data is ready for LoRaWAN payload construction.
 * 
 * Returns: nothing; updates g_elevator_state
 */
void dispatch_menu(void);

/* ============================================
   OPTIONAL: DEBUG/LOGGING (compile with DEBUG_MODE)
   ============================================ */
#ifdef DEBUG_MODE

/**
 * Print parsed elevator status to UART (debug mode only)
 * 
 * Outputs:
 *   Car ID, Direction, Level, OCSS, MCSS, Door status, Variables, Error count
 * 
 * Requires: printf() redirected to UART (see implementation notes)
 */
void print_elevator_status(void);

/**
 * Print LCD screen simulation
 * 
 * Shows top_screen and bottom_screen as 16-char bordered display.
 */
void print_lcd_screen(void);

#endif

/* ============================================
   LOOKUP TABLES
   ============================================ */

/**
 * Look up full name of OCSS (Operation Control SubSystem) code
 * 
 * Args:
 *   code: 3-character OCSS code (e.g., "IDL", "NOR", "SHO")
 * 
 * Returns:
 *   Pointer to description string (static), or "UNKNOWN" if not found
 */
const char* lookup_ocss(const char* code);

/**
 * Look up full name of MCSS (Maintenance Control SubSystem) code
 * 
 * Args:
 *   code: 2-character MCSS code (e.g., "ST", "ID", "NR")
 * 
 * Returns:
 *   Pointer to description string (static), or "UNKNOWN" if not found
 */
const char* lookup_mcss(const char* code);

/* ============================================
   PAYLOAD CONSTRUCTION (TODO: Complete in application)
   ============================================ */

/**
 * Build LoRaWAN payload from parsed elevator state
 * 
 * Convert elevator_obj_t into binary LoRaWAN packet format.
 * 
 * TODO: Define your payload structure and populate it here.
 * 
 * Example:
 *   - uint8_t car_id (0-2 for A/B/C)
 *   - uint8_t direction (0=down, 1=up)
 *   - uint16_t level (floor number)
 *   - uint8_t ocss/mcss codes (3+2 bytes)
 *   - uint8_t door_status (bitmask)
 *   - uint16_t error_count
 * 
 * Args:
 *   payload_buffer: output buffer (must be large enough)
 *   payload_len: pointer to int; set to actual length used
 * 
 * Returns: nothing
 */
void build_lorawan_payload(uint8_t *payload_buffer, int *payload_len);

/**
 * Send LoRaWAN frame to module
 * 
 * Transmit constructed payload over LoRa interface.
 * 
 * TODO: Implement based on your LoRa module API (e.g., RN2483, SX1276).
 * 
 * Args:
 *   payload: binary data buffer
 *   length: payload size in bytes
 * 
 * Returns: nothing
 */
void send_lorawan_frame(const uint8_t *payload, int length);

#endif // ELEVATOR_PARSER_STM32_H