#ifndef WOLFBOOT_MENU_H
#define WOLFBOOT_MENU_H

#include <stdint.h>

/* What the operator asked for */
typedef enum {
    SLOT_AUTO = 0,   /* no override; use wolfBoot’s normal policy */
    SLOT_A    = 1,
    SLOT_B    = 2,
} slot_choice_t;

/* One-time interactive menu. Returns SLOT_AUTO/A/B */
slot_choice_t menu_preboot_run(void);

/* Persistent “default slot” helpers (tiny flash record you implement) */
void          persist_preferred_slot(slot_choice_t choice);
slot_choice_t read_preferred_slot(void);   /* returns SLOT_AUTO if none/invalid */
void          clear_preferred_slot(void);

/* Global “hint” written by loader, read by selector */
extern volatile slot_choice_t g_menu_choice;

#endif /* WOLFBOOT_MENU_H */