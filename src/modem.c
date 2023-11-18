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

#include <supermez80.h>
#include <stdio.h>
#include <string.h>
#include <modem_xfer.h>
#include <stdarg.h>
#include <utils.h>

uint8_t *modem_buf = NULL;
uint8_t modem_buf_ofs = 0;
uint8_t modem_buf_len = 0;

static uint8_t modem_in_use = 0;
static uint8_t modem_on_line = 0;
static uint8_t modem_receiving = 0;
static FIL *filep = NULL;
static int raw;
static uint8_t set_key_input = 0;
static ymodem_context ctx;

#define MAX_FILE_NAME_LEN 13
#ifdef DEBUG
#define msgbuf_size 1200
#else
#define msgbuf_size 256
#endif
#define msgtmpbuf_size 128
static char save_file_name[MAX_FILE_NAME_LEN] = { 0 };
static char *msgbuf = NULL;
static unsigned int msglen = 0;
static char *msgtmpbuf = NULL;

static int __modem_open(void)
{
    if (modem_in_use) {
        printf("%s: medem is busy\n\r", __func__);
        return -1;
    }

    modem_in_use = 1;

    #if defined(DEBUG)
    static uint8_t tmp[msgbuf_size];
    msgbuf = (char*)tmp;
    #else
    msgbuf = util_memalloc(msgbuf_size);
    #endif
    msgtmpbuf = util_memalloc(msgtmpbuf_size);
    modem_buf = util_memalloc(MODEM_XFER_BUF_SIZE);
    if (msgbuf == NULL || msgtmpbuf == NULL || modem_buf == NULL) {
        modem_close();
        printf("%s: memory allocation failed\n\r", __func__);
        return -1;
    }

    raw = set_key_input_raw(1);
    set_key_input = 1;

    return 0;
}

int modem_send_open(char *file_name, uint32_t size)
{
    int res;

    res = __modem_open();
    if (res != 0) {
        return res;
    }

    modem_receiving = 0;
    ymodem_send_init(&ctx, modem_buf);
    res = ymodem_send_header(&ctx, file_name, size);
    if (res != MODEM_XFER_RES_OK) {
        modem_xfer_printf(MODEM_XFER_LOG_ERROR, "ymodem_send_header() failed, %d\n\r", res);
        modem_close();
        printf("%s(%d): failed\n\r", __func__, __LINE__);
        return -1;
    }
    modem_on_line = 1;
    modem_buf_ofs = 0;

    return 0;
}

int modem_recv_open(void)
{
    int res;

    res = __modem_open();
    if (res != 0) {
        return res;
    }

    filep = get_file();
    if (filep == NULL) {
        modem_close();
        printf("too many files\n\r");
        return -1;
    }

    modem_receiving = 1;
    ymodem_receive_init(&ctx, modem_buf);
    modem_on_line = 1;
    modem_buf_ofs = 0;
    modem_buf_len = (uint8_t)n;

    return 0;
}

int modem_send(void)
{
    int res;

    res = ymodem_send_block(&ctx);
    if (res != MODEM_XFER_RES_OK) {
        modem_xfer_printf(MODEM_XFER_LOG_ERROR, "ymodem_send_block() failed, %d\n\r", res);
        modem_on_line = 0;
        return -1;
    }

    return 0;
}

int modem_write(uint8_t *buf, unsigned int len)
{
    unsigned int n;
    int res;

    if (!modem_on_line) {
        return -1;
    }

    res = 0;
    while (0 < len) {
        n = UTIL_MIN(MODEM_XFER_BUF_SIZE - modem_buf_ofs, len);
        memcpy(&modem_buf[modem_buf_ofs], buf, n);
        modem_buf_ofs += n;
        if (modem_buf_ofs == MODEM_XFER_BUF_SIZE) {
            if (modem_send() != 0) {
                if (0 < res) {
                    return res;
                } else {
                    return -1;
                }
            }
            modem_buf_ofs = 0;
        }
        len -= n;
        res += n;
    }

    return res;
}

int modem_recv_to_save(void)
{
    int res;
    unsigned int n;

    while ((res = ymodem_receive_block(&ctx, &n)) == MODEM_XFER_RES_OK) {
        if (ctx.file_name[0] == '\0') {
            return MODEM_XFER_RES_OK;
        }
        res = modem_xfer_save(ctx.file_name, ctx.file_offset, ctx.buf, n);
        if (res != MODEM_XFER_RES_OK) {
            ymodem_send_cancel(&ctx);
            return res;
        }
    }
    modem_on_line = 0;

    return res;
}

int modem_read(uint8_t *buf, unsigned int len)
{
    unsigned int n;
    int res;

    if (!modem_on_line) {
        return -1;
    }

    res = 0;
    while (0 < len) {
        n = UTIL_MIN(modem_buf_len - modem_buf_ofs, len);
        memcpy(buf, &modem_buf[modem_buf_ofs], n);
        modem_buf_ofs += n;
        len -= n;
        res += n;
        if (modem_buf_ofs == modem_buf_len) {
            if (ymodem_receive_block(&ctx, &n) != MODEM_XFER_RES_OK) {
                modem_close();
                printf("ymodem_receive_block() failed\n\r");
                if (0 < res) {
                    return res;
                } else {
                    return -1;
                }
            }
            modem_buf_ofs = 0;
            modem_buf_len = (uint8_t)n;
        }
    }

    return res;
}

void modem_cancel(void)
{
    if (modem_on_line) {
        ymodem_send_cancel(&ctx);
        modem_on_line = 0;
    }
}

void modem_close(void)
{
    int res;

    // flush
    if (0 < modem_buf_ofs) {
        if (!modem_receiving && modem_on_line && modem_buf != NULL) {
            memset(&modem_buf[modem_buf_ofs], 0x00, MODEM_XFER_BUF_SIZE - modem_buf_ofs);
            modem_send();
        }
        modem_buf_ofs = 0;
    }

    // hungup
    if (modem_on_line) {
        if (modem_receiving) {
            ymodem_send_cancel(&ctx);
        } else {
            res = ymodem_send_end(&ctx);
            if (res != MODEM_XFER_RES_OK) {
                modem_xfer_printf(MODEM_XFER_LOG_ERROR, "ymodem_send_end() failed, %d\n\r", res);
            }
        }
        modem_on_line = 0;
    }

    // resotre key input mode
    if (set_key_input) {
        set_key_input_raw(raw);
        set_key_input = 0;
    }

    // flush message buffer
    if (modem_receiving) {
        printf("\n\r");
    }
    if (msgbuf != NULL && msglen != 0) {
        #ifdef DEBUG
        printf("---------- %d bytes\n\r", msglen);
        #endif
        printf("%s\n\r", msgbuf);
        #ifdef DEBUG
        printf("----------\n\r");
        #endif
        msglen = 0;
        msgbuf[0] = '\0';
    }

    // close file
    if (filep) {
        if (save_file_name[0]) {
            f_sync(filep);
            f_close(filep);
            save_file_name[0] = '\0';
        }
        put_file(filep);
        filep = NULL;
    }

    // release buffers
    if (modem_buf) {
        util_memfree(modem_buf);
        modem_buf = NULL;
    }
    if (msgtmpbuf) {
        util_memfree(msgtmpbuf);
        msgtmpbuf = NULL;
    }
    #if !defined(DEBUG)
    if (msgbuf) {
        util_memfree(msgbuf);
        msgbuf = NULL;
    }
    #endif

    // release the mdoem
    modem_in_use = 0;
}

int modem_xfer_tx(uint8_t c)
{
    putch_buffered(c);

    return 1;
}

int modem_xfer_rx(uint8_t *c, int timeout_ms)
{
    return getch_buffered_timeout((char*)c, timeout_ms);
}

static void save_msg(const char *msg)
{
    unsigned int len = strlen(msg);
    if (msgbuf_size <= msglen + len) {
        unsigned int ofs = msglen + len - msgbuf_size;
        for (unsigned int i = 0; i < msgbuf_size - ofs; i++) {
            msgbuf[i] = msgbuf[ofs + i];
        }
        msglen -= ofs;
    }
    memcpy(&msgbuf[msglen], msg, len + 1);
    msglen += len;
}

void modem_xfer_printf(int log_level, const char *format, ...)
{
    const unsigned int bufsize = msgtmpbuf_size;
    char *buf = msgtmpbuf;

    va_list ap;
    va_start (ap, format);
    vsnprintf(buf, bufsize, format, ap);
    va_end (ap);
    unsigned int len = strlen(buf);
    if (0 < len && buf[len - 1] == '\n') {
        buf[len - 1] = '\r';
        save_msg(buf);
        save_msg("\n");
    } else {
        save_msg(buf);
    }
}

int modem_xfer_save(char *file_name, uint32_t offset, uint8_t *buf, uint16_t size)
{
    FRESULT fres;
    char tmp[MAX_FILE_NAME_LEN];
    unsigned int n;

    memcpy(tmp, file_name, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';
    if (memcmp(save_file_name, tmp, MAX_FILE_NAME_LEN) != 0) {
        //
        // new file
        //
        // close previous file if already opened
        if (save_file_name[0]) {
            save_file_name[0] = '\0';
            fres = f_sync(filep);
            if (f_close(filep) != FR_OK || fres != FR_OK) {
                return -1;
            }
        }
        // open new file
        if (f_open(filep, tmp, FA_WRITE|FA_OPEN_ALWAYS) != FR_OK) {
            save_file_name[0] = '\0';
            return -2;
        }
        strncpy(save_file_name, tmp, MAX_FILE_NAME_LEN);
    }

    // move file pointer to the offset
    if ((fres = f_lseek(filep, offset)) != FR_OK) {
        return -3;
    }

    if (buf != NULL && size != 0) {
        // write to the file
        if (f_write(filep, buf, size, &n) != FR_OK || n != size) {
            return -4;
        }
    } else {
        // truncate the file
        if (f_truncate(filep) != FR_OK) {
            return -5;
        }
    }

    return 0;
}
