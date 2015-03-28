/* spi.c
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#include "bleprofile.h"
#include "bleapp.h"
#include "muchi.h"
#include "gpiodriver.h"
#include "spiffydriver.h"

#include "spi.h"

/* Use 1MHz speed */
#define SPEED                           1000000

/* CS for all SPI slaves is active low */
#define CS_ASSERT                       1
#define CS_DEASSERT                     0

/* CS for all SPI slaves is on the same port */
#define CS_PORT                         0
#define DISPLAY_CS_PIN                 14
#define ACCEL_CS_PIN                    8

void spi_master_init(enum spi_slave_selector spi_sel)
{
    /* Use SPIFFY2 interface as master */
    spi2PortConfig.masterOrSlave = MASTER2_CONFIG;

    /* pullup for MISO for master */
    spi2PortConfig.pinPullConfig = INPUT_PIN_PULL_UP;

    /* Use P03 for CLK, P02 for MOSI and P01 for MISO */
    spi2PortConfig.spiGpioConfig = MASTER2_P24_CLK_P04_MOSI_P25_MISO;

    /* Initialize SPIFFY2 instance */
    spiffyd_init(SPIFFYD_2);

    /* Configure the SPIFFY2 HW block */
    spiffyd_configure(SPIFFYD_2, SPEED, SPI_MSB_FIRST, SPI_SS_ACTIVE_LOW, SPI_MODE_0);

    /* Configure chip selects */
    if (spi_sel & ACCEL)
        gpio_configurePin(CS_PORT, ACCEL_CS_PIN, GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE,
                                                                            CS_DEASSERT);
    if (spi_sel & DISPLAY)
        gpio_configurePin(CS_PORT, DISPLAY_CS_PIN, GPIO_OUTPUT_ENABLE | GPIO_INPUT_DISABLE,
                                                                            CS_DEASSERT);
}

UINT8 spi_read_reg8(enum spi_slave_selector sel, UINT8 addr)
{
    UINT8 buffer;
    int pin = (sel & DISPLAY) ? DISPLAY_CS_PIN : ACCEL_CS_PIN;

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, pin, CS_ASSERT);     /* Assert chipselect */

    spiffyd_txData(SPIFFYD_2, 1, &addr);
    spiffyd_rxData(SPIFFYD_2, 1, &buffer);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, pin, CS_DEASSERT);   /* Deassert chipselect */

    return buffer;
}

void spi_write_reg8(enum spi_slave_selector sel, UINT8 addr, UINT8 val)
{
    int pin = (sel & DISPLAY) ? DISPLAY_CS_PIN : ACCEL_CS_PIN;

    /* Writing to both in one command not (yet?) supported. */
    GRPR_ASSERT(sel == DISPLAY || sel == ACCEL);

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, pin, CS_ASSERT);     /* Assert chipselect */

    spiffyd_txData(SPIFFYD_2, 1, &addr);
    spiffyd_txData(SPIFFYD_2, 1, &val);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, pin, CS_DEASSERT);   /* Deassert chipselect */

    return;
}

void spi_write_buf(enum spi_slave_selector sel, const UINT8 *buf, UINT8 len)
{
    int pin = (sel & DISPLAY) ? DISPLAY_CS_PIN : ACCEL_CS_PIN;

    /* Writing to both in one command not (yet?) supported. */
    GRPR_ASSERT(sel == DISPLAY || sel == ACCEL);

    /* The spi hw controller on the 20737 supports max 16 byte xfers */
    GRPR_ASSERT(len <= 16);

    /* pull CSB low to start the command */
    gpio_setPinOutput(CS_PORT, pin, CS_ASSERT);     /* Assert chipselect */

    spiffyd_txData(SPIFFYD_2, len, buf);

    /* deassert CSB  to end the command */
    gpio_setPinOutput(CS_PORT, pin, CS_DEASSERT);   /* Deassert chipselect */

    return;
}

void spi_cs_assert(enum spi_slave_selector sel)
{
    int pin = (sel & DISPLAY) ? DISPLAY_CS_PIN : ACCEL_CS_PIN;

    gpio_setPinOutput(CS_PORT, pin, CS_ASSERT);
}

void spi_cs_deassert(enum spi_slave_selector sel)
{
    int pin = (sel & DISPLAY) ? DISPLAY_CS_PIN : ACCEL_CS_PIN;

    gpio_setPinOutput(CS_PORT, pin, CS_DEASSERT);
}

void __spi_write_buf(const UINT8 *buf, UINT8 len)
{
    spiffyd_txData(SPIFFYD_2, len, buf);
    return;
}
