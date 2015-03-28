/* spi.h
 *
 * Copyright 2014, Muchi Corporation
 * Licensed under GPLv2
 *
 * vim:set et sw=4 ts=4:
 */
#ifndef SPI_H
#define SPI_H

#include "types.h"

enum spi_slave_selector { ACCEL = 0x1, DISPLAY = 0x2};

void spi_master_init(enum spi_slave_selector spi_sel);

void spi_write_reg8(enum spi_slave_selector sel, UINT8 addr, UINT8 val);
UINT8 spi_read_reg8(enum spi_slave_selector sel, UINT8 addr);

void spi_write_buf(enum spi_slave_selector sel, const UINT8 *buf, UINT8 len);

void spi_cs_assert(enum spi_slave_selector sel);
void spi_cs_deassert(enum spi_slave_selector sel);
void __spi_write_buf(const UINT8 *buf, UINT8 len);

#endif /* SPI_H */
