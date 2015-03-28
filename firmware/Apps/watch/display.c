/* accel.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */

#include "bleprofile.h"
#include "gpiodriver.h"
#include "rtc.h"

#include "u8glib/u8g.h"
#include "muchi.h"
#include "spi.h"
#include "spar_utils.h"
#include "string.h"
#include "mainfsm.h"
#include "display.h"
#include "store.h"
#include "muchi_iomap.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define BACKLIGHT_ON() \
    do { \
        gpio_setPinOutput((MUCHI_GPIO_BACKLIGHT) >> 4 , MUCHI_GPIO_BACKLIGHT % 16, \
                 GPIO_PIN_OUTPUT_LOW); \
    } while(0);

#define BACKLIGHT_OFF() \
    do { \
        gpio_setPinOutput((MUCHI_GPIO_BACKLIGHT) >> 4 , MUCHI_GPIO_BACKLIGHT % 16, \
                 GPIO_PIN_OUTPUT_HIGH); \
    } while(0);

extern u8g_dev_t xLS010B7DH01u8gDevice;

struct _cp {
    int x;
    int y;
    int len;
};

static int __display_on = 1;

enum cursors { CUR_AVATAR, CUR_OK, CUR_NONE, __MAX_CUR };

static struct _cp cursor_pos[3] = {
    { 24, 38, 16},          /* <-- at this location, cursor is scaled by 2, that's why coords seem off */
    { 46, 30, 34},
    { 0,   0,  0},
};
static int curr_cursor = CUR_AVATAR;
static char msg[4] = ""; char g_avatar[2] = { 56, 0 };

static u8g_t u8g;

static const struct coord {
    int x;
    int y;
} nhour[12] = {
    { 64, 14 }, { 89, 21 }, { 107, 39 }, { 114, 64 }, { 107, 89 }, { 89, 107 }, { 64, 114 }, { 39, 107 }, { 21, 89 }, { 14, 64 }, { 21, 39 }, { 39, 21 },
};

static const struct coord hour[12] = {
    {64, 34}, {79, 38}, {90, 49}, {94, 64}, {90, 79}, {79, 90}, {64, 94}, {49, 90}, {38, 79}, {34, 64}, {38, 49}, {49, 38},
};

/* this array is used to position peers along the circle so that they are as
 * far appart as possible, but without moving peers that are already in the
 * circle.
 */
static int i2idx[10] = { 0, 5, 3, 8, 1, 6, 4, 9, 7, 2 };

static struct coord circle50[10] = { { 64,  14 }, { 93,  24 }, {112,  49 }, {112,  79 }, { 93, 104 }, { 64, 114 }, { 35, 104 }, { 16,  79 }, { 16,  49 }, { 35,  24 }, };

static struct coord circle40[10] = { { 64, 24 }, { 88, 32 }, { 102, 52 }, { 102, 76 }, { 88, 96 }, { 64, 104 }, { 40, 96 }, { 26, 76 }, { 26, 52 }, { 40, 32 }, };

static struct coord circle30[10] = { { 64, 34 }, { 82, 40 }, { 93, 55 }, { 93, 73 }, { 82, 88 }, { 64, 94 }, { 46, 88 }, { 35, 73 }, { 35, 55 }, { 46, 40 }, };

static struct coord circle20[10] = { { 64, 44 }, { 76, 48 }, { 83, 58 }, { 83, 70 }, { 76, 80 }, { 64, 84 }, { 52, 80 }, { 45, 70 }, { 45, 58 }, { 52, 48 }, };

/* rssi -> circle mapping */
#define MAX_RSSI -30
static struct coord* rssi2circ[4] = {
    circle20, /*  MAX_RSSI */
    circle30, /*  MAX_RSSI - 20 dB */
    circle40, /*  MAX_RSSI - 40 dB */
    circle50, /*  MAX_RSSI - 60 dB */
};


static const struct coord minsec[60] = {
    {64, 24}, {68, 24}, {72, 25}, {76, 26}, {80, 27}, {84, 29}, {88, 32}, {91, 34}, {94, 37}, {96, 40}, {99, 44}, {101, 48}, {102, 52}, {103, 56}, {104, 60},
{104, 64}, {104, 68}, {103, 72}, {102, 76}, {101, 80}, {99, 84}, {96, 88}, {94, 91}, {91, 94}, {88, 96}, {84, 99}, {80, 101}, {76, 102}, {72, 103}, {68, 104},
{64, 104}, {60, 104}, {56, 103}, {52, 102}, {48, 101}, {44, 99}, {40, 96}, {37, 94}, {34, 91}, {32, 88}, {29, 84}, {27, 80}, {26, 76}, {25, 72}, {24, 68}, {24, 64},
{24, 60}, {25, 56}, {26, 52}, {27, 48}, {29, 44}, {32, 40}, {34, 37}, {37, 34}, {40, 32}, {44, 29}, {48, 27}, {52, 26}, {56, 25}, {60, 24}, };


static void draw_watch(void * notyet)
{
    unsigned int i, h, w;
    char s[3];

    UNUSED(notyet);
    u8g_SetFont(&u8g, u8g_font_freedoomr10r);
    u8g_SetFontPosCenter(&u8g);

    for (i = 0; i < ARRAY_SIZE(nhour); i++) {
        h = (i == 0) ? 12 : i;
        sprintf(s, "%d", h);
        w = u8g_GetStrWidth(&u8g, s);
        u8g_DrawStr(&u8g, nhour[i].x - w/2, nhour[i].y, s);
    }
}

int display_keypress(grouper_keys_t key)
{
    int avatar_changed = 0;
    switch (key) {
        case KEY_ACCEPT:
            if (curr_cursor == CUR_OK) {
                avatar_changed = 1;
            }
            curr_cursor = (curr_cursor + 1) % ARRAY_SIZE(cursor_pos);
            break;
        case KEY_NEXT:
            if (curr_cursor == CUR_AVATAR) {
                g_avatar[0]++;
                if (g_avatar[0] > '\x52')
                    g_avatar[0] = '\xa';
            }
            if (curr_cursor == CUR_OK) {
                curr_cursor = CUR_AVATAR;
            }
    }

    if (curr_cursor == CUR_OK) {
        sprintf(msg, "OK?");
    } else {
        msg[0] = '\0';
    }

    return avatar_changed;
}

static void draw_hands(int hh, int mm, int ss)
{
    /* draw hour hand */
    u8g_DrawLine(&u8g, 64,64, hour[hh].x, hour[hh].y);

    /* draw minute hand */
    u8g_DrawLine(&u8g, 64,64, minsec[mm].x, minsec[mm].y);

    /* draw second hand */
    u8g_DrawLine(&u8g, 64,64, minsec[ss].x, minsec[ss].y);
    u8g_DrawDisc(&u8g, minsec[ss].x, minsec[ss].y, 3, U8G_DRAW_ALL);
}

static void draw_self()
{
    int w;

    u8g_SetFont(&u8g, u8g_unifont_upper);
    u8g_SetFontPosCenter(&u8g);
    w = u8g_GetStrWidth(&u8g, g_avatar);
    u8g_DrawStr(&u8g, 64 - w/2, 35, g_avatar);
}

static void draw_others()
{
    int w, idx;
    char avatar1[2] = { 58, 0 };
    struct coord *circle;
    INT8 rssi;
    int rssi_idx;

    u8g_SetFont(&u8g, u8g_unifont_upper);
    u8g_SetFontPosCenter(&u8g);
    w = u8g_GetStrWidth(&u8g, g_avatar);
    u8g_DrawStr(&u8g, 64 - w/2, 64, g_avatar);

    idx = grpr_store_get_first();
    for (idx = grpr_store_get_first(); idx != -1; idx = grpr_store_get_next(idx)) {
        if (grpr_store_is_expired(idx))
            continue;
        rssi = grpr_store_get_rssi(idx);
        rssi_idx = -(rssi - MAX_RSSI)/20;
        if (rssi_idx < 0)
            rssi_idx = 0;
        else if (rssi > (INT8) ARRAY_SIZE(rssi2circ) - 1)
            rssi_idx = ARRAY_SIZE(rssi2circ) - 1;
        circle = rssi2circ[rssi_idx];
        avatar1[0] = grpr_store_get_avatar(idx);
        w = u8g_GetStrWidth(&u8g, avatar1);
        u8g_DrawStr(&u8g, circle[i2idx[idx]].x - w/2, circle[i2idx[idx]].y, avatar1);
    }
}

static void draw_message(void *text)
{
    int w, i, lines, lastlen;

#define MAX_NUM_LINES       4
#define INTERLINE_SPACING  16
    char line[MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES + 1];

    lines = (strlen(text) + 1) / (MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES);
    lastlen = (strlen(text) + 1) % (MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES);
    u8g_SetColorIndex(&u8g, 1);
    u8g_SetFont(&u8g, u8g_font_profont22r);
    u8g_SetFontPosCenter(&u8g);
    for (i = 0; i < lines; i++) {
        if (i < lines - 1) {
            memcpy(line, text + i*(MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES), MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES);
            line[MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES] = '\0';
        } else {
            memcpy(line, text + i*(MUCHI_MESSAGE_MAX_LEN/MAX_NUM_LINES), lastlen);
        }

        w = u8g_GetStrWidth(&u8g, line);
        u8g_DrawStr(&u8g, 64 - w/2, 44 + i * INTERLINE_SPACING, line);
    }
    u8g_SetFont(&u8g, u8g_unifont_upper);
    u8g_SetFontPosCenter(&u8g);
    w = u8g_GetStrWidth(&u8g, g_avatar);
    u8g_DrawStr(&u8g, 64 - w/2, 15, g_avatar);
}

static void draw_4letter(void *text)
{
    int w;

    u8g_SetColorIndex(&u8g, 1);
    u8g_SetFont(&u8g, u8g_font_profont22r);
    u8g_SetFontPosCenter(&u8g);
    w = u8g_GetStrWidth(&u8g, text);
    u8g_DrawStr(&u8g, 64 - w/2, 64, text);

    u8g_SetFont(&u8g, u8g_unifont_upper);
    u8g_SetFontPosCenter(&u8g);
    w = u8g_GetStrWidth(&u8g, g_avatar);
    u8g_DrawStr(&u8g, 64 - w/2, 30, g_avatar);
}

static void draw_config()
{
    int w;

    u8g_SetColorIndex(&u8g, 1);
    u8g_DrawDisc(&u8g, 64, 64, 64, U8G_DRAW_ALL);

    u8g_SetColorIndex(&u8g, 0);

    u8g_SetFont(&u8g, u8g_font_profont22r);
    u8g_SetFontPosCenter(&u8g);

    if (strlen(msg) > 0) {
        w = u8g_GetStrWidth(&u8g, msg);
        u8g_DrawStr(&u8g, 64 - w/2, 22, msg);
    }

#define ZOOM_SCALE 2
    u8g_SetScale2x2(&u8g);
    u8g_SetFont(&u8g, u8g_unifont_upper);
    u8g_SetFontPosCenter(&u8g);
    w = u8g_GetStrWidth(&u8g, g_avatar) * ZOOM_SCALE;
    u8g_DrawStr(&u8g, (64 - w/2) / ZOOM_SCALE, 64 / ZOOM_SCALE, g_avatar);

    if (curr_cursor != CUR_AVATAR)
        u8g_UndoScale(&u8g);
    u8g_DrawHLine(&u8g, cursor_pos[curr_cursor].x, cursor_pos[curr_cursor].y,
                                                                cursor_pos[curr_cursor].len);
    if (curr_cursor == CUR_AVATAR)
        u8g_UndoScale(&u8g);
#undef ZOOM_SCALE
}

static void picture_loop(void (draw_fn)(void *), void *arg)
{
    u8g_FirstPage(&u8g);

    do {
        draw_fn(arg);
    } while (u8g_NextPage(&u8g));
}


static void display_text(const char *text)
{
    picture_loop(draw_4letter, (void *) text);
}

static void display_message()
{
    BLEPROFILE_DB_PDU db_pdu;
    bleprofile_ReadHandle(HANDLE_MUCHI_MESSAGE, &db_pdu);
    if (db_pdu.len == 0)
        return;
    GRPR_ASSERT(db_pdu.len < ARRAY_SIZE(db_pdu.pdu));
    db_pdu.pdu[db_pdu.len] = '\0';
    picture_loop(draw_message, db_pdu.pdu);
}

static void display_watch()
{
    RtcTime t;

    rtc_getRTCTime(&t);
    u8g_FirstPage(&u8g);

    do {
        u8g_SetColorIndex(&u8g, 1);
        u8g_DrawBox(&u8g, 0, 0, 128, 128);
        u8g_SetColorIndex(&u8g, 0);
        draw_watch(NULL);
        draw_hands(t.hour % 12, t.minute, t.second);
        draw_self();
    } while (u8g_NextPage(&u8g));
}

static void display_radar()
{
    u8g_FirstPage(&u8g);

    do {
        u8g_SetColorIndex(&u8g, 1);
        u8g_DrawBox(&u8g, 0, 0, 128, 128);
        u8g_SetColorIndex(&u8g, 0);
        draw_others();
    } while (u8g_NextPage(&u8g));
}

static void display_config()
{
    u8g_FirstPage(&u8g);

    do {
        draw_config();
    } while (u8g_NextPage(&u8g));
}

void display_init()
{
    u8g_Init(&u8g, &xLS010B7DH01u8gDevice);

    __display_on = 1;
     gpio_configurePin(MUCHI_GPIO_DISPLAY_5V_ON >> 4 , MUCHI_GPIO_DISPLAY_5V_ON % 16,
             GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE, GPIO_PIN_OUTPUT_HIGH);
     gpio_configurePin((MUCHI_GPIO_DISPLAY_EXTCOMIN) >> 4 , MUCHI_GPIO_DISPLAY_EXTCOMIN % 16,
             GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE, GPIO_PIN_OUTPUT_HIGH);
     gpio_configurePin((MUCHI_GPIO_BACKLIGHT) >> 4 , MUCHI_GPIO_BACKLIGHT % 16, \
                 GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE, GPIO_PIN_OUTPUT_HIGH);
     display_text("MUCHI");
}

void display_off()
{
    gpio_setPinOutput(MUCHI_GPIO_DISPLAY_5V_ON >> 4 , MUCHI_GPIO_DISPLAY_5V_ON % 16, GPIO_PIN_OUTPUT_LOW);
    __display_on = 0;
}

void display_on()
{
#ifndef CHARM_BUILD
    gpio_setPinOutput(MUCHI_GPIO_DISPLAY_5V_ON >> 4 , MUCHI_GPIO_DISPLAY_5V_ON % 16, GPIO_PIN_OUTPUT_HIGH);
    __display_on = 1;
#endif
}

/* Note: This function must be called at least once per second per display requirements */
void display_update()
{
    static int extcomin = GPIO_PIN_OUTPUT_LOW;
    extcomin = (extcomin == GPIO_PIN_OUTPUT_LOW) ? GPIO_PIN_OUTPUT_HIGH : GPIO_PIN_OUTPUT_LOW;

    if (!__display_on)
        return;

    gpio_setPinOutput(MUCHI_GPIO_DISPLAY_EXTCOMIN >> 4, MUCHI_GPIO_DISPLAY_EXTCOMIN % 16, extcomin);

    if (mainfsm_get_state() == MSTATE_TIME) {
        display_watch();
    } else if (mainfsm_get_state() == MSTATE_RADAR) {
        display_radar();
    } else if (mainfsm_get_state() == MSTATE_MESSAGE) {
        display_message();
    } else if (mainfsm_get_state() == MSTATE_CFG) {
        display_config();
    }
}
