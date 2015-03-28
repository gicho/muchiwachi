/* adverts.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */

#include "string.h"

#include "blecm.h"
#include "blecen.h"
#include "bleprofile.h"
#include "spar_utils.h"

#include "muchi.h"
#include "groupfsm.h"
#include "store.h"
#include "display.h"


/* private unpublished API from Broadcom */
extern void blecm_setTxPowerInADV(INT32 dbm);

void grpr_stop_adverts()
{
    ble_trace1("%s\r\n", (UINT32) __FUNCTION__);
    bleprofile_Discoverable(NO_DISCOVERABLE, NULL);
}

void grpr_start_adverts(UINT8 flags) 
{
    BLEPROFILE_DB_PDU db_pdu;
    BLE_ADV_FIELD adv[2];
    struct muchi_service_data adv_data;

    ble_trace1("%s\r\n", (UINT32) __FUNCTION__);
    bleprofile_ReadHandle(HANDLE_MUCHI_SERVICE_UUID, &db_pdu);

    GRPR_ASSERT(db_pdu.len == 16);

    // flags
    adv[0].len     = 1 + 1;
    adv[0].val     = ADV_FLAGS;
    adv[0].data[0] = LE_GENERAL_DISCOVERABLE| BR_EDR_NOT_SUPPORTED;

    // service data
    memcpy(adv_data.uuid, db_pdu.pdu, 16);
    adv_data.groupid = 0;
    adv_data.state = flags;
    adv_data.msgid = 0;
    adv_data.msg = 0;
    adv_data.avatar = g_avatar[0];
    adv[1].len      = sizeof(adv_data) + 1;
    adv[1].val      = ADV_SERVICE_DATA_UUID128;
    memcpy(adv[1].data, &adv_data, sizeof(adv_data));

    bleprofile_GenerateADVData(adv, 2);

    blecm_setTxPowerInADV(4);
    bleprofile_Discoverable(LOW_UNDIRECTED_DISCOVERABLE, NULL);
}

void grpr_process_adverts(HCIULP_ADV_PACKET_REPORT_WDATA *evt)
{
    BLE_ADV_FIELD *p_field;
    UINT8         *data = (UINT8 *)(evt->data);
    UINT8         *ptr = data;
    UINT8         dataLen = (UINT8)(evt->dataLen);
    struct muchi_service_data *srvc_data = NULL;

    BLEPROFILE_DB_PDU db_pdu;

    bleprofile_ReadHandle(HANDLE_MUCHI_SERVICE_UUID, &db_pdu);
    GRPR_ASSERT(db_pdu.len == 16);

    while(1) {
        p_field = (BLE_ADV_FIELD *)ptr;

        if ((p_field->len == sizeof(*srvc_data) + 1) &&
            (p_field->val == ADV_SERVICE_DATA_UUID128) &&
            (memcmp (p_field->data, db_pdu.pdu, 16) == 0)) {

            int idx;

            srvc_data = (struct muchi_service_data *) p_field->data;

            ble_trace1("got adv with rssi %d\r\n", evt->rssi);

            if (srvc_data->state & FLAG_GROUPING) {
                groupfsm_process_event_with_data(GROUPING_ADV, evt);
            }

            idx = grpr_store_get(evt->wd_addr);
            if (idx >= 0) {
                grpr_store_timestamp(idx);
                grpr_store_set_rssi(idx, evt->rssi);
                grpr_store_set_avatar(idx, srvc_data->avatar);
            }

            break;
        }

        ptr += (p_field->len + 1);

        if (ptr >= data + dataLen) {
            break;
        }
    }
}
