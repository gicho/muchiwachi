/* gestures.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#include "bleapp.h"
#include "gpiodriver.h"
#include "lis3dsh.h"
#include "accel.h"
#include "mainfsm.h"
#include "muchi_iomap.h"
#include "muchi.h"
#include "bleappevent.h"

static void accel_int_handler(void *unused, UINT8 gpio_pin);

void gestures_init()
{
    UINT16 interrupt_handler_mask[3] = {0, 0, 0};

    /* configure accel int1 */
    interrupt_handler_mask[MUCHI_GPIO_ACCEL_INT1 >> 4] |= (1 << (MUCHI_GPIO_ACCEL_INT1 & 0xf));
    interrupt_handler_mask[MUCHI_GPIO_ACCEL_INT2 >> 4] |= (1 << (MUCHI_GPIO_ACCEL_INT2 & 0xf));

    gpio_registerForInterrupt(interrupt_handler_mask, accel_int_handler, NULL);

    gpio_configurePin(MUCHI_GPIO_ACCEL_INT1 >> 4, MUCHI_GPIO_ACCEL_INT1 & 0xf,
        GPIO_EN_INT_RISING_EDGE | GPIO_PULL_DOWN, 0);
    gpio_configurePin(MUCHI_GPIO_ACCEL_INT2 >> 4, MUCHI_GPIO_ACCEL_INT2 & 0xf,
        GPIO_EN_INT_RISING_EDGE | GPIO_PULL_DOWN, 0);

    /* configure spi for accelerator access. */
    accel_spi_master_init();

    /*
     * STATE MACHINE 1
     * Program accelerometer to first look for horizontal, then tilt on X axis.
     */

    /* soft reset */
    accel_write_reg8(LIS3DSH_REG_CTRL_3, 0x01);

    /* INT1 and INT2 enable, active high, interrupt latched */
    accel_write_reg8(LIS3DSH_REG_CTRL_3, 0x58);

    /* 12.5 Hz sampling, enable all three axis */
    accel_write_reg8(LIS3DSH_REG_CTRL_4, 0x37);

    /* 4-wire SPI, 2g full-scale, no filtering */
    accel_write_reg8(LIS3DSH_REG_CTRL_5, 0x0);

    /* Threshold 2: 0.5g  ( = cos(60) * g * 0x7f / 2g) */
    accel_write_reg8(LIS3DSH_REG_THRESHOLD_2_PIN1, 0x20);

    /* Timer4 for double-tilt timeout: 1.04 seconds (divider = 1.04s * 12.5 Hz = 13) */
    accel_write_reg8(LIS3DSH_REG_TIMER4_PIN1, 13);

    /* Timer2 for end-of-radar timeout: 60 seconds (divider = 60s * 12.5 Hz = 750 = 0x2ee) */
    accel_write_reg8(LIS3DSH_REG_TIMER2_PIN1_H, 0x02);
    accel_write_reg8(LIS3DSH_REG_TIMER2_PIN1_L, 0xee);

    /* Mask A on x,y axis */
    accel_write_reg8(LIS3DSH_REG_MASK_A_1, 0xF0);

    /* Mask B on negative y axis */
    accel_write_reg8(LIS3DSH_REG_MASK_B_1, 0x10);

    /* Assert int1 on cont.  Signed thresholds. Always use standard mask */
    accel_write_reg8(LIS3DSH_REG_SETTING_MOTOIN_1, 0x23);

    {  /* state machine program below */

        /* next if: all axes below threshold 2 */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 0, LIS3DSH_COND_LLTH2);

        /* switch mask to only look at y axis */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 1, LIS3DSH_CMD_SELSA);

        /* next if: y larger than threshold */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 2, LIS3DSH_COND_GNTH2);

        /* reset if: timer4 expired  next if: y lower than threshold */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 3,
                                    (LIS3DSH_COND_TI4 << 4) | LIS3DSH_COND_LNTH2);

        /* reset if: timer4 expired  next if: y greater than threshold */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 4,
                                    (LIS3DSH_COND_TI4 << 4) | LIS3DSH_COND_GNTH2);

        /* reset if: timer4 expired  next if: y lower than threshold */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 5,
                                    (LIS3DSH_COND_TI4 << 4) | LIS3DSH_COND_LNTH2);

        /* trigger first INT1 (tilt) */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 6, LIS3DSH_CMD_OUTW);

        /* jump if either of the following 2 conditions is true */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 7, LIS3DSH_CMD_JMP);

        /* cond1: y greater than threshold cond2: timer 3 done. */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 8, (LIS3DSH_COND_TI2 << 4) | LIS3DSH_COND_GNTH2);

        /* jump address is the same for either condition:  (jump_addr << 4) | jump_addr */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 9, 0xaa);

        /* restore mask to look at both axis */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 10, LIS3DSH_CMD_SELMA);

        /* trigger second INT1 and restart */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_1_BASE + 11, LIS3DSH_CMD_CONT);
    }

    /* STATE MACHINE 2 -------------------------------------------------------------
       Turn off display on short inactivity
     */

    /* Timer2 short inactivity: 60 seconds (divider = 60s * 12.5 Hz = 750 = 0x2ee) */
    accel_write_reg8(LIS3DSH_REG_TIMER2_PIN2_H, 0x02);
    accel_write_reg8(LIS3DSH_REG_TIMER2_PIN2_L, 0xee);

    /* Motion detection threshold 1: 0.1 g  ( = 0.1 * g * 0x7f / 2g) */
    accel_write_reg8(LIS3DSH_REG_THRESHOLD_1_PIN2, 0x4);

    /* Mask A and B on all three axis */
    accel_write_reg8(LIS3DSH_REG_MASK_A_2, 0xFC);
    accel_write_reg8(LIS3DSH_REG_MASK_B_2, 0xFC);

    /* Absolute thresholds. Differential mode. Assert int2 on cont. */
    accel_write_reg8(LIS3DSH_REG_SETTING_MOTOIN_2, 0x11);

    {  /* state machine program below */

        /* next if: no motion */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_2_BASE + 0, LIS3DSH_COND_LNTH2);

        /* reset if: motion       next if: timer expires */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_2_BASE + 1, (LIS3DSH_COND_GNTH1 << 4) | LIS3DSH_COND_TI2);

        /* trigger second INT2 and wait for OUTS2 to be read (display off) */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_2_BASE + 2, LIS3DSH_CMD_OUTW);

        /* next if: motion */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_2_BASE + 3, LIS3DSH_COND_GNTH1);

        /* back to top (wake) */
        accel_write_reg8(LIS3DSH_REG_STATE_MACHINE_2_BASE + 4, LIS3DSH_CMD_CONT);
    }

    /* Enable state machine 1, route it to INT1, max hystheresis (0x7) */
    accel_write_reg8(LIS3DSH_REG_CTRL_1, 0xE1);

    /* Enable state machine 2, route it to INT2, no hystheresis (0x0) */
    accel_write_reg8(LIS3DSH_REG_CTRL_2, 0x09);
}


static INT32 serialized_callback(void* context)
{
    (void)context;

    mainfsm_process_event((enum mainfsm_event) context);
    return BLE_APP_EVENT_NO_ACTION;
}

static void accel_int1_handler()
{
    int prog_counter;

    prog_counter = accel_read_reg8(LIS3DSH_REG_PROGRAM_RESET_PIN_1);

    /* Must read this to clear interrupt 1 */
    accel_read_reg8(LIS3DSH_REG_INTERRUPT_MANAGE_PIN_1);

    /*ble_trace1("accel_int1_handler, prog_counter = %d\r\n", prog_counter);*/

    /* prog_counter on OUTW command will be the address of OUTW + 1 */
    if (prog_counter == 7)
        bleappevt_serialize(serialized_callback, (void*)TILT60);
    else
        bleappevt_serialize(serialized_callback, (void*)UNTILT60);
}

static void accel_int2_handler()
{
    UINT8 triggers, prog_counter;

    prog_counter = accel_read_reg8(LIS3DSH_REG_PROGRAM_RESET_PIN_2);

    /* Must read this to clear interrupt 2 and continue state machine execution, if stopped */
    triggers = accel_read_reg8(LIS3DSH_REG_INTERRUPT_MANAGE_PIN_2);

    /*ble_trace2("accel_int2_handler, prog_counter = %d, triggers = %x\r\n", prog_counter, triggers);*/

    /* prog_counter on OUTW command will be the address of OUTW + 1 */
    if (prog_counter == 3) {
        bleappevt_serialize(serialized_callback, (void*)SHORT_INACTIVE);
    } else {
        bleappevt_serialize(serialized_callback, (void*)WAKEUP);
    }
}

/**
 * For some reason fw developers decided not to mangle the gpio_pin before
 * passing it back to the interrupt handler.  I found this info on the forums:
 *  "check interrupt source pin by checking the BITs 7:5 and  4:0 in 2nd
 *   parameter that is passed when interrupt handler is called."
 */
#define HANDLER_ARG_TO_PIN_NUMBER(x) (((x & 0x60) >> 1) | (x & 0xf))

static void accel_int_handler(void *unused, UINT8 gpio_pin)
{
    (void)unused;

    UINT8 pin = HANDLER_ARG_TO_PIN_NUMBER(gpio_pin);
    ble_trace2("accel_int_handler, gpio_pin = %d ==> pin %d\r\n", gpio_pin, pin);
    switch (pin) {
        case (MUCHI_GPIO_ACCEL_INT1):
            accel_int1_handler();
            break;
        case (MUCHI_GPIO_ACCEL_INT2):
            accel_int2_handler();
            break;
        default:
            ble_trace0("Unexpected gpio interrupt\r\n");
    }
}


void accel_debug_interrupts()
{
#if 0
    int val, addr;
    for (addr = LIS3DSH_REG_INFO_1; addr <= LIS3DSH_REG_FIFO_SRC_CTRL; addr++) {
        val = accel_read_reg8(addr);
        ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);
    }
    ble_trace0("== sm 1 ==\r\n");
    for (addr = LIS3DSH_REG_STATE_MACHINE_1_BASE; addr <= LIS3DSH_REG_INTERRUPT_MANAGE_PIN_1; addr++) {
        val = accel_read_reg8(addr);
        ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);
    }

    ble_trace0("== sm 2 ==\r\n");
    for (addr = LIS3DSH_REG_STATE_MACHINE_2_BASE; addr <= LIS3DSH_REG_INTERRUPT_MANAGE_PIN_2; addr++) {
        val = accel_read_reg8(addr);
        ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);
    }
#endif
#if 0
    addr = LIS3DSH_REG_INTERRUPT_STATE;
    val = accel_read_reg8(addr);
    ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);

    addr = LIS3DSH_REG_INTERRUPT_MANAGE_PIN_1;
    val = accel_read_reg8(addr);
    ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);

    addr = LIS3DSH_REG_INTERRUPT_MANAGE_PIN_2;
    val = accel_read_reg8(addr);
    ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);

    addr = LIS3DSH_REG_INTERRUPT_STATE;
    val = accel_read_reg8(addr);
    ble_trace2("accel_debug reg %x = 0x%x\r\n", addr, val);
#endif

    /* After some time the accelerometer will stop sampling for no apparent
     * reason.  The only way to restart normal operation is to power-cycle the
     * accelerometer's measuring block.  This is achieved by the code below.
     */

    ble_trace0("accel off/on\r\n");
    accel_write_reg8(LIS3DSH_REG_CTRL_4, 0x0);
    /* 12.5 Hz sampling, enable all three axis */
    accel_write_reg8(LIS3DSH_REG_CTRL_4, 0x37);
}
