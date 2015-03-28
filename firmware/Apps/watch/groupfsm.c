/* groupfsm.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */

#include "blecen.h"
#include "bleprofile.h"
#include "spar_utils.h"

#include "muchi.h"
#include "groupfsm.h"
#include "display.h"
#include "store.h"

#define NUM_STATES 2

typedef enum { IDLE, GROUPING } state_t;
static state_t cur_state = IDLE;

typedef struct {
    enum groupfsm_event type;
    void *data;
} event_t;

typedef state_t state_func_t(event_t evt);
typedef void transition_func_t(event_t evt);


/* state actions */
static state_t do_state_idle(event_t evt);
static state_t do_state_grouping(event_t evt);

/* transition actions */
static void do_idle_to_grouping(event_t evt);
static void do_grouping_to_idle(event_t evt);

/* transition table [from][to] */
static transition_func_t * const transition_table[ NUM_STATES ][ NUM_STATES ] = {
    { NULL,                 do_idle_to_grouping },
    { do_grouping_to_idle,  NULL                },
};


static state_func_t* const state_table[ NUM_STATES ] = {
    do_state_idle, do_state_grouping
};

static char* state_name[ NUM_STATES ] = {
    "IDLE", "GROUPING"
};

#define NUM_EVENTS 6
static char* event_name[ NUM_EVENTS ] = {
    "GROUPING_START", "GROUPING_STOP", "GROUPING_ADV"
};


/*****************************************************************************/
state_t do_state_idle(event_t evt)
{
    switch (evt.type) {
        case GROUPING_START:
            grpr_start_adverts(FLAG_GROUPING);
            return GROUPING;
        default:
            return IDLE;
    }
}

/*****************************************************************************/
#define GROUPER_MIN_RSSI_INVITATION   -50
state_t do_state_grouping(event_t evt)
{
    switch (evt.type) {
        case GROUPING_ADV:
            if (evt.data != NULL) {
                HCIULP_ADV_PACKET_REPORT_WDATA *pktdata = evt.data;
                if (pktdata->rssi > GROUPER_MIN_RSSI_INVITATION) {
                    grpr_store_add(pktdata->wd_addr);
                }
            }
            return GROUPING;
        case GROUPING_STOP:
            grpr_start_adverts(0);
            return IDLE;
        default:
            return GROUPING;
    }
}

/*****************************************************************************/
void do_idle_to_grouping(event_t evt)
{
    UNUSED(evt);
}

/*****************************************************************************/
void do_grouping_to_idle(event_t evt)
{
    UNUSED(evt);
}

/*****************************************************************************/
void groupfsm_process_event(enum groupfsm_event type) {
    groupfsm_process_event_with_data(type, NULL);
}

/*****************************************************************************/
void groupfsm_process_event_with_data(enum groupfsm_event type, void *data) {
    event_t evt = { type, data };
    state_t new_state = state_table[cur_state](evt);


    transition_func_t *transition = transition_table[cur_state][new_state];

    if (transition) {
        transition(evt);
    }

    if (cur_state != new_state) {
        ble_trace1("G-EVENT: %s\r\n", (INT32) event_name[type]);
        ble_trace2("G-STATE: %s -> %s\r\n", (INT32) state_name[cur_state], (INT32) state_name[new_state]);
    }

    cur_state = new_state;
}

/*****************************************************************************/
UINT32 groupfsm_get_state()
{
    return cur_state;
}
