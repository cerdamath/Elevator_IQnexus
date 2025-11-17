#include "Elevator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Elevator* Elevator_create(void) {
    Elevator* e = (Elevator*)malloc(sizeof(Elevator));
    if (!e) return NULL;
    strcpy(e->carid, "$");
    strcpy(e->level, "$");
    strcpy(e->dir, "$");
    strcpy(e->ocss, "$");
    strcpy(e->mcss, "$");
    strcpy(e->doors, "$");
    strcpy(e->menu_screen, "");
    return e;
}

void Elevator_destroy(Elevator *e) {
    free(e);
}

void Elevator_printScreen(const Elevator* e) {
    printf("+-----------------------------+\n");
    printf("|       ELEVATOR STATUS       |\n");
    printf("+-----------------------------+\n");
    printf("| CarID   : %-15s |\n", Elevator_getCarID(e));
    printf("| Level   : %-15s |\n", Elevator_getLevel(e));
    printf("| Dir     : %-15s |\n", Elevator_getDir(e));
    printf("| OCSS    : %-15s |\n", Elevator_getOCSS(e));
    printf("| MCSS    : %-15s |\n", Elevator_getMCSS(e));
    printf("| Doors   : %-15s |\n", Elevator_getDoors(e));
    printf("| Menu    : %-15s |\n", Elevator_getMenuScreen(e));
    printf("+-----------------------------+\n");
}


// Getters
const char* Elevator_getCarID(const Elevator *e)     { return e->carid; }   
const char* Elevator_getLevel(const Elevator *e)     { return e->level; }
const char* Elevator_getDir(const Elevator *e)       { return e->dir; }
const char* Elevator_getOCSS(const Elevator *e)      { return e->ocss; }
const char* Elevator_getMCSS(const Elevator *e)      { return e->mcss; }
const char* Elevator_getDoors(const Elevator *e)     { return e->doors; }
const char* Elevator_getMenuScreen(const Elevator *e){ return e->menu_screen; }

// Setters
void Elevator_setCarID(Elevator *e, const char* value)       { strncpy(e->carid, value, sizeof(e->carid)-1); e->carid[sizeof(e->carid)-1]='\0'; }
void Elevator_setLevel(Elevator *e, const char* value)       { strncpy(e->level, value, sizeof(e->level)-1); e->level[sizeof(e->level)-1]='\0'; }
void Elevator_setDir(Elevator *e, const char* value)         { strncpy(e->dir, value, sizeof(e->dir)-1); e->dir[sizeof(e->dir)-1]='\0'; }
void Elevator_setOCSS(Elevator *e, const char* value)        { strncpy(e->ocss, value, sizeof(e->ocss)-1); e->ocss[sizeof(e->ocss)-1]='\0'; }
void Elevator_setMCSS(Elevator *e, const char* value)        { strncpy(e->mcss, value, sizeof(e->mcss)-1); e->mcss[sizeof(e->mcss)-1]='\0'; }
void Elevator_setDoors(Elevator *e, const char* value)       { strncpy(e->doors, value, sizeof(e->doors)-1); e->doors[sizeof(e->doors)-1]='\0'; }
void Elevator_setMenuScreen(Elevator *e, const char* value)  { strncpy(e->menu_screen, value, sizeof(e->menu_screen)-1); e->menu_screen[sizeof(e->menu_screen)-1]='\0'; }
