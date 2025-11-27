/**
 * elevator_parser.c - STM32 L0 Implementation
 * 
 * Parses serial data from elevator control system and constructs LoRaWAN payloads.
 * Architecture: Interrupt-driven UART RX with ring buffer; non-blocking frame processing.
 * 
 * Key Design Decisions:
 * 
 * 1. Ring Buffer Decoupling
 *    - ISR pushes raw bytes into ring buffer (fast, deterministic)
 *    - Main loop extracts complete frames (can take time)
 *    - Prevents frame loss due to processing delays
 * 
 * 2. No Dynamic Memory
 *    - All structures are static (allocated at compile time)
 *    - No malloc/free; predictable memory usage
 *    - Safe for STM32 L0 (~8KB RAM)
 * 
 * 3. Minimal ISR Work
 *    - ISR: append byte to ring buffer, set flag (~10µs)
 *    - Main: process frames, build payloads (~ms timescale)
 *    - Prevents ISR from blocking on slow operations
 * 
 * 4. Non-blocking Main Loop
 *    - No select(), no blocking read() calls
 *    - Main loop can check other peripherals (SPI for LoRa, GPIO for watchdog, etc.)
 *    - Responsive to multiple tasks
 */

#include "elevator_parser.h"
#include <ctype.h>

/* ============================================
   GLOBAL STATE
   ============================================ */
elevator_state_t g_elevator_state = {0};
uint8_t huart_rx_byte = 0;

/* ============================================
   COMPLETE OCSS ENCODING (All 44 codes)
   ============================================ */

/**
 * ALL 44 OCSS operational modes mapped to 1-byte codes (0x00-0x2B)
 */
static const struct {
    const char* name;
    uint8_t code;
} ocss_encode_map[] = {
    {"SHO", 0x00}, {"WCO", 0x01}, {"WCS", 0x02}, {"ACP", 0x03}, {"ANS", 0x04},
    {"ARD", 0x05}, {"ATT", 0x06}, {"CBP", 0x07}, {"CHC", 0x08}, {"COR", 0x09},
    {"CTL", 0x0A}, {"DBF", 0x0B}, {"DCP", 0x0C}, {"DCS", 0x0D}, {"DHB", 0x0E},
    {"DLM", 0x0F}, {"DTC", 0x10}, {"DTO", 0x11}, {"EFO", 0x12}, {"EFS", 0x13},
    {"EHS", 0x14}, {"EMT", 0x15}, {"EPC", 0x16}, {"EPR", 0x17}, {"EPW", 0x18},
    {"EQR", 0x19}, {"EQO", 0x1A}, {"ESB", 0x1B}, {"GCB", 0x1C}, {"HAD", 0x1D},
    {"HBP", 0x1E}, {"IDL", 0x1F}, {"INI", 0x20}, {"INS", 0x21}, {"ISC", 0x22},
    {"LNS", 0x23}, {"MIT", 0x24}, {"NAV", 0x25}, {"NOR", 0x26}, {"OLD", 0x27},
    {"PKS", 0x28}, {"PRK", 0x29}, {"REI", 0x2A}, {"ROT", 0x2B},
    {NULL, 0xFF}
};

/**
 * Decode 44 OCSS codes back to string names
 * Index matches code value: ocss_decode_map[0x00] = "SHO", etc.
 */
static const char* ocss_decode_map[] = {
    "SHO", "WCO", "WCS", "ACP", "ANS", "ARD", "ATT", "CBP", "CHC", "COR",
    "CTL", "DBF", "DCP", "DCS", "DHB", "DLM", "DTC", "DTO", "EFO", "EFS",
    "EHS", "EMT", "EPC", "EPR", "EPW", "EQR", "EQO", "ESB", "GCB", "HAD",
    "HBP", "IDL", "INI", "INS", "ISC", "LNS", "MIT", "NAV", "NOR", "OLD",
    "PKS", "PRK", "REI", "ROT"
};

/**
 * OCSS Long descriptions (optional, for reference/logging)
 */
static const char* ocss_long_names[] = {
    "Shabat Operation",
    "Wild Car Operation",
    "Wheel Chair Service",
    "Anti Crime Protection",
    "Anti Nuisance Service",
    "Automatic Return Device",
    "ATTended service",
    "Car Button Protection",
    "Cut off Hall Call",
    "Correction Run",
    "Car To Landing",
    "Drive / Break Fault",
    "Delayed Car Protection",
    "Door Check Sequence",
    "Door Hold Button mode",
    "Door Lock Monitoring",
    "Door Time protection Close",
    "Door Time protection Open",
    "Emergency Fireman's Operation",
    "Emergency Fireman's Service",
    "Emergency Hospital Service",
    "Emergency Medical Transport",
    "Emergency Power wait for Correction run",
    "Emergency Power Rescue run",
    "Emergency Power Wait for normal",
    "Earth Quake automatic Recovery",
    "Earth Quake Operation",
    "Emergency Stop Button resp. J-Relay fault",
    "General Control of Buttons",
    "Hoistway Access Detection",
    "Hall Button Protection",
    "IDLe",
    "INItialize",
    "INspection",
    "Independent ServICe",
    "Load Non Stop service",
    "Moderate Incoming Traffic",
    "Not Available",
    "NORmal",
    "OverLoad Device",
    "ParKing Switch",
    "PaRKing",
    "Remote Elevator Inspection",
    "car RIOT operation"
};

/* ============================================
   COMPLETE MCSS ENCODING (12 codes)
   ============================================ */

static const struct {
    const char* name;
    uint8_t code;
} mcss_encode_map[] = {
    {"CR", 0x00}, {"EF", 0x01}, {"ES", 0x02}, {"EW", 0x03},
    {"FR", 0x04}, {"ID", 0x05}, {"IN", 0x06}, {"NR", 0x07},
    {"RL", 0x08}, {"RS", 0x09}, {"SR", 0x0A}, {"ST", 0x0B},
    {NULL, 0xFF}
};

static const char* mcss_decode_map[] = {
    "CR", "EF", "ES", "EW", "FR", "ID", "IN", "NR", "RL", "RS", "SR", "ST"
};

static const char* mcss_long_names[] = {
    "Correction Run",
    "Emergency Fast Run",
    "Emergency Stop",
    "Emergency during Wait",
    "Fast Run",
    "Idle",
    "Inspection Run",
    "Not Ready",
    "Relevel",
    "Rescue Run",
    "Slow Run",
    "Stop"
};

/* ============================================
   COMPLETE SYSTEM INPUT MAPPING (ALL 102 codes)
   These are for reference/lookup from frames
   ============================================ */

/**
 * ALL 102 system input codes with descriptions
 * These can be looked up but not sent in compact format
 * (they're too numerous for payload transmission - use specific codes only)
 */
static const struct {
    const char* code;
    const char* description;
} system_input_map[] = {
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
    {"DFD", "Disable Front Door"},
    {"DRD", "Disable Rear Door"},
    {"GCB", "General Control Button"},
    {"^GCB", "Rear General Control Button"},
    {"CRC", "Card Reader Contact"}
};

/* ============================================
   VARIABLE ENCODING (4 bits per variable)
   ============================================ */

static const char* var_decode_map[] = {
    "None", "EDB", "DCB", "EDB,DCB", "HOV", "EDB,HOV",
    "DCB,HOV", "EDB,DCB,HOV", "DLC", "EDB,DLC", "DCB,DLC", "HOV,DLC",
    "Rsvd-C", "Rsvd-D", "Rsvd-E", "Rsvd-F"
};

/* ============================================
   ENCODING FUNCTIONS
   ============================================ */

/**
 * encode_ocss_compact - Map all 44 OCSS codes
 */
static uint8_t encode_ocss_compact(const char *ocss)
{
    if (!ocss || ocss[0] == '\0') return 0xFF;
    
    for (int i = 0; ocss_encode_map[i].name; i++) {
        if (strncmp(ocss, ocss_encode_map[i].name, 3) == 0) {
            return ocss_encode_map[i].code;
        }
    }
    return 0xFF;  // Unknown
}

/**
 * encode_mcss_compact - Map all 12 MCSS codes
 */
static uint8_t encode_mcss_compact(const char *mcss)
{
    if (!mcss || mcss[0] == '\0') return 0xFF;
    
    for (int i = 0; mcss_encode_map[i].name; i++) {
        if (strncmp(mcss, mcss_encode_map[i].name, 2) == 0) {
            return mcss_encode_map[i].code;
        }
    }
    return 0xFF;  // Unknown
}

/**
 * lookup_system_input - Get description for system input code
 */
static const char* lookup_system_input(const char *code)
{
    if (!code || code[0] == '\0') return "Unknown";
    
    for (int i = 0; i < 102; i++) {
        if (strcmp(system_input_map[i].code, code) == 0) {
            return system_input_map[i].description;
        }
    }
    return "Not found";
}

/**
 * encode_variable_to_4bit - Map variable strings to 4-bit codes
 */
static uint8_t encode_variable_to_4bit(const char *var_str)
{
    if (!var_str || var_str[0] == '\0') return 0x0;
    
    uint8_t code = 0;
    bool has_e = false, has_d = false, has_c = false, has_b = false;
    bool has_h = false, has_o = false, has_v = false, has_l = false;
    
    for (int i = 0; i < 4 && var_str[i]; i++) {
        if (isupper((unsigned char)var_str[i])) {
            if (var_str[i] == 'E') has_e = true;
            if (var_str[i] == 'D') has_d = true;
            if (var_str[i] == 'C') has_c = true;
            if (var_str[i] == 'B') has_b = true;
            if (var_str[i] == 'H') has_h = true;
            if (var_str[i] == 'O') has_o = true;
            if (var_str[i] == 'V') has_v = true;
            if (var_str[i] == 'L') has_l = true;
        }
    }
    
    if (has_e && has_d && has_b) code |= 0x1;
    if (has_d && has_c && has_b && !has_e) code |= 0x2;
    if (has_h && has_o && has_v) code |= 0x4;
    if (has_d && has_l && has_c) code |= 0x8;
    
    return code;
}

/* ============================================
   UART ISR CONTEXT FUNCTIONS
   ============================================ */

/**
 * uart_rx_isr_handler - Called from ISR when byte received
 * 
 * This function is invoked by the UART interrupt handler for every received byte.
 * It must be FAST (typically < 100µs to avoid missed bytes).
 * 
 * Design:
 *   - Append byte to ring buffer
 *   - If buffer full: increment error counter (frame loss)
 *   - If byte is '\n': signal frame_ready flag for main loop
 */
void uart_rx_isr_handler(uint8_t byte)
{
    ring_buffer_t *rb = &g_elevator_state.rx_buffer;
    
    // Check if buffer has space
    if (rb->count < RX_BUFFER_SIZE) {
        // Add byte at head
        rb->buffer[rb->head] = byte;
        rb->head = (rb->head + 1) % RX_BUFFER_SIZE;
        rb->count++;
        
        // Signal when potential frame start detected
        if (byte == '\n') {
            g_elevator_state.frame_ready = true;
        }
    } else {
        // Buffer overflow: increment error counter, discard byte
        // In production, this might trigger a recovery routine or LED alarm
        g_elevator_state.frame_errors++;
    }
}

/**
 * HAL UART callback - Redirects to our handler
 * 
 * Add this to stm32l0xx_it.c (in the HAL_UART_RxCpltCallback section):
 * 
 *   void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
 *   {
 *       if (huart->Instance == USART2) {  // Match your UART
 *           uart_rx_isr_handler(huart_rx_byte);
 *           HAL_UART_Receive_IT(&huart2, &huart_rx_byte, 1);  // Re-enable for next byte
 *       }
 *   }
 * 
 * Or, if using LL library, your USART2_IRQHandler in stm32l0xx_it.c:
 * 
 *   void USART2_IRQHandler(void)
 *   {
 *       if (LL_USART_IsActiveFlag_RXNE(USART2)) {
 *           uart_rx_isr_handler(LL_USART_ReceiveData8(USART2));
 *           LL_USART_ClearFlag_RXNE(USART2);
 *       }
 *   }
 */

/* ============================================
   MAIN LOOP CONTEXT FUNCTIONS
   ============================================ */

/**
 * elevator_parser_init - Initialize parser and UART
 * 
 * Call from main() after system initialization (HAL_Init, MX_USART2_UART_Init, etc.).
 * Sets up ring buffer and starts UART receive interrupt.
 * 
 * Side effects:
 *   - Zero-initializes ring buffer
 *   - Zero-initializes elevator_obj_t and state fields
 *   - Starts UART RX ISR
 */
void elevator_parser_init(void)
{
    // Initialize ring buffer
    g_elevator_state.rx_buffer.head = 0;
    g_elevator_state.rx_buffer.tail = 0;
    g_elevator_state.rx_buffer.count = 0;
    
    // Initialize state
    g_elevator_state.frame_ready = false;
    g_elevator_state.frame_errors = 0;
    
    // Zero structures
    memset(&g_elevator_state.elevator, 0, sizeof(elevator_obj_t));
    memset(g_elevator_state.fields, 0, sizeof(g_elevator_state.fields));
    
    // Start UART receive with interrupt
    // Using STM32 HAL:
    HAL_UART_Receive_IT(&huart2, &huart_rx_byte, 1);
    
    #ifdef DEBUG_MODE
    printf("[ELEVATOR] Parser initialized\r\n");
    printf("[ELEVATOR] UART RX interrupt enabled\r\n");
    printf("[ELEVATOR] Waiting for frames...\r\n");
    #endif
}

/**
 * extract_frame_from_buffer - Extract one frame from ring buffer
 * 
 * Reads FRAME_LENGTH bytes from ring buffer tail and advances tail pointer.
 * Call only after verifying sufficient data available.
 * 
 * Returns:
 *   true if frame extracted successfully
 *   false if insufficient data in buffer
 */
static bool extract_frame_from_buffer(char *frame_out, int *frame_length)
{
    ring_buffer_t *rb = &g_elevator_state.rx_buffer;
    
    // Not enough data for complete frame
    if (rb->count < FRAME_LENGTH) {
        return false;
    }
    
    // Copy FRAME_LENGTH bytes starting from tail
    for (int i = 0; i < FRAME_LENGTH; i++) {
        frame_out[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % RX_BUFFER_SIZE;
    }
    
    frame_out[FRAME_LENGTH] = '\0';
    rb->count -= FRAME_LENGTH;
    *frame_length = FRAME_LENGTH;
    
    return true;
}

/**
 * elevator_parser_process - Main processing function
 * 
 * Call repeatedly from main loop (or SysTick interrupt).
 * Non-blocking; returns immediately if no complete frame available.
 * 
 * Process:
 *   1. Check if frame_ready flag set
 *   2. Extract frame from ring buffer
 *   3. Validate frame structure
 *   4. Parse elevator parameters and menu state
 *   5. Clear frame_ready flag for next frame
 * 
 * After this returns, check g_elevator_state.elevator to read parsed data,
 * and g_elevator_state.elevator.position to determine current menu.
 * 
 * Side effects:
 *   - Updates g_elevator_state.elevator (parsed parameters)
 *   - Updates g_elevator_state.frame_errors (if validation fails)
 *   - Clears frame_ready flag
 */
void elevator_parser_process(void)
{
    // No complete frame available; return immediately
    if (!g_elevator_state.frame_ready) {
        return;
    }
    
    char frame[FRAME_LENGTH + 1];
    int frame_len = 0;
    
    // Try to extract frame from ring buffer
    if (!extract_frame_from_buffer(frame, &frame_len)) {
        // Shouldn't happen if frame_ready is set correctly, but defensive
        g_elevator_state.frame_ready = false;
        return;
    }
    
    // Validate frame structure
    if (validate_frame(frame)) {
        // Frame is valid; extract parameters
        extract_frame_parts(frame);
        
        // Determine menu type and parse accordingly
        dispatch_menu();
        
        #ifdef DEBUG_MODE
        print_elevator_status();
        #endif
    } else {
        // Frame validation failed
        g_elevator_state.frame_errors++;
        
        #ifdef DEBUG_MODE
        printf("[ERROR] Invalid frame structure\r\n");
        printf("[DEBUG] Expected: newline at [0], '[H' at [17:18], total length %d\r\n", FRAME_LENGTH);
        printf("[DEBUG] Got: %s\r\n", frame);
        #endif
    }
    
    // Always clear frame_ready for next iteration
    g_elevator_state.frame_ready = false;
}

/* ============================================
   FRAME VALIDATION & PARSING
   ============================================ */

/**
 * validate_frame - Check frame structure
 * 
 * Validates:
 *   - Length is exactly FRAME_LENGTH bytes
 *   - First character is newline ('\n')
 *   - Characters at positions 17-18 are "[H"
 * 
 * Returns: true if valid, false otherwise
 */
bool validate_frame(const char* frame)
{
    // Check length
    if (strlen(frame) != FRAME_LENGTH) {
        return false;
    }
    
    // Check for newline at start
    if (frame[0] != '\n') {
        return false;
    }
    
    // Check for marker at positions 17-18
    if (frame[17] != '[' || frame[18] != 'H') {
        return false;
    }
    
    return true;
}

/**
 * extract_frame_parts - Parse frame into top/bottom LCD screens
 * 
 * Frame format:
 *   Byte 0: '\n' (newline)
 *   Byte 1-16: LCD top half (16 chars)
 *   Byte 17-18: "[H" (marker)
 *   Byte 19-34: LCD bottom half (16 chars)
 * 
 * This function splits the data around the "[H" marker:
 *   - First half before "[H" → bottom_screen
 *   - Second half after "[H" → top_screen
 * 
 * (Yes, it's backwards; this is how the elevator system sends it.)
 */
void extract_frame_parts(const char* frame)
{
    const char* data_start = frame + 1;  // Skip leading newline
    const char* marker_pos = strstr(data_start, "[H");
    
    if (marker_pos == NULL) {
        // Shouldn't happen if validate_frame() passed, but safe fallback
        g_elevator_state.elevator.top_screen[0] = '\0';
        g_elevator_state.elevator.bottom_screen[0] = '\0';
        return;
    }
    
    int data_length = marker_pos - data_start;
    
    // Copy first half to bottom_screen
    if (data_length > 0) {
        strncpy(g_elevator_state.elevator.bottom_screen, data_start, data_length);
        g_elevator_state.elevator.bottom_screen[data_length] = '\0';
    } else {
        g_elevator_state.elevator.bottom_screen[0] = '\0';
    }
    
    // Copy second half (after "[H") to top_screen
    if (data_length > 0) {
        strncpy(g_elevator_state.elevator.top_screen, data_start + data_length + 2, data_length);
        g_elevator_state.elevator.top_screen[data_length] = '\0';
    } else {
        g_elevator_state.elevator.top_screen[0] = '\0';
    }
}

/* ============================================
   MENU PARSING & DISPATCH
   ============================================ */

/**
 * contains - Simple substring search
 */
static bool contains(const char *haystack, const char *needle)
{
    return strstr(haystack, needle) != NULL;
}

/**
 * is_input_menu - Check if top_screen starts with A, B, or C
 * 
 * Input menu format: "ABC..." → Menu selection input
 * Other menu: something else
 */
static bool is_input_menu(const char *top_screen)
{
    if (top_screen == NULL || top_screen[0] == '\0') {
        return false;
    }
    char ch = top_screen[0];
    return (ch == 'A' || ch == 'B' || ch == 'C');
}

/**
 * input_menu_parse - Extract elevator state from input menu frame
 * 
 * When top_screen starts with A/B/C, frame contains elevator sensor data.
 * Parse positions are hardcoded based on elevator protocol.
 * 
 * Frame layout example:
 *   top_screen:    "A-01IDL SR][]["  (16 chars)
 *   bottom_screen: "xxxx xxxx xxxx xxxx"  (16 chars, var1-var4)
 * 
 * Offsets in top_screen:
 *   [0]: Car ID (A/B/C)
 *   [1]: Direction (U/D/-)
 *   [2:4]: Level (floor number, 2 digits)
 *   [5:8]: OCSS status (3 chars)
 *   [9:11]: MCSS status (2 chars)
 *   [12:14]: Front door (e.g., "][")
 *   [14:16]: Rear door (e.g., "[]")
 * 
 * Offsets in bottom_screen (4-char variable fields):
 *   [0:4]: var1
 *   [4:8]: var2
 *   [8:12]: var3
 *   [12:16]: var4
 */
static void input_menu_parse(void)
{
    elevator_obj_t *e = &g_elevator_state.elevator;
    
    // Parse top screen (fixed positions)
    memcpy(e->car_id, e->top_screen + 0, 1);      e->car_id[1] = '\0';
    memcpy(e->direction, e->top_screen + 1, 1);   e->direction[1] = '\0';
    memcpy(e->level, e->top_screen + 2, 2);       e->level[2] = '\0';
    memcpy(e->ocss, e->top_screen + 5, 3);        e->ocss[3] = '\0';
    memcpy(e->mcss, e->top_screen + 9, 2);        e->mcss[2] = '\0';
    memcpy(e->door, e->top_screen + 12, 2);       e->door[2] = '\0';
    memcpy(e->rear_door, e->top_screen + 14, 2);  e->rear_door[2] = '\0';
    
    // Parse bottom screen (4-char variable fields)
    memcpy(e->var1, e->bottom_screen + 0, 4);     e->var1[4] = '\0';
    memcpy(e->var2, e->bottom_screen + 4, 4);     e->var2[4] = '\0';
    memcpy(e->var3, e->bottom_screen + 8, 4);     e->var3[4] = '\0';
    memcpy(e->var4, e->bottom_screen + 12, 4);    e->var4[4] = '\0';
    
    #ifdef DEBUG_MODE
    printf("\r\n+------ ELEVATOR STATUS ------+\r\n");
    printf("| Car ID    : %s\r\n", e->car_id);
    printf("| Direction : %s\r\n", e->direction);
    printf("| Level     : %s\r\n", e->level);
    printf("| OCSS      : %s (%s)\r\n", e->ocss, lookup_ocss(e->ocss));
    printf("| MCSS      : %s (%s)\r\n", e->mcss, lookup_mcss(e->mcss));
    printf("| Front Door: %s\r\n", e->door);
    printf("| Rear Door : %s\r\n", e->rear_door);
    printf("| Vars      : %s | %s | %s | %s\r\n", e->var1, e->var2, e->var3, e->var4);
    printf("+------ END STATUS ------+\r\n");
    #endif
}

/**
 * dispatch_menu - Determine menu type and parse
 * 
 * Sets g_elevator_state.elevator.position to indicate current menu:
 *   - MENU_INPUT: Elevator data frame (ready for LoRaWAN payload)
 *   - MENU_SYSTEM, MENU_STATUS, MENU_TCBC: Menu screens
 *   - MENU_PRINCIPAL: Top-level menu
 *   - NA: Unknown screen
 * 
 * After dispatch_menu() returns, check position to determine next action.
 * For production, focus on MENU_INPUT state (that's when you have sensor data).
 */
void dispatch_menu(void)
{
    elevator_obj_t *e = &g_elevator_state.elevator;
    
    // Check if this is an input menu (elevator sensor data)
    if (is_input_menu(e->top_screen)) {
        e->position = MENU_INPUT;
        input_menu_parse();
        
        // Build and send payload
        uint8_t payload[64];
        int len = 0;
        
        // Option 1: Minimal (1 byte)
        build_minimal_payload(payload, &len);
        
        // Option 2: Extended (variable)
        // build_extended_payload(payload, &len);
        
        send_lorawan_frame(payload, len);
        return;
    }
    
    // Check for menu screens with specific keywords
    if (contains(e->top_screen, "Menu")) {
        if (contains(e->top_screen, "SYSTEM")) {
            e->position = MENU_SYSTEM;
        } else if (contains(e->top_screen, "STATUS")) {
            e->position = MENU_STATUS;
        } else if (contains(e->top_screen, "TCBC")) {
            e->position = MENU_TCBC;
        } else {
            e->position = NA;
        }
        return;
    }
    
    // Default to unknown
    e->position = NA;
}

/* ============================================
   PAYLOAD ENCODING UTILITIES
   ============================================ */

/**
 * map_direction - Convert direction to 2-bit value
 * 'U' → 1 (Up), 'D' → 0 (Down), '-' → 2 (Stationary), else → 3 (Error)
 */
static uint8_t map_direction(const char *dir)
{
    if (dir == NULL || dir[0] == '\0') {
        return 3;  // error
    }
    
    switch (dir[0]) {
        case 'U': return 1;  // Up
        case 'D': return 0;  // Down
        case '-': return 2;  // Stationary/idle
        default: return 3;   // Error/unknown
    }
}

/**
 * map_door_state - Convert door character to 1-bit value
 * '[' → 0 (Closed/safe), ']' → 1 (Open/unsafe)
 */
static uint8_t map_door_state(const char *door)
{
    if (door == NULL || door[0] == '\0') {
        return 0;  // Default to safe (closed)
    }
    return (door[0] == ']') ? 1 : 0;  // ] = open (1), [ = closed (0)
}

/**
 * map_operational_status - Determine elevator operational mode
 * 
 * Returns:
 *   1: Working states (NOR, PRK initial, IDL between calls)
 *   2: Idle (stationary, waiting)
 *   3: Parking (PRK - parked state)
 *   0: Out of Service (any other state like EFO, EFS, INS, etc.)
 * 
 * Logic:
 *   - NOR = Normal operation (1)
 *   - PRK = Parking (3) 
 *   - IDL = Idle/waiting (2)
 *   - Others (EFO, EFS, INS, etc.) = Out of Service (0)
 */
static uint8_t map_operational_status(const char *ocss)
{
    if (ocss == NULL || ocss[0] == '\0') {
        return 0;  // Unknown = out of service
    }
    
    if (strncmp(ocss, "NOR", 3) == 0) return 1;  // Normal working
    if (strncmp(ocss, "IDL", 3) == 0) return 2;  // Idle
    if (strncmp(ocss, "PRK", 3) == 0) return 3;  // Parking
    
    // All other states (EFO, EFS, INS, WCS, ACP, etc.) are out of service
    return 0;
}

/**
 * map_level - Convert ASCII level string to 8-bit value (0-99)
 * Input: "00" to "99" (2-char string)
 * Output: 0-99 (uint8_t), or 255 if invalid
 */
static uint8_t map_level(const char *level)
{
    if (level == NULL || level[0] == '\0' || level[1] == '\0') {
        return 255;  // error: string too short
    }
    
    uint8_t tens = 0, ones = 0;
    
    // Parse tens digit
    if (level[0] >= '0' && level[0] <= '9') {
        tens = level[0] - '0';
    } else {
        return 255;  // invalid tens digit
    }
    
    // Parse ones digit
    if (level[1] >= '0' && level[1] <= '9') {
        ones = level[1] - '0';
    } else {
        return 255;  // invalid ones digit
    }
    
    uint8_t result = tens * 10 + ones;
    
    if (result > 99) {
        return 255;  // error: level out of range
    }
    
    return result;
}

/**
 * pack_bits - Pack bitfield value into byte at specific bit position
 * 
 * Usage:
 *   uint8_t byte = 0;
 *   pack_bits(&byte, 0, 2, 0x1);   // Put 0x1 (2 bits) at bit 0
 *   pack_bits(&byte, 2, 1, 0x0);   // Put 0x0 (1 bit) at bit 2
 *   pack_bits(&byte, 3, 1, 0x0);   // Put 0x0 (1 bit) at bit 3
 *   pack_bits(&byte, 4, 2, 0x1);   // Put 0x1 (2 bits) at bit 4
 */
static void pack_bits(uint8_t *byte, uint8_t start_bit, uint8_t num_bits, uint8_t value)
{
    uint8_t mask = (1 << num_bits) - 1;
    uint8_t shift_val = value & mask;
    *byte |= (shift_val << start_bit);


}

/**
 * count_uppercase_vars - Count how many variables are uppercase (enabled)
 * 
 * Uppercase = enabled/on
 * Lowercase = disabled/off
 * Example: "dcb" → 0, "DCB" → 3, "dCb" → 1
 */
static uint8_t count_uppercase_vars(const char *var_str)
{
    if (var_str == NULL) return 0;
    
    uint8_t count = 0;
    for (int i = 0; i < 4 && var_str[i] != '\0'; i++) {
        if (isupper((unsigned char)var_str[i])) {
            count++;
        }
    }
    return count;
}

/**
 * extract_uppercase_vars - Extract only uppercase characters from 4 variables
 * 
 * Returns: dynamically built string of uppercase chars only
 * Example input: var1="DCB", var2="dls", var3="HOV", var4="lrc"
 * Returns: "DBHOV" (6 chars)
 * 
 * WARNING: Caller must free returned pointer (or use static buffer)
 */
static void extract_uppercase_vars(const char *v1, const char *v2, const char *v3, const char *v4,
                                   char *output_buffer, int *output_len)
{
    if (output_buffer == NULL) return;
    
    int idx = 0;
    const char *vars[4] = {v1, v2, v3, v4};
    
    // Iterate through all 4 variables
    for (int i = 0; i < 4; i++) {
        if (vars[i] == NULL) continue;
        
        // Extract uppercase chars from this variable (max 4 chars per variable)
        for (int j = 0; j < 4 && vars[i][j] != '\0'; j++) {
            if (isupper((unsigned char)vars[i][j])) {
                output_buffer[idx++] = vars[i][j];
            }
        }
    }
    
    output_buffer[idx] = '\0';
    *output_len = idx;
}

/* ============================================
   MINIMAL PAYLOAD (1 byte)
   ============================================ */

/**
 * build_minimal_payload - Ultra-compact 1-byte payload
 * 
 * Format:
 *   Byte 0: [Padding(2b) | Status(2b) | Rear(1b) | Front(1b) | Direction(2b)]
 * 
 * Bitfield breakdown (LSB first):
 *   Bits 0-1: Direction (0=Down, 1=Up, 2=Stationary, 3=Error)
 *   Bit 2: Front Door (0=Closed, 1=Open)
 *   Bit 3: Rear Door (0=Closed, 1=Open)
 *   Bits 4-5: Status (0=Out of Service, 1=Working, 2=Idle, 3=Parked)
 *   Bits 6-7: Reserved for future use
 * 
 * Example: Up, Doors Closed, Normal
 *   Byte 0 = 0x09
 *   Binary: 0000 1001
 *           |||| ||||
 *           |||| ++-- Direction = 1 (Up)
 *           ||++---- Doors = 00 (both closed)
 *           ++------ Status = 1 (Working)
 * 
 * Airtime: Absolute minimum (1 byte overhead + LoRaWAN header ~20 bytes)
 * Use when: Frequent updates (every 30-60 seconds), battery-critical
 */
void build_minimal_payload(uint8_t *payload_buffer, int *payload_len)
{
    if (payload_buffer == NULL || payload_len == NULL) {
        return;
    }
    
    elevator_obj_t *e = &g_elevator_state.elevator;
    
    uint8_t byte0 = 0;
    pack_bits(&byte0, 0, 2, map_direction(e->direction));           // Bits 0-1
    pack_bits(&byte0, 2, 1, map_door_state(e->door));               // Bit 2: Front
    pack_bits(&byte0, 3, 1, map_door_state(e->rear_door));          // Bit 3: Rear
    pack_bits(&byte0, 4, 2, map_operational_status(e->ocss));       // Bits 4-5
    // Bits 6-7 reserved
    
    payload_buffer[0] = byte0;
    *payload_len = 1;
    
    #ifdef DEBUG_MODE
    printf("[MINIMAL] 1-byte: 0x%02X\r\n", byte0);
    printf("  Dir=%u Front=%u Rear=%u Status=%u\r\n",
           map_direction(e->direction),
           map_door_state(e->door),
           map_door_state(e->rear_door),
           map_operational_status(e->ocss));
    #endif
}

/* ============================================
   EXTENDED COMPACT PAYLOAD 
   ============================================ */

/**
 * build_extended_compact_payload - 6-byte format
 * Byte 0: Status header
 * Byte 1: Level
 * Byte 2: OCSS code (one of 44 codes)
 * Byte 3: MCSS code (one of 12 codes)
 * Byte 4: Var1-2
 * Byte 5: Var3-4
 */
void build_extended_compact_payload(uint8_t *payload_buffer, int *payload_len)
{
    if (!payload_buffer || !payload_len) return;
    
    elevator_obj_t *e = &g_elevator_state.elevator;
    uint8_t idx = 0;
    
    uint8_t byte0 = 0;
    pack_bits(&byte0, 0, 2, map_direction(e->direction));
    pack_bits(&byte0, 2, 1, map_door_state(e->door));
    pack_bits(&byte0, 3, 1, map_door_state(e->rear_door));
    uint8_t status = (e->ocss == 0x1F) ? 2 : (e->ocss == 0x29) ? 3 : 1;
    pack_bits(&byte0, 4, 2, status);
    payload_buffer[idx++] = byte0;
    
    payload_buffer[idx++] = map_level(e->level);
    payload_buffer[idx++] = e->ocss;  // All 44 OCSS codes
    payload_buffer[idx++] = e->mcss;  // All 12 MCSS codes
    
    uint8_t byte4 = 0;
    pack_bits(&byte4, 0, 4, e->var1);
    pack_bits(&byte4, 4, 4, e->var2);
    payload_buffer[idx++] = byte4;
    
    uint8_t byte5 = 0;
    pack_bits(&byte5, 0, 4, e->var3);
    pack_bits(&byte5, 4, 4, e->var4);
    payload_buffer[idx++] = byte5;
    
    *payload_len = idx;

    #ifdef DEBUG_MODE
    printf("[EXTENDED] %d bytes: ", *payload_len);
    for (int i = 0; i < *payload_len; i++) {
        if (i >= 5) {
            printf("%c", (char)payload_buffer[i]);  // Print vars as ASCII
        } else {
            printf("%02X ", payload_buffer[i]);     // Print hex for header
        }
    }
    printf("\r\n");
    printf("  Dir=%u Level=%u OCSS=%s Vars=%s\r\n",
           map_direction(e->direction),
           map_level(e->level),
           e->ocss,
           var_buffer);
    #endif
}
/* ============================================
   DEBUG HELPERS (Optional)
   ============================================ */

#ifdef DEBUG_MODE

void print_elevator_status(void)
{
    elevator_obj_t *e = &g_elevator_state.elevator;
    
    printf("\r\n╔══════════════════════════════╗\r\n");
    printf("║     ELEVATOR STATUS FRAME     ║\r\n");
    printf("╠══════════════════════════════╣\r\n");
    printf("║ Car ID    : %s%-14s║\r\n", e->car_id, "");
    printf("║ Direction : %s%-14s║\r\n", e->direction, "");
    printf("║ Level     : %s%-14s║\r\n", e->level, "");
    printf("║ OCSS      : %s%-14s║\r\n", e->ocss, "");
    printf("║ MCSS      : %s%-14s║\r\n", e->mcss, "");
    printf("║ Front Door: %s%-14s║\r\n", e->door, "");
    printf("║ Rear Door : %s%-14s║\r\n", e->rear_door, "");
    printf("║ Var1-4    : %s %s %s %s║\r\n", e->var1, e->var2, e->var3, e->var4);
    printf("║ Errors    : %-16u║\r\n", e->frame_error_count);
    printf("║ Position  : %-16d║\r\n", e->position);
    printf("╚══════════════════════════════╝\r\n");
}

void print_lcd_screen(void)
{
    printf("\r\n+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\r\n");
    
    printf("|%-*s|\r\n", LCD_WIDTH, g_elevator_state.elevator.top_screen);
    
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\r\n");
    
    printf("|%-*s|\r\n", LCD_WIDTH, g_elevator_state.elevator.bottom_screen);
    
    printf("+");
    for (int i = 0; i < LCD_WIDTH; i++) printf("-");
    printf("+\r\n\r\n");
}

#endif

/* ============================================
   END OF FILE
   ============================================ */