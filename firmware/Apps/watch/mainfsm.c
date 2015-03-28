/* mainfsm.c
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
#include "mainfsm.h"
#include "groupfsm.h"
#include "display.h"
#include "nvram.h"

#define NUM_STATES 6

static state_t cur_state = MSTATE_BOOT;

typedef struct {
    enum mainfsm_event type;
    void *data;
} event_t;

typedef state_t state_func_t(event_t evt);
typedef void transition_func_t(event_t evt);

/* state actions */
static state_t do_state_boot(event_t evt);
static state_t do_state_idle(event_t evt);
static state_t do_state_cfg(event_t evt);
static state_t do_state_time(event_t evt);
static state_t do_state_radar(event_t evt);
static state_t do_state_message(event_t evt);

/* transition actions */
static void do_boot_to_idle(event_t evt);
static void do_idle_to_cfg(event_t evt);
static void do_cfg_to_time(event_t evt);
static void do_radar_to_time(event_t evt);
static void do_time_to_radar(event_t evt);

/* state variables */

/* transition table [from][to] */
static transition_func_t * const transition_table[ NUM_STATES ][ NUM_STATES ] = {
    { NULL,  do_boot_to_idle,  NULL,              NULL,              NULL            , NULL },
    { NULL,  NULL,             do_idle_to_cfg,    NULL,              NULL            , NULL },
    { NULL,  NULL,             NULL,              do_cfg_to_time,    NULL            , NULL },
    { NULL,  NULL,             NULL,              NULL,              do_time_to_radar, NULL },
    { NULL,  NULL,             NULL,              do_radar_to_time,  NULL            , NULL },
    { NULL,  NULL,             NULL,              NULL,              NULL            , NULL },
};


static state_func_t* const state_table[ NUM_STATES ] = {
    do_state_boot, do_state_idle, do_state_cfg, do_state_time, do_state_radar, do_state_message
};

static char* state_name[ NUM_STATES ] = {
    "BOOT", "IDLE", "CONF", "TIME", "RADAR", "MESSAGE"
};

#define NUM_EVENTS 12
static char* event_name[ NUM_EVENTS ] = {
    "BOOT_DONE", "TILT60", "UNTILT60", "NEXT_PUSH", "ACCEPT_PUSH", "BOTH_HOLD", "BOTH_RELEASE", "TICK_1S", "WAKEUP", "SHORT_INACTIVE", "LONG_INACTIVE", "MESSAGE_ARRIVED"
};

static state_t do_state_boot(event_t evt)
{
    switch (evt.type) {
        case BOOT_DONE:
            return MSTATE_IDLE;
        default:
            return MSTATE_BOOT;
    }
}

static state_t do_state_idle(event_t evt)
{
    UINT8 config_valid;
    switch (evt.type) {
        case NEXT_PUSH:
        case ACCEPT_PUSH:
            bleprofile_ReadNVRAM(MUCHI_NVM_CONFIG_IS_VALID, 1, &config_valid);
            return config_valid ? MSTATE_TIME : MSTATE_CFG;
        default:
            return MSTATE_IDLE;
    }
}

static state_t do_state_message(event_t evt)
{
    switch (evt.type) {
        case TICK_1S:
            display_update();
            return MSTATE_MESSAGE;
        case ACCEPT_PUSH:
            return MSTATE_TIME;
        default:
            return MSTATE_MESSAGE;
    }
}


static state_t do_state_time(event_t evt)
{
    static int ignore_wake = 0;
    switch (evt.type) {
        case MESSAGE_ARRIVED:
            return MSTATE_MESSAGE;
        case TICK_1S:
            display_update();
            return MSTATE_TIME;
        case TILT60:
            display_update();
            return MSTATE_RADAR;
        case ACCEPT_PUSH:
            ignore_wake = 1;
            /* fall through */
        case SHORT_INACTIVE:
            display_off();
            return MSTATE_TIME;
        case NEXT_PUSH:
            ignore_wake = 0;
        case WAKEUP:
            if (!ignore_wake) {
                display_on();
                grpr_start_adverts(0);
            }
            return MSTATE_TIME;
        case LONG_INACTIVE:
            grpr_stop_adverts();
            return MSTATE_TIME;
        case BOTH_HOLD:
            display_on();
            groupfsm_process_event(GROUPING_START);
            return MSTATE_RADAR;
        case BOTH_RELEASE:
            groupfsm_process_event(GROUPING_STOP);
            return MSTATE_TIME;
        default:
            return MSTATE_TIME;
    }
}

static state_t do_state_cfg(event_t evt)
{
    int config_done;
    switch (evt.type) {
        case NEXT_PUSH:
            display_keypress(KEY_NEXT);
            display_update();
            return MSTATE_CFG;
        case ACCEPT_PUSH:
            config_done = display_keypress(KEY_ACCEPT);
            if (config_done) {
                int bytes;
                bytes = bleprofile_WriteNVRAM(MUCHI_NVM_AVATAR_CODE, sizeof(g_avatar),
                                                                (UINT8 *) &g_avatar);
                if (bytes == sizeof(g_avatar)) {
                    UINT8 valid_config = 1;
                    bleprofile_WriteNVRAM(MUCHI_NVM_CONFIG_IS_VALID, 1, &valid_config);
                }
            }
            return config_done ? MSTATE_TIME : MSTATE_CFG;
        case TICK_1S:
            display_update();
            return MSTATE_CFG;
        default:
            return MSTATE_CFG;
    }
}

static state_t do_state_radar(event_t evt)
{
    switch (evt.type) {
        case MESSAGE_ARRIVED:
            return MSTATE_MESSAGE;
        case TICK_1S:
            display_update();
            return MSTATE_RADAR;
        case UNTILT60:
            display_update();
            return MSTATE_TIME;
        case SHORT_INACTIVE:
            display_off();
            return MSTATE_TIME;
        case BOTH_HOLD:
            groupfsm_process_event(GROUPING_START);
            return MSTATE_RADAR;
        case BOTH_RELEASE:
            groupfsm_process_event(GROUPING_STOP);
            return MSTATE_TIME;
        default:
            return MSTATE_RADAR;
    }
}

static void do_time_to_radar(event_t evt)
{
    UNUSED(evt);
    ble_trace0("scan start\r\n");
    blecen_Scan(LOW_SCAN);
}

static void do_radar_to_time(event_t evt)
{
    UNUSED(evt);
    ble_trace0("scan stop\r\n");
    blecen_Scan(NO_SCAN);
}

static void do_boot_to_idle(event_t evt)
{
    UNUSED(evt);
    UINT8 config_valid, bytes;

#ifdef CHARM_BUILD
    /* This is a compile-time given configuration.  Save to NVM. */
    g_avatar[0] = 0xa + (char) CHARM_BUILD;
    config_valid = 1;
    bleprofile_WriteNVRAM(MUCHI_NVM_AVATAR_CODE, sizeof(g_avatar), (UINT8 *) &g_avatar);
    bleprofile_WriteNVRAM(MUCHI_NVM_CONFIG_IS_VALID, 1, &config_valid);
#endif

    bytes = bleprofile_ReadNVRAM(MUCHI_NVM_CONFIG_IS_VALID, 1, &config_valid);
    if (config_valid) {
        bytes = bleprofile_ReadNVRAM(MUCHI_NVM_AVATAR_CODE, sizeof(g_avatar),
                                                        (UINT8 *) &g_avatar);
        /* we only start adverts if we've found a valid config */
        grpr_start_adverts(0);
    }
    display_off();
}

static void do_idle_to_cfg(event_t evt)
{
    UNUSED(evt);
    display_on();
}

static void do_cfg_to_time(event_t evt)
{
    UNUSED(evt);
    UINT8 config_valid, bytes;

    bytes = bleprofile_ReadNVRAM(MUCHI_NVM_CONFIG_IS_VALID, 1, &config_valid);
    if (config_valid) {
        bytes = bleprofile_ReadNVRAM(MUCHI_NVM_AVATAR_CODE, sizeof(g_avatar),
                                                        (UINT8 *) &g_avatar);
        grpr_start_adverts(0);
    } else {
        ble_trace0("exiting conf state without a valid configuration!\r\n");
    }
}

/*****************************************************************************/
void mainfsm_process_event(enum mainfsm_event type) {
    mainfsm_process_event_with_data(type, NULL);
}

/*****************************************************************************/
void mainfsm_process_event_with_data(enum mainfsm_event type, void *data) {
    event_t evt = { type, data };
    /*ble_trace2("state_table[%d](%d)\r\n", cur_state, evt.type);*/
    state_t new_state = state_table[cur_state](evt);


    transition_func_t *transition = transition_table[cur_state][new_state];

    if (transition) {
        transition(evt);
    }

    if (cur_state != new_state) {
        ble_trace1("M-EVENT: %s\r\n", (INT32) event_name[type]);
        ble_trace2("M-STATE: %s -> %s\r\n", (INT32) state_name[cur_state], (INT32) state_name[new_state]);
    }

    cur_state = new_state;
}

/*****************************************************************************/
state_t mainfsm_get_state()
{
    return cur_state;
}
