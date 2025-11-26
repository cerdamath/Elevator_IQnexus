#ifndef ELEVATOR_PARSER_H
#define ELEVATOR_PARSER_H

#include <stdint.h>
#include <stdbool.h>

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
// callback removed; not used on MCU
sm_menu_e position;
char top_screen[LCD_WIDTH + 1];
char bottom_screen[LCD_WIDTH + 1];
char car_id; // "A" + '\0'
char direction; // "-" + '\0'​
char level; // "01" + '\0'​
char ocss; // "IDL" + '\0'​
char mcss; // "ST" + '\0'​
char door; // "][" + '\0'​
char rear_door; // "[]" + '\0'​
char var1; // "dcb" + '\0'​
char var2;
char var3;
char var4;

int frame_error_count;
} elevator_obj_t;

// API for your STM32 code
void Metro_Init(void);
void Metro_PushByte(uint8_t b);

bool Metro_HasNewFrame(void);
const elevator_obj_t *Metro_GetLastElevator(void);

// Optional: expose menu position/fields if useful to you
const menu_field_t *Metro_GetMenuFields(uint8_t *count);
sm_menu_e Metro_GetMenuPosition(void);

#endif // ELEVATOR_PARSER_H
