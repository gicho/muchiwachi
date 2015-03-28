/* groupfsm.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */

enum groupfsm_event { GROUPING_START, GROUPING_STOP, GROUPING_ADV };
void groupfsm_process_event(enum groupfsm_event evt);
void groupfsm_process_event_with_data(enum groupfsm_event evt, void* data);
