/* muchi.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#ifndef MUCHI_H
#define MUCHI_H

#include "bleapp.h"
#include "blecen.h"
#include "bleprofile.h"

/* following definitions for handles used in the GATT database */
#define HANDLE_MUCHI_SERVICE_UUID                      0x28
#define HANDLE_MUCHI_TIME                              0x2a
#define HANDLE_MUCHI_MESSAGE                           0x2c

#define MUCHI_LOW_ADV_INTERVAL_IN_MS                1000
#define MUCHI_LOW_SCAN_WINDOW_IN_MS                 30
#define MUCHI_LOW_SCAN_INTERVAL_IN_MS               60

#define MUCHI_NODE_EXPIRATION_IN_S                  5

/* Note that UUIDs need to be reversed when publishing in the database */
/* {01ecd400-f26a-11e3-beb2-0002a5d5c51b} */
/* Generated on June 12th 2014 by http://www.itu.int/ITU-T/asn1/cgi-bin/uuid_generate */
#define UUID_MUCHI_SERVICE           0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xb2, 0xbe, 0xe3, 0x11, 0x6a, 0xf2, 0x00, 0xd4, 0xec, 0x01

/* {6f85dfe0-fe20-11e3-ba7a-0002a5d5c51b} */
/* Generated on June 27th 2014 by http://www.itu.int/ITU-T/asn1/cgi-bin/uuid_generate */
#define UUID_MUCHI_TIME   0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0x7a, 0xba, 0xe3, 0x11, 0x20, 0xfe, 0xe0, 0xdf, 0x85, 0x6f

/* {614381e0-00cc-11e4-8ba8-0002a5d5c51b} */
/* Generated on June 30th 2014 by http://www.itu.int/ITU-T/asn1/cgi-bin/uuid_generate */
#define UUID_MUCHI_MESSAGE    			0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xa8, 0x8b, 0xe4, 0x11, 0xcc, 0x00, 0xe0, 0x81, 0x43, 0x61
#define MUCHI_MESSAGE_MAX_LEN           20

#define UNUSED(x) (void)(x)
#define MS_TO_SLOTS(x) ((x * 1000) / 625)

#define GRPR_ASSERT(x) { \
    if (!(x)) \
        ble_trace2("ERROR: Assertion failed in line %d or file %s\r\n", __LINE__, (UINT32)__FILE__); \
} \

enum muchi_state_flags {
    FLAG_GROUPING = 0x1,
};

/* this is the advertising information we need for our service.  Until we get a
 * 32-bit UUID from the Bluetooth SIG things will be crammed.
 */
struct __attribute__((__packed__)) muchi_service_data {
    UINT8 uuid[16];     /* this must be first */
    UINT32 groupid;     /* unused */
    UINT8 state;
    UINT16 msgid;       /* unused */
    UINT16 msg;         /* unused */
    UINT8 avatar;
};

typedef struct t_APP_STATE
{
    UINT32  app_timer_count;
    UINT32  app_fine_timer_count;
} tAPP_STATE;

void grpr_start_adverts(UINT8 flags);
void grpr_stop_adverts();
void grpr_process_adverts(HCIULP_ADV_PACKET_REPORT_WDATA *evt);
void grpr_service_adverts(int count);

void grpr_buttons_configure(void);
void grpr_service_buttons(int count);
UINT32 groupfsm_get_state();

#endif
