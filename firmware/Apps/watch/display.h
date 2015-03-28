/* display.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#ifndef DISPLAY_H
#define DISPLAY_H
#include "types.h"

typedef enum keys { KEY_ACCEPT, KEY_NEXT } grouper_keys_t;

void display_init();
void display_off();
void display_on();
void display_update();

int display_keypress(grouper_keys_t key);

extern char g_avatar[2];

#endif /* DISPLAY_H */
