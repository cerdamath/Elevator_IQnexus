#ifndef ELEVATOR_H
#define ELEVATOR_H

#define MAX_DOORS_DESC 64
#define MAX_MENU_SCREEN 128

typedef struct Elevator {
    char carid[2];
    char level[32];
    char dir[32];
    char ocss[32];
    char mcss[32];
    char doors[MAX_DOORS_DESC];
    char menu_screen[MAX_MENU_SCREEN];
} Elevator;

// Constructor
Elevator* Elevator_create(void);
// Destructor
void Elevator_destroy(Elevator *e);

void Elevator_printScreen(const Elevator* e);

// Getters
const char* Elevator_getCarID(const Elevator *e);
const char* Elevator_getLevel(const Elevator *e);
const char* Elevator_getDir(const Elevator *e);
const char* Elevator_getOCSS(const Elevator *e);
const char* Elevator_getMCSS(const Elevator *e);
const char* Elevator_getDoors(const Elevator *e);
const char* Elevator_getMenuScreen(const Elevator *e);

// Setters
void Elevator_setCarID(Elevator *e, const char* value);
void Elevator_setLevel(Elevator *e, const char* value);
void Elevator_setDir(Elevator *e, const char* value);
void Elevator_setOCSS(Elevator *e, const char* value);
void Elevator_setMCSS(Elevator *e, const char* value);
void Elevator_setDoors(Elevator *e, const char* value);
void Elevator_setMenuScreen(Elevator *e, const char* value);

#endif
