/*
 * Copyright (c) 2023 @hanyazou
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <xc.h>
#include <stdio.h>
#include "SPI.h"
#include "ch376.h"

#include <picconfig.h>

#define CH376_DEBUG
#if defined(CH376_DEBUG)
#define dprintf(args) do { printf args; } while(0)
#else
#define dprintf(args) do { } while(0)
#endif

#define SPI_PREFIX SPI0

#define CMD_GET_IC_VER      0x01
#define CMD_SET_BAUDRATE    0x02
#define CMD_ENTER_SLEEP     0x03
#define CMD_RESET_ALL       0x05
#define CMD_CHECK_EXIST     0x06
#define CMD_SET_SDO_INT     0x0b
#define CMD_SET_SDO_INT_MAGIC       0x16
#define CMD_SET_SDO_INT_ENABLE      0x90
#define CMD_SET_SDO_INT_DISABLE     0x10
#define CMD_GET_FILE_SIZE   0x0c
#define CMD_SET_USB_MODE    0x15
#define CMD_GET_STATUS      0x22
#define CMD_RD_USB_DATA0    0x27
#define CMD_WR_HOST_DATA    0x2c
#define CMD_WR_REQ_DATA     0x2d
#define CMD_WR_OFS_DATA     0x2e
#define CMD_SET_FILE_NAME   0x2f
#define CMD_DISK_CONNECT    0x30
#define CMD_DISK_MOUNT      0x31
#define CMD_FILE_OPEN       0x32
#define CMD_FILE_ENUM_GO    0x33
#define CMD_FILE_CREATE     0x34
#define CMD_FILE_ERASE      0x35
#define CMD_FILE_CLOSE      0x36
#define CMD_DIR_INFO_READ   0x37
#define CMD_DIR_INFO_SAVE   0x38
#define CMD_BYTE_LOCATE     0x39
#define CMD_BYTE_READ       0x3a
#define CMD_BYTE_RD_GO      0x3b
#define CMD_BYTE_WRITE      0x3c
#define CMD_BYTE_WR_GO      0x3d
#define CMD_DISK_CAPACITY   0x3e
#define CMD_DISK_QUERY      0x3f
#define CMD_DIR_CREATE      0x30
#define CMD_SEC_LOCATE      0x4a
#define CMD_SEC_READ        0x4b
#define CMD_SEC_WRITE       0x3c
#define CMD_DISK_BOC_CMD    0x50
#define CMD_DISK_READ       0x54
#define CMD_DISK_RD_GO      0x55
#define CMD_DISK_WRITE      0x56
#define CMD_DISK_WR_GO      0x57

#define CMD_RET_SUCCESS     0x51
#define CMD_RET_ABORT       0x5f

#define USB_INT_SUCCESS     0x14
#define USB_INT_CONNECT     0x15
#define USB_INT_DISCONNECT  0x16
#define USB_INT_BUF_OVER    0x17
#define USB_INT_USB_READY   0x18
#define USB_INT_DISK_READ   0x1d
#define USB_INT_WRITE       0x1e
#define USB_INT_DISK_ERR    0x1f

static struct CH376 {
    struct SPI *spi;
    uint8_t clock_delay;
} ctx_ = { 0 };
struct CH376 *CH376_ctx = &ctx_;

void ch376_set_sdo_int(struct CH376 *ctx, int enable)
{
    struct SPI *spi = ctx->spi;

    // set SDO pin is always in the output state. When the SCS chip selection is invalid,
    // it serves as the interrupt request output, which is equivalent to the INT# pin for
    // MCU to query the interrupt request status.
    TRISC2 = 1;  // set D2 as input
    uint8_t buf[3];
    buf[0] = CMD_SET_SDO_INT;
    buf[1] = CMD_SET_SDO_INT_MAGIC;
    buf[2] = enable ? CMD_SET_SDO_INT_ENABLE : CMD_SET_SDO_INT_DISABLE;
    SPI(begin_transaction)(spi);
    SPI(send)(spi, buf, 3);
    SPI(end_transaction)(spi);
}

int ch376_init(struct CH376 *ctx, struct SPI *spi, uint16_t clock_delay)
{
    ctx->spi = spi;
    ctx->clock_delay = clock_delay;
    uint8_t status;

    dprintf(("\n\rch376: initialize ...\n\r"));

    SPI(begin)(spi);
    SPI(configure)(spi, clock_delay, SPI_MSBFIRST, SPI_MODE0);

    uint8_t buf[2];
    buf[0] = CMD_GET_IC_VER;
    SPI(begin_transaction)(spi);
    SPI(send)(spi, buf, 1);
    buf[1] = SPI(receive_byte)(spi);
    SPI(end_transaction)(spi);

    dprintf(("ch376: version %02XH\n\r", buf[1]));

    // USB host mode
    dprintf(("ch376: send CMD_SET_USB_MODE host ... \n\r"));
    buf[0] = CMD_SET_USB_MODE;
    buf[1] = 0x06;
    SPI(begin_transaction)(spi);
    SPI(send)(spi, buf, 2);
    __delay_us(10);
    status = SPI(receive_byte)(spi);
    SPI(end_transaction)(spi);
    dprintf(("ch376: send CMD_SET_USB_MODE host, status = %02XH\n\r", status));

    ch376_set_sdo_int(ctx, 1);

 do_connect:
    status = 0xff;
    while (status != USB_INT_SUCCESS) {
        dprintf(("ch376: send CMD_DISK_CONNECT ... \n\r"));
        uint8_t buf[2];
        buf[0] = CMD_DISK_CONNECT;
        SPI(begin_transaction)(spi);
        SPI(send)(spi, buf, 1);
        SPI(end_transaction)(spi);

        for (int i = 0; i < 100 && (PORTC & (1 << 2)); i++) {
            __delay_ms(50);
        }

        buf[0] = CMD_GET_STATUS;
        SPI(begin_transaction)(spi);
        SPI(send)(spi, buf, 1);
        buf[1] = SPI(receive_byte)(spi);
        SPI(end_transaction)(spi);
        // if (status != buf[1])
        {
            dprintf(("ch376: status = %02XH\n\r", buf[1]));
            status = buf[1];
        }
        __delay_ms(500);
    }

#if 1
    status = 0xff;
    while (status != USB_INT_SUCCESS) {
        dprintf(("ch376: CMD_DISK_MOUNT ...\n\r"));
        uint8_t buf[2];
        buf[0] = CMD_DISK_MOUNT;
        SPI(begin_transaction)(spi);
        SPI(send)(spi, buf, 1);
        SPI(end_transaction)(spi);

        for (int i = 0; i < 100 && (PORTC & (1 << 2)); i++) {
            __delay_ms(50);
        }

        buf[0] = CMD_GET_STATUS;
        SPI(begin_transaction)(spi);
        SPI(send)(spi, buf, 1);
        buf[1] = SPI(receive_byte)(spi);
        SPI(end_transaction)(spi);
        // if (status != buf[1])
        {
            dprintf(("ch376: status = %02XH\n\r", buf[1]));
            status = buf[1];
        }
        if (status == USB_INT_DISCONNECT)
            goto do_connect;
    }
#endif

    status = 0xff;
    while (status != USB_INT_SUCCESS) {
        dprintf(("ch376: CMD_DISK_CAPACITY ...\n\r"));
        uint8_t buf[2];
        buf[0] = CMD_DISK_CAPACITY;
        SPI(begin_transaction)(spi);
        SPI(send)(spi, buf, 1);
        SPI(end_transaction)(spi);

        for (int i = 0; i < 100 && (PORTC & (1 << 2)); i++) {
            __delay_ms(50);
        }

        buf[0] = CMD_GET_STATUS;
        SPI(begin_transaction)(spi);
        SPI(send)(spi, buf, 1);
        buf[1] = SPI(receive_byte)(spi);
        SPI(end_transaction)(spi);
        // if (status != buf[1])
        {
            dprintf(("ch376: status = %02XH\n\r", buf[1]));
            status = buf[1];
        }
        if (status == USB_INT_DISCONNECT)
            goto do_connect;
    }

    dprintf(("ch376: CMD_RD_USB_DATA0: "));
    buf[0] = CMD_RD_USB_DATA0;
    SPI(begin_transaction)(spi);
    SPI(send)(spi, buf, 1);
    buf[1] = SPI(receive_byte)(spi);
    uint32_t capacity = 0;
    for (int i = 0; i < buf[1]; i++) {
        capacity |= (((uint32_t)SPI(receive_byte)(spi)) << (8 * i));
    }
    dprintf(("%08lX %ld KB\n\r", capacity, capacity / 2));

    SPI(end_transaction)(spi);

    dprintf(("ch376: initialize ...done\n\r"));

    return 0;
}
