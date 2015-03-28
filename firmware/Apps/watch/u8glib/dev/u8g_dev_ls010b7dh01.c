/* Copyright 2012-2014, Peter A. Bigot
 * Copyright 2014, Muchi Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the software nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** @file
 *
 * @brief Device support for u8glib on Sharp ls010b7dh01
 *
 * Use $(U8GLIB_CSRC)/u8g_pb8h1.c with this driver.
 *
 */

#include <u8glib/u8g.h>
#include <spi.h>
#include <string.h>
#include <stddef.h>
#include <spar_utils.h>

#include <muchi.h>

/* Single-pixel U8GLib apparently cannot handle more than 8 rows per
 * page. */
#define ROWS_PER_PAGE 8
#define LS010B7DH01_ROWS 128
#define LS010B7DH01_COLUMNS 128
#define LS010B7DH01_BYTES_PER_LINE ((LS010B7DH01_COLUMNS + 7) / 8)

#define SHARPLCD_MODE_DYNAMIC 0x80
#define SHARPLCD_MODE_COMIN   0x40


/* Single page containing all pixels.  A page is 128 px wide x 8 px high, or 128 bytes */
/* Our display has 16 pages */
static uint8_t cgram_[ROWS_PER_PAGE * LS010B7DH01_BYTES_PER_LINE];

static u8g_pb_t pb = {
    { /* u8g_page_t */
        ROWS_PER_PAGE,
        LS010B7DH01_ROWS,
        0, 0, 0
    },
    LS010B7DH01_COLUMNS,
    cgram_,
};



static uint8_t reverse_bits (uint8_t i)
{
  uint8_t rv;
  rv = i;
  rv = ((rv & 0xaa) >> 1) | ((rv & 0x55) << 1);
  rv = ((rv & 0xcc) >> 2) | ((rv & 0x33) << 2);
  rv = ((rv & 0xf0) >> 4) | ((rv & 0x0f) << 4);
  return rv;
}


static int update_lines (int start_line, int num_lines, const uint8_t * line_data)
{
    int line = start_line;
    int end_line;
    uint8_t cmd[2];
    const uint8_t * dp = line_data;
    static uint8_t comin = 0;

    if (line < 1) {
        return -1;
    }
    if (0 > num_lines) {
        /* From start line to end of device */
        end_line = LS010B7DH01_ROWS + 1 - line;
    } else {
        end_line = start_line + num_lines;
    }

    /* we are doing extcomin toggling in hardware, but does not hurt to do it
     * in software as an extra precaution */
    if (comin) {
        cmd[0] = SHARPLCD_MODE_DYNAMIC | SHARPLCD_MODE_COMIN;
        comin = 0;
    } else {
        cmd[0] = SHARPLCD_MODE_DYNAMIC;
        comin = 1;
    }

    spi_cs_assert(DISPLAY);

    while (line < end_line) {
        cmd[1] = reverse_bits(line);
        __spi_write_buf(cmd, sizeof(cmd));
        __spi_write_buf(dp, LS010B7DH01_BYTES_PER_LINE);
        dp += LS010B7DH01_BYTES_PER_LINE;
        ++line;
        /* Use cmd[0] to insert 8 trailing zeros to indicate multiline */
        /* See Programming Sharp’s Memory LCDs by Ken Green, Figure 5 */
        cmd[0] = 0;
    }

    /* Transmit 16 trailing zero bits to complete multiline */
    cmd[1] = 0;
    __spi_write_buf(cmd, sizeof(cmd));
    spi_cs_deassert(DISPLAY);

    return 0;
}



static uint8_t u8g_com_fn (u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
    uint8_t rc = 0;

    UNUSED(u8g); UNUSED(arg_val); UNUSED(arg_ptr);

    switch (msg) {
        case U8G_COM_MSG_INIT:
            // maybe confirm that spi is initialized?
            break;
        case U8G_COM_MSG_WRITE_SEQ: {
            int start_line = 1 + pb.p.page_y0;
            int num_lines = (pb.p.page_y1 - pb.p.page_y0) + 1;

            rc = ! update_lines(start_line, num_lines, cgram_);
            break;
            }
        /* All these messages below are not needed */
        case U8G_COM_MSG_STOP:
        case U8G_COM_MSG_ADDRESS:
        case U8G_COM_MSG_CHIP_SELECT:
        case U8G_COM_MSG_RESET:
        case U8G_COM_MSG_WRITE_BYTE:
        case U8G_COM_MSG_WRITE_SEQ_P:
        default:
            GRPR_ASSERT(-1);
            break;
        }
    return rc;
}

static uint8_t u8g_dev_fn (u8g_t * u8g, u8g_dev_t * dev, uint8_t msg, void * arg)
{
    UNUSED(u8g); UNUSED(dev); UNUSED(msg); UNUSED(arg);

    int rc = 0;

    switch (msg) {
        default:
            /* Anything not specifically handled is delegated to the base function */
            rc = u8g_dev_pb8h1_base_fn(u8g, dev, msg, arg);
            break;
        case U8G_DEV_MSG_GET_MODE:
            rc = u8g_dev_pb8h1_base_fn(u8g, dev, msg, arg);
            break;
        case U8G_DEV_MSG_INIT: {
            /*LS010B7DH01sharplcdSetEnabled_ni(&dd->device, 1);*/
            memset(cgram_, 0, sizeof(cgram_));
            break;
        }
        case U8G_DEV_MSG_PAGE_NEXT: {
            /* this function just delegates to U8G_COM_MSG_WRITE_SEQ */
            rc = u8g_pb_WriteBuffer(&pb, u8g, dev);
            if (rc) {
                rc = u8g_dev_pb8h1_base_fn(u8g, dev, msg, arg);
            }
            break;
        }
    }
    return rc;
}

u8g_dev_t xLS010B7DH01u8gDevice = { u8g_dev_fn, &pb, u8g_com_fn };
