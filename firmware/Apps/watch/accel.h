/*
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#ifndef SPI_H
#define SPI_H

#include "types.h"

void accel_spi_master_init();

void accel_write_reg8(UINT8 addr, UINT8 val);
void accel_write_reg8_1(UINT8 addr, UINT8 val, UINT8 arg);
UINT8 accel_read_reg8(UINT8 addr);
UINT16 accel_read_reg16(UINT8 addr);
#endif /* SPI_H */
