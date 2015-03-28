/* main.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#include "types.h"
#include "bleprofile.h"
#include "blecen.h"
#include "bleapp.h"
#include "gpiodriver.h"
#include "stdio.h"
#include "platform.h"
#include "blecm.h"
#include "devicelpm.h"
#include "spar_utils.h"
#include "string.h"

#include "bleapptimer.h"
#include "emconinfo.h"
#include "leatt.h"
#include "lesmp.h"
#include "blecli.h"
#include "puart.h"
#include "aclk.h"
#include "miadriver.h"
#include "thread_and_mem_mgmt.h"
#include "bleapputils.h"
#include "rtc.h"
#include "additional_advertisement_control.h"

#include "muchi.h"
#include "mainfsm.h"
#include "display.h"
#include "store.h"
#include "spi.h"
#include "gestures.h"
#include "muchi_iomap.h"

const UINT8 muchi_service[16] = {UUID_MUCHI_SERVICE};

/******************************************************
 *                      Constants
 ******************************************************/
#define NVRAM_ID_HOST_LIST              0x10    /* ID of the memory block used for NVRAM access */

#define CONNECT_ANY                     0x01
#define CONNECT_HELLO_SENSOR            0x02
#define SMP_PAIRING                     0x04
#define SMP_ERASE_KEY                   0x08

#define RMULP_CONN_HANDLE_START            0x40
#define MASTER                            0
#define SLAVE                            1

/******************************************************
 *                     Structures
 ******************************************************/
#pragma pack(1)
/*host information for NVRAM */
typedef PACKED struct
{
    /* BD address of the bonded host */
    BD_ADDR  bdaddr;

    /* Current value of the client configuration descriptor */
    UINT16  characteristic_client_configuration;

}  HOSTINFO;
#pragma pack()

/******************************************************
 *               Function Prototypes
 ******************************************************/

static void   muchi_create(void);
static void   muchi_timeout(UINT32 count);
static void   muchi_advertisement_report(HCIULP_ADV_PACKET_REPORT_WDATA *evt);
static void   muchi_connection_up(void);
static void   muchi_connection_down(void);
static void   muchi_advertisement_timeout(void);
static void   muchi_process_rsp(int len, int attr_len, UINT8 *data);
static void   muchi_process_write_rsp();
static int    muchi_write_handler(LEGATTDB_ENTRY_HDR *p);
static void   muchi_timer_callback(UINT32 arg);
static UINT32 lpm_callback(LowPowerModePollType type, UINT32 context);

/* private unpublished API */
extern UINT8 *emconinfo_getPtr(void);
extern INT32 emconinfo_getDiscReason(void);
extern void blecen_connDown(void);
extern void blecen_appTimerCb(UINT32 arg);
extern void blecen_appFineTimerCb(UINT32 arg);
void blecen_leAdvReportCb(HCIULP_ADV_PACKET_REPORT_WDATA *evt);
void blecm_StackCheckInit();

/******************************************************
 *               Variables Definitions
 ******************************************************/
/*
 * This is the GATT database for the Grouper application.  Grouper
 * can connect to another Grouper, but it also provides service for
 * somebody to access.  The database defines services, characteristics and
 * descriptors supported by the application.  Each attribute in the database
 * has a handle, (characteristic has two, one for characteristic itself,
 * another for the value).  The handles are used by the peer to access
 * attributes, and can be used locally by application, for example to retrieve
 * data written by the peer.  Definition of characteristics and descriptors
 * has GATT Properties (read, write, notify...) but also has permissions which
 * identify if peer application is allowed to read or write into it.
 * Handles do not need to be sequential, but need to be in order.
 */
const UINT8 muchi_gatt_database[]=
{
    /* Handle 0x01: GATT service */
    /* Service change characteristic is optional and is not present */
    PRIMARY_SERVICE_UUID16 (0x0001, UUID_SERVICE_GATT),

    /* Handle 0x14: GAP service */
    /* Device Name and Appearance are mandatory characteristics.  Peripheral */
    /* Privacy Flag only required if privacy feature is supported.  Reconnection */
    /* Address is optional and only when privacy feature is supported. */
    /* Peripheral Preferred Connection Parameters characteristic is optional */
    /* and not present. */
    PRIMARY_SERVICE_UUID16 (0x0014, UUID_SERVICE_GAP),

    /* Handle 0x15: characteristic Device Name, handle 0x16 characteristic value. */
    /* Any 16 byte string can be used to identify the sensor.  Just need to */
    /* replace the "Grouper" string below. */
    CHARACTERISTIC_UUID16 (0x0015, 0x0016, UUID_CHARACTERISTIC_DEVICE_NAME,
                           LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 16),
#ifdef CHARM_BUILD
       'M','u','c','h','i','c','h','a','r','m',0x00,0x00,0x00,0x00,0x00,0x00,
#else
       'M','u','c','h','i','w','a','c','h','i',0x00,0x00,0x00,0x00,0x00,0x00,
#endif


    /* Handle 0x17: characteristic Appearance, handle 0x18 characteristic value. */
    /* List of approved appearances is available at bluetooth.org.  Current */
    /* value is set to 0x200 - Generic Tag */
    CHARACTERISTIC_UUID16 (0x0017, 0x0018, UUID_CHARACTERISTIC_APPEARANCE,
                           LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 2),
        BIT16_TO_8(APPEARANCE_GENERIC_TAG),

    /* Handle 0x28: Grouper Service. */
    /* This is the main proprietary service of Grouper application. */
    PRIMARY_SERVICE_UUID128 (HANDLE_MUCHI_SERVICE_UUID, UUID_MUCHI_SERVICE),

    /* Handle 0x29: This macro call defines at once the characteristic */
    /* declaration (handle 0x29) and the characteristic value (handle */
    /* HANDLE_MUCHI_TIME = 0x2a). */
    CHARACTERISTIC_UUID128_WRITABLE (HANDLE_MUCHI_TIME - 1, HANDLE_MUCHI_TIME, UUID_MUCHI_TIME,
            LEGATTDB_CHAR_PROP_READ | LEGATTDB_CHAR_PROP_WRITE,
            LEGATTDB_PERM_READABLE  | LEGATTDB_PERM_WRITE_REQ , 3),
        0x00, 0x00, 0x00,   /* hh, mm, ss */

    /* Handle 0x2b: This macro call defines at once the characteristic */
    /* declaration (handle 0x2b) and the characteristic value (handle */
    /* HANDLE_MUCHI_MESSAGE = 0x2c). */
    CHARACTERISTIC_UUID128_WRITABLE (HANDLE_MUCHI_MESSAGE - 1, HANDLE_MUCHI_MESSAGE, UUID_MUCHI_MESSAGE,
            LEGATTDB_CHAR_PROP_READ | LEGATTDB_CHAR_PROP_WRITE,
            LEGATTDB_PERM_READABLE  | LEGATTDB_PERM_WRITE_REQ , MUCHI_MESSAGE_MAX_LEN),
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
       0x00,0x00,0x00,0x00,

    /* Handle 0x4d: Device Info service */
    /* Device Information service helps peer to identify manufacture or vendor */
    /* of the device.  It is required for some types of the devices (for example HID, */
    /* and medical, and optional for others.  There are a bunch of characteristics */
    /* available, out of which Hello Sensor implements 3. */
    PRIMARY_SERVICE_UUID16 (0x004d, UUID_SERVICE_DEVICE_INFORMATION),

    /* Handle 0x4e: characteristic Manufacturer Name, handle 0x4f characteristic value */
    CHARACTERISTIC_UUID16 (0x004e, 0x004f, UUID_CHARACTERISTIC_MANUFACTURER_NAME_STRING, LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 8),
        'M','u','c','h','i',0x00,0x00,0x00,

    /* Handle 0x50: characteristic Model Number, handle 0x51 characteristic value */
    CHARACTERISTIC_UUID16 (0x0050, 0x0051, UUID_CHARACTERISTIC_MODEL_NUMBER_STRING, LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 8),
        '4','3','2','1',0x00,0x00,0x00,0x00,

    /* Handle 0x52: characteristic System ID, handle 0x53 characteristic value */
    CHARACTERISTIC_UUID16 (0x0052, 0x0053, UUID_CHARACTERISTIC_SYSTEM_ID, LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 8),
        0xef,0x48,0xa2,0x32,0x17,0xc6,0xa6,0xbc,

    /* Handle 0x61: Battery service */
    /* This is an optional service which allows peer to read current battery level. */
    PRIMARY_SERVICE_UUID16 (0x0061, UUID_SERVICE_BATTERY),

    /* Handle 0x62: characteristic Battery Level, handle 0x63 characteristic value */
    CHARACTERISTIC_UUID16 (0x0062, 0x0063, UUID_CHARACTERISTIC_BATTERY_LEVEL,
                           LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 1),
        0x64,
};

const BLE_PROFILE_CFG muchi_cfg =
{
    .fine_timer_interval            = 1000, /* ms */
    .default_adv                    = NO_DISCOVERABLE,
    .button_adv_toggle              = 0,    /* pairing button make adv toggle (if 1) or always on (if 0) */
    .high_undirect_adv_interval     = 32,   /* 32 slots = 20 ms, or the shortest adv interval allowed by the BT spec */
    .low_undirect_adv_interval      = MS_TO_SLOTS(MUCHI_LOW_ADV_INTERVAL_IN_MS),
    .high_undirect_adv_duration     = 30,   /* seconds */
    .low_undirect_adv_duration      = 300,  /* seconds */
    .high_direct_adv_interval       = 0,    /* seconds */
    .low_direct_adv_interval        = 0,    /* seconds */
    .high_direct_adv_duration       = 0,    /* seconds */
    .low_direct_adv_duration        = 0,    /* seconds */
#ifdef CHARM_BUILD
    .local_name                     = "Muchicharm", /* [LOCAL_NAME_LEN_MAX]; */
#else
    .local_name                     = "Muchiwachi", /* [LOCAL_NAME_LEN_MAX]; */
#endif
    .cod                            = {BIT16_TO_8(APPEARANCE_GENERIC_TAG), 0x00}, /* [COD_LEN]; */
    .ver                            = "1.00",         /* [VERSION_LEN]; */
    .encr_required                  = 0, /* (SECURITY_ENABLED | SECURITY_REQUEST),    // data encrypted and device sends security request on every connection */
    .disc_required                  = 0,    /* if 1, disconnection after confirmation */
    .test_enable                    = 1,    /* TEST MODE is enabled when 1 */
    .tx_power_level                 = 0,    /* dbm */
    .con_idle_timeout               = 0,    /* second  0-> no timeout */
    .powersave_timeout              = 1,    /* second  0-> no timeout */
    .hdl                            = {0x00, 0x00, 0x00, 0x00, 0x00}, /* [HANDLE_NUM_MAX]; */
    .serv                           = {0x00, 0x00, 0x00, 0x00, 0x00},
    .cha                            = {0x00, 0x00, 0x00, 0x00, 0x00},
    .findme_locator_enable          = 0,    /* if 1 Find me locator is enable */
    .findme_alert_level             = 0,    /* alert level of find me */
    .client_grouptype_enable        = 1,    /* if 1 grouptype read can be used */
    .linkloss_button_enable         = 0,    /* if 1 linkloss button is enable */
    .pathloss_check_interval        = 0,    /* second */
    .alert_interval                 = 0,    /* interval of alert */
    .high_alert_num                 = 0,    /* number of alert for each interval */
    .mild_alert_num                 = 0,    /* number of alert for each interval */
    .status_led_enable              = 1,    /* if 1 status LED is enable */
    .status_led_interval            = 0,    /* second */
    .status_led_con_blink           = 0,    /* blink num of connection */
    .status_led_dir_adv_blink       = 0,    /* blink num of dir adv */
    .status_led_un_adv_blink        = 0,    /* blink num of undir adv */
    .led_on_ms                      = 0,    /* led blink on duration in ms */
    .led_off_ms                     = 0,    /* led blink off duration in ms */
    .buz_on_ms                      = 100,  /* buzzer on duration in ms */
    .button_power_timeout           = 0,    /* seconds */
    .button_client_timeout          = 1,    /* seconds */
    .button_discover_timeout        = 3,    /* seconds */
    .button_filter_timeout          = 10,   /* seconds */
#ifdef BLE_UART_LOOPBACK_TRACE
    .button_uart_timeout            = 15,   /* seconds */
#endif
};

/* Following structure defines UART configuration */
const BLE_PROFILE_PUART_CFG muchi_puart_cfg =
{
    /*.baudrate   =*/ 115200,
    /*.txpin      =*/ GPIO_PIN_UART_TX | PUARTDISABLE,
    /*.rxpin      =*/ GPIO_PIN_UART_RX | PUARTDISABLE,
};

/* Following structure defines GPIO configuration used by the application */
static const BLE_PROFILE_GPIO_CFG muchi_gpio_cfg =
{
    /*.gpio_pin =*/
    {
        GPIO_PIN_WP, /* This need to be used to enable/disable NVRAM write protect */
        MUCHI_GPIO_BAT_MONITOR,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 /* others not used */
    },
    /*.gpio_flag =*/
    {
        GPIO_SETTINGS_WP,
        GPIO_SETTINGS_BATTERY,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }
};

tAPP_STATE muchi;

/* Following variables are in ROM */
extern BLE_CEN_CFG        blecen_cen_cfg;
extern BLEAPP_TIMER_CB    blecen_usertimerCb;

/******************************************************
 *               Function Definitions
 ******************************************************/

/* Application initialization */
APPLICATION_INIT()
{
    bleapp_set_cfg((UINT8 *)muchi_gatt_database,
                   sizeof(muchi_gatt_database),
                   (void *)&muchi_cfg,
                   (void *)&muchi_puart_cfg,
                   (void *)&muchi_gpio_cfg,
                   muchi_create);
}

/* Create muchi */
void muchi_create(void)
{

    ble_trace0("muchi_create()\n");
    ble_trace0(bleprofile_p_cfg->ver);

    /* This should turn on the code in ROM that updates the battery information characteristic. */
    /* It depends on having the battery gpio correctly configured in muchi_gpio_cfg as well as */
    /* UUID_SERVICE_BATTERY defined in the GATT database. */
    blebat_Init();

    /* dump the database to debug uart. */
    legattdb_dumpDb();

    /* BLE Central default parameters.  Change if appropriate */
    /*blecen_cen_cfg.scan_type                = HCIULP_ACTIVE_SCAN; */
    /*blecen_cen_cfg.scan_adr_type            = HCIULP_PUBLIC_ADDRESS; */
    /*blecen_cen_cfg.scan_filter_policy       = HCIULP_SCAN_FILTER_POLICY_WHITE_LIST_NOT_USED; */
    /*blecen_cen_cfg.filter_duplicates        = HCIULP_SCAN_DUPLICATE_FILTER_ON; */
    /*blecen_cen_cfg.init_filter_policy       = HCIULP_INITIATOR_FILTER_POLICY_WHITE_LIST_NOT_USED; */
    /*blecen_cen_cfg.init_addr_type           = HCIULP_PUBLIC_ADDRESS; */
    /*blecen_cen_cfg.high_scan_interval       = 96;       // slots (slot = 625 usecs => 60 ms) */
    /*blecen_cen_cfg.low_scan_interval        = 2048;     // slots */
    /*blecen_cen_cfg.high_scan_window         = 48;       // slots (30 ms => 50% duty cycle) */
    /*blecen_cen_cfg.low_scan_window          = 18;       // slots */
    /*blecen_cen_cfg.high_scan_duration       = 30;       // seconds */
    /*blecen_cen_cfg.low_scan_duration        = 300;      // seconds */
    /*blecen_cen_cfg.high_conn_min_interval   = 40;       // frames */
    /*blecen_cen_cfg.low_conn_min_interval    = 400;      // frames */
    /*blecen_cen_cfg.high_conn_max_interval   = 56;       // frames */
    /*blecen_cen_cfg.low_conn_max_interval    = 560;      // frames */
    /*blecen_cen_cfg.high_conn_latency        = 0;        // number of connection event */
    /*blecen_cen_cfg.low_conn_latency         = 0;        // number of connection event */
    /*blecen_cen_cfg.high_supervision_timeout = 10;       // N * 10ms */
    /*blecen_cen_cfg.low_supervision_timeout  = 100;      // N * 10ms */
    /*blecen_cen_cfg.conn_min_event_len       = 0;        // slots */
    /*blecen_cen_cfg.conn_max_event_len       = 0;        // slots */

    /* BLE Central changed parameter */
    blecen_cen_cfg.scan_type                  = HCIULP_PASSIVE_SCAN;
    blecen_cen_cfg.filter_duplicates          = HCIULP_SCAN_DUPLICATE_FILTER_OFF;
    blecen_cen_cfg.low_scan_interval          = MS_TO_SLOTS(MUCHI_LOW_SCAN_INTERVAL_IN_MS);
    blecen_cen_cfg.low_scan_window            = MS_TO_SLOTS(MUCHI_LOW_SCAN_WINDOW_IN_MS);
    blecen_usertimerCb                        = muchi_timer_callback;
    blecen_cen_cfg.high_supervision_timeout   = 400;      /* N * 10ms */
    blecen_cen_cfg.low_supervision_timeout    = 700;      /* N * 10ms */
    /*
     * this next parameter affects not just the scan time limit but also how
     * long to wait until a connection attempt is considered to have failed.
     */
    blecen_cen_cfg.high_scan_duration         = 10;       /* seconds */

    blecen_Create();
    blecen_Scan(NO_SCAN);

    /* register connection up and connection down handler. */
    bleprofile_regAppEvtHandler(BLECM_APP_EVT_LINK_UP, muchi_connection_up);
    bleprofile_regAppEvtHandler(BLECM_APP_EVT_LINK_DOWN, muchi_connection_down);
    bleprofile_regAppEvtHandler(BLECM_APP_EVT_ADV_TIMEOUT, muchi_advertisement_timeout);

    /* register to process peripheral advertisements, notifications and indications */
    blecm_RegleAdvReportCb((BLECM_FUNC_WITH_PARAM) muchi_advertisement_report);

    /* GATT client callbacks */
    leatt_regReadRspCb((LEATT_TRIPLE_PARAM_CB) muchi_process_rsp);
    leatt_regReadByTypeRspCb((LEATT_TRIPLE_PARAM_CB) muchi_process_rsp);
    leatt_regReadByGroupTypeRspCb((LEATT_TRIPLE_PARAM_CB) muchi_process_rsp);
    leatt_regWriteRspCb((LEATT_NO_PARAM_CB) muchi_process_write_rsp);

    /* register to process client writes */
    legattdb_regWriteHandleCb((LEGATTDB_WRITE_CB)muchi_write_handler);

    /* configure buttons and leds */
    grpr_buttons_configure();

    /* Disable GPIOs used by the 32K external LPO. */
    gpio_configurePin(0, 10, GPIO_INPUT_DISABLE, 0);
    gpio_configurePin(0, 11, GPIO_INPUT_DISABLE, 0);
    gpio_configurePin(0, 12, GPIO_INPUT_DISABLE, 0);

    /* Initialize real-time clock */
    rtcConfig.oscillatorFrequencykHz = RTC_REF_CLOCK_SRC_32KHZ;
    /*rtcConfig.oscillatorFrequencykHz = RTC_REF_CLOCK_SRC_128KHZ;*/
    rtc_init();

    /* change timer callback function.  because we are running ROM app, need to */
    /* stop timer first. */
    bleprofile_KillTimer();
    bleprofile_regTimerCb(muchi_timeout, blecen_appTimerCb);
    bleprofile_StartTimer();

    /* Switch to the external 32K clock for better accuracy */
    bleapputils_changeLPOSource(LPO_32KHZ_OSC, FALSE, 250);

    /* XXX: display and accel have conflicting SPI configurations.  For now just program accel, */
    /* then reconfigure SPI for display.  So order of next two instructions is significant. */
    gestures_init();

    /* initialize spi for accel and display */
    spi_master_init(DISPLAY);

    /* configure display */
    display_init();

    /* turn off LRA driver */
    gpio_configurePin(MUCHI_GPIO_LRA_ENABLE / 16, MUCHI_GPIO_LRA_ENABLE % 16, GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE , 0);

    /* initialize store from NVRAM */
    grpr_store_init();

    mainfsm_process_event(BOOT_DONE);

    /* Power management */
    devlpm_registerForLowPowerQueries(lpm_callback, 0);
}


/* This function will be called on every connection establishmen */
void muchi_connection_up(void)
{
    /*UINT8 *p_remote_pub_addr = (UINT8 *)emconninfo_getPeerPubAddr();*/

    /* This ugliness is required to tell blecen to stop the connection timer */
    blecen_Conn(NO_CONN, NULL, 0);
}

/* This function will be called when connection goes down */
void muchi_connection_down(void)
{
    /* This ugliness is required to tell blecen to stop the connection timer */
    blecen_Conn(NO_CONN, NULL, 0);
}

extern void accel_debug_interrupts();

static void muchi_timeout(UINT32 type)
{
    UNUSED(type);
    BLEPROFILE_DB_PDU db_pdu;
    RtcTime timebuf;

    rtc_getRTCTime(&timebuf);
    db_pdu.len = 3;
    db_pdu.pdu[0] = timebuf.hour & 0xff;
    db_pdu.pdu[1] = timebuf.minute & 0xff;
    db_pdu.pdu[2] = timebuf.second & 0xff;
    bleprofile_WriteHandle(HANDLE_MUCHI_TIME, &db_pdu);
    ble_trace3("Time is %d:%d:%d\r\n",  timebuf.hour, timebuf.minute, timebuf.second);
    mainfsm_process_event(TICK_1S);

    /* XXX: Workaround for the frozen accel problem: toggle the power on the sensing block */
    /*  once per second. */
    accel_debug_interrupts();
}

void muchi_timer_callback(UINT32 arg)
{
    switch(arg)
    {
    case BLEAPP_APP_TIMER_SCAN:
        ble_trace0("\r\nScan timed out\n");
        break;

    case BLEAPP_APP_TIMER_CONN:
        ble_trace0("\r\nConnection Failed\n");
        break;
    }
}

void muchi_advertisement_timeout(void)
{
    /* XXX: generate an event for main fsm */
    grpr_start_adverts(0);
}

void muchi_advertisement_report(HCIULP_ADV_PACKET_REPORT_WDATA *evt)
{
    grpr_process_adverts(evt);
}


void muchi_process_rsp(int len, int attr_len, UINT8 *data)
{
    UNUSED(attr_len);
    UNUSED(data);
    UNUSED(len);
}

void muchi_process_write_rsp(void)
{
}

/* */
/* Process write request or command from peer device */
/* */
int muchi_write_handler(LEGATTDB_ENTRY_HDR *p)
{
    UINT16 handle  = legattdb_getHandle(p);
    int len        = legattdb_getAttrValueLen(p);
    UINT8 *attrPtr = legattdb_getAttrValue(p);
    RtcTime current_time;
    char buffer[30];


    if (handle == HANDLE_MUCHI_TIME && len == 3) {
        /* Date not kept (yet?) so provide valid values to rtc */
        current_time.year = 2014;
        current_time.month = 0; /* jan */
        current_time.day = 1; /* 1st */

        current_time.hour = attrPtr[0] % 24;
        current_time.minute = attrPtr[1] % 60;
        current_time.second = attrPtr[2] % 60;
        if(rtc_setRTCTime(&current_time))
        {
            memset(buffer, 0x00, sizeof(buffer));

            ble_trace0("Current time set to:\r\n");
            rtc_ctime(&current_time, buffer);
            ble_trace0(buffer);
        }
    } else if (handle == HANDLE_MUCHI_MESSAGE && len > 0) {
        ble_trace1("Message received of len: %d\r\n", len);
        mainfsm_process_event(MESSAGE_ARRIVED);
    }

    return 0;
}

static UINT32 lpm_callback(LowPowerModePollType type, UINT32 context)
{
    UNUSED(context);
    if (type == LOW_POWER_MODE_POLL_TYPE_POWER_OFF)
        return 0;
    else /* type == LOW_POWER_MODE_POLL_TYPE_SLEEP */
        return ~0;
}
