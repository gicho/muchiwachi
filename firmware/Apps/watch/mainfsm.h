/* mainfsm.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
typedef enum { MSTATE_BOOT, MSTATE_IDLE, MSTATE_CFG, MSTATE_TIME, MSTATE_RADAR, MSTATE_MESSAGE } state_t;

enum mainfsm_event { BOOT_DONE, TILT60, UNTILT60, NEXT_PUSH, ACCEPT_PUSH, BOTH_HOLD, BOTH_RELEASE, TICK_1S, WAKEUP, SHORT_INACTIVE, LONG_INACTIVE, MESSAGE_ARRIVED };

void mainfsm_process_event(enum mainfsm_event evt);
void mainfsm_process_event_with_data(enum mainfsm_event evt, void* data);

state_t mainfsm_get_state();
