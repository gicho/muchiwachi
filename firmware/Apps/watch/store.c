/* store.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#include "blecen.h"
#include "bleprofile.h"
#include "spar_utils.h"
#include "string.h"
#include "rtc.h"

#include "muchi.h"

#include "store.h"
#include "nvram.h"
#define MAX_NODES 100

/* This is a minimal database to keep track of known nodes. */

// TODO: handle wrap-around.
#define IS_EXPIRED(node, now) (node.timestamp + MUCHI_NODE_EXPIRATION_IN_S < now)

extern tAPP_STATE grouper;

#pragma pack(1)
struct __attribute__ ((__packed__)) grouper_node {
    UINT8 addr[WD_ADDRESS_SIZE];
    UINT16 timestamp;
    INT8 rssi;
    INT8 avatar;
};
#pragma pack()

static struct grouper_node nodes[MAX_NODES];
static UINT8 num_nodes;
static int __grpr_store_lookup(UINT8 *addr);

static void load_from_nvram()
{
    int i;
    for (i = 0; i < num_nodes; i += 2) {
        ble_trace2("Loading nodes %d-%d from NVRAM\r\n", i, i+1);
        bleprofile_ReadNVRAM(MUCHI_NVM_STORE_BASE + i/2,
                             2 * sizeof(nodes[0]), (UINT8*) &nodes[i]);
    }
}

static void save_to_nvram()
{
    int i;
    /* storing 2 nodes in each read operation.  Each NVRAM record is 255 bytes,
     * so we could even pack it tighter if needed.  Zeroing the last unused odd
     * node is ok.
     */
    for (i = 0; i < num_nodes; i += 2) {
        ble_trace2("Saving nodes %d-%d to NVRAM\r\n", i, i+1);
        bleprofile_WriteNVRAM(MUCHI_NVM_STORE_BASE + i/2,
                             2 * sizeof(nodes[0]), (UINT8*) &nodes[i]);
    }
    bleprofile_WriteNVRAM(MUCHI_NVM_STORE_COUNT, 1, &num_nodes);
}

void grpr_store_init()
{
    memset(nodes, 0, sizeof(nodes));
    bleprofile_ReadNVRAM(MUCHI_NVM_STORE_COUNT, 1, &num_nodes);
    if (num_nodes)
        load_from_nvram();
}


/*****************************************************************************/
int grpr_store_add(UINT8 *addr)
{
    int i;
    int new_node = 0;

    if (addr == NULL || num_nodes == MAX_NODES)
        return -1;

    i = __grpr_store_lookup(addr);
    if (i < 0) {
        i = num_nodes++;
        new_node = 1;
    }

    grpr_store_timestamp(i);
    memcpy(nodes[i].addr, addr, WD_ADDRESS_SIZE);
    if (new_node)
        save_to_nvram();
    return i;
}

/*****************************************************************************/
int grpr_store_get(UINT8 *addr)
{
    return __grpr_store_lookup(addr);
}

/*****************************************************************************/
int grpr_store_get_first()
{
    if (num_nodes > 0)
        return 0;
    else
        return -1;
}

/*****************************************************************************/
int grpr_store_get_next(int idx)
{
    if (idx + 1 < num_nodes)
        return idx + 1;
    else
        return -1;
}

/*****************************************************************************/
int grpr_store_get_total_expired()
{
    int i, e = 0;
    for (i = 0; i < num_nodes; i++) {
        if (grpr_store_is_expired(i)) {
            e += 1;
        }
    }
    return e;
}

/*****************************************************************************/
int grpr_store_is_expired(int idx)
{
	tRTC_REAL_TIME_CLOCK raw_clock;
	rtc_getRTCRawClock(&raw_clock);
    return IS_EXPIRED(nodes[idx], raw_clock.reg32map.rtc32[0] >> 15);
}

/*****************************************************************************/
int grpr_store_timestamp(int idx)
{
	tRTC_REAL_TIME_CLOCK raw_clock;
    if (idx < 0 || idx >= num_nodes)
        return -1;

	rtc_getRTCRawClock(&raw_clock);
    // We are using a 32768 Hz clock, so every 2^15 cycles is 1 second.
    // 1 second resolution for our timestamp is sufficient.
    nodes[idx].timestamp = raw_clock.reg32map.rtc32[0] >> 15;
    return 0;
}

/*****************************************************************************/
int grpr_store_set_rssi(int idx, INT8 rssi)
{
    if (idx < 0 || idx >= num_nodes)
        return -1;

    nodes[idx].rssi = rssi;
    return 0;
}

/*****************************************************************************/
INT8 grpr_store_get_rssi(int idx)
{
    if (idx < 0 || idx >= num_nodes)
        return -1;

    return nodes[idx].rssi;
}

/*****************************************************************************/
int grpr_store_set_avatar(int idx, INT8 avatar)
{
    if (idx < 0 || idx >= num_nodes)
        return -1;

    nodes[idx].avatar = avatar;
    return 0;
}

/*****************************************************************************/
INT8 grpr_store_get_avatar(int idx)
{
    if (idx < 0 || idx >= num_nodes)
        return -1;

    return nodes[idx].avatar;
}

/*****************************************************************************/
int grpr_store_get_total_count()
{
    return num_nodes;
}

/*****************************************************************************/
int grpr_store_dump()
{
    int i;

    ble_trace0("\r\nStore dump:\r\n");
    for (i = 0; i < num_nodes; i++) {
        UINT8 *a = nodes[i].addr;
        ble_trace6("%x:%x:%x:%x:%x:%x \r\n", *(a+5), *(a+4), *(a+3), *(a+2), *(a+1), *a);
    }
    return 1;
}

static int __grpr_store_lookup(UINT8 *addr)
{
    int i;

    if (addr == NULL)
        return -1;

    for (i = 0; i < num_nodes; i++)
        if (memcmp(&nodes[i].addr, addr, WD_ADDRESS_SIZE) == 0)
            return i;

    return -1;
}
