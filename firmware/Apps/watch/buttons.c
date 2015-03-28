/* buttons.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */

#include "blecm.h"
#include "blecen.h"
#include "bleprofile.h"
#include "bleapputils.h"
#include "spar_utils.h"
#include "gpiodriver.h"

#include "muchi.h"
#include "groupfsm.h"
#include "mainfsm.h"

#define UNUSED(x) (void)(x)

#define GPIO_ACCEPT_BTN GPIO_PIN_P15
#define GPIO_NEXT_BTN GPIO_PIN_P3

static void buttons_interrupt_handler(void *unused, UINT8 value);
static void accept_button_handler(UINT8 value);
static void next_button_handler(UINT8 value);

void grpr_buttons_configure(void)
{
    UINT16 interrupt_handler_mask[3] = {0, 0, 0};

    /* SW1 - next */
    interrupt_handler_mask[GPIO_NEXT_BTN >> 4] |= (1 << (GPIO_NEXT_BTN & 0xf));

    /* SW3 - accept */
    interrupt_handler_mask[GPIO_ACCEPT_BTN >> 4] |= (1 << (GPIO_ACCEPT_BTN & 0xf));

    gpio_registerForInterrupt(interrupt_handler_mask, buttons_interrupt_handler, NULL);

    /* button pushed drives pin high */
    gpio_configurePin(GPIO_NEXT_BTN >> 4, GPIO_NEXT_BTN & 0xf, GPIO_EN_INT_BOTH_EDGE, 0);

    /* button pushed drives pin high */
    gpio_configurePin(GPIO_ACCEPT_BTN >> 4, GPIO_ACCEPT_BTN & 0xf, GPIO_EN_INT_BOTH_EDGE, 0);
}


static void buttons_interrupt_handler(void *unused, UINT8 value)
{
    UNUSED(unused);

    switch (value) {
        case GPIO_NEXT_BTN:
            next_button_handler(gpio_getPinInput(GPIO_NEXT_BTN >> 4, GPIO_NEXT_BTN & 0xf));
            break;
        case GPIO_ACCEPT_BTN:
            accept_button_handler(gpio_getPinInput(GPIO_ACCEPT_BTN >> 4, GPIO_ACCEPT_BTN & 0xf));
            break;
        default:
            ble_trace1("Unexpected interrupt from pin %x\n", value);
    }
}

static void next_button_handler(UINT8 value)
{
    int accept_button_value = gpio_getPinInput(GPIO_ACCEPT_BTN >> 4, GPIO_ACCEPT_BTN & 0xf);
    if (value) {
        if (accept_button_value)
            mainfsm_process_event(BOTH_HOLD);
        else
            mainfsm_process_event(NEXT_PUSH);
    } else {
            mainfsm_process_event(BOTH_RELEASE);
    }
}

static void accept_button_handler(UINT8 value)
{
    int next_button_value = gpio_getPinInput(GPIO_NEXT_BTN >> 4, GPIO_NEXT_BTN & 0xf);
    if (value) {
        if (next_button_value)
            mainfsm_process_event(BOTH_HOLD);
        else
            mainfsm_process_event(ACCEPT_PUSH);
    } else {
            mainfsm_process_event(BOTH_RELEASE);
    }
}
