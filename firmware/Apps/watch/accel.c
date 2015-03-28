/* accel.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */


#include "string.h"
#include "spar_utils.h"
#include "bleprofile.h"
#include "bleapp.h"
#include "gpiodriver.h"
#include "spiffydriver.h"

#include "accel.h"
#include "lis3dsh.h"

/* Use 1 MHz for SPI speed */
#define SPEED                           1000000

#define CS_ASSERT                       0
#define CS_DEASSERT                     1

/* CS for all SPI slaves is on the same port */
#define CS_PORT                         0
#define ACCEL_CS_PIN                    8

void accel_spi_master_init()
{
    /* Use SPIFFY2 interface as master */
    spi2PortConfig.masterOrSlave = MASTER2_CONFIG;

    /* pullup for MISO for master */
    spi2PortConfig.pinPullConfig = INPUT_PIN_PULL_UP;

    /* Use P24 for CLK, P04 for MOSI and P25 for MISO */
    spi2PortConfig.spiGpioConfig = MASTER2_P24_CLK_P04_MOSI_P25_MISO;

    /* Initialize SPIFFY2 instance */
    spiffyd_init(SPIFFYD_2);

    /* Configure the SPIFFY2 HW block */
    spiffyd_configure(SPIFFYD_2, SPEED, SPI_MSB_FIRST, SPI_SS_ACTIVE_LOW, SPI_MODE_0);

    /* Configure chip select */
    gpio_configurePin(CS_PORT, ACCEL_CS_PIN, GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE,
                                                                            CS_DEASSERT);
}

UINT8 accel_read_reg8(UINT8 addr)
{
    UINT8 buffer;

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_ASSERT);     /* Assert chipselect */

    addr |= LIS3DSH_READ_FLAG;
    spiffyd_txData(SPIFFYD_2, 1, &addr);
    spiffyd_rxData(SPIFFYD_2, 1, &buffer);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_DEASSERT);   /* Deassert chipselect */

    return buffer;
}

void accel_write_reg8(UINT8 addr, UINT8 val)
{

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_ASSERT);     /* Assert chipselect */

    spiffyd_txData(SPIFFYD_2, 1, &addr);
    spiffyd_txData(SPIFFYD_2, 1, &val);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_DEASSERT);   /* Deassert chipselect */

#if 0
    {
        UINT8 rval;
        rval = accel_read_reg8(addr);
        ble_trace3("ACCEL READBACK: %x = %x", addr, rval, val);
    }
#endif

    return;
}

void accel_write_reg8_1(UINT8 addr, UINT8 val, UINT8 arg)
{
    UINT8 buf[2];

    buf[0] = val;
    buf[1] = arg;

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_ASSERT);     /* Assert chipselect */

    spiffyd_txData(SPIFFYD_2, 1, &addr);
    spiffyd_txData(SPIFFYD_2, 2, buf);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_DEASSERT);   /* Deassert chipselect */

    return;
}

UINT16 accel_read_reg16(UINT8 addr)
{
    UINT8 buffer[2];

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_ASSERT);     /* Assert chipselect */

    addr |= LIS3DSH_READ_FLAG;
    spiffyd_txData(SPIFFYD_2, 1, &addr);
    spiffyd_rxData(SPIFFYD_2, 2, buffer);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, ACCEL_CS_PIN, CS_DEASSERT);   /* Deassert chipselect */

    return (buffer[1] << 8) | buffer[0];
}
