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
#include <assert.h>

#include <ff.h>
#include <utils.h>

static FIL *auxout_filep = NULL;
static uint8_t auxout_error = 0;
static timer_t auxout_timer;
static FIL *auxin_filep = NULL;
static uint8_t auxin_error = 0;
static timer_t auxin_timer;
static FSIZE_t auxin_offset = 0;
static uint8_t auxin_line_feed = 0;

static void auxout_timer_callback(timer_t *timer) {
    FRESULT fr;

    #ifdef CPM_IO_AUX_DEBUG
    printf("%s: close AUXOUT.TXT\n\r", __func__);
    #endif
    fr = f_close(auxout_filep);
    put_file(auxout_filep);
    auxout_filep = NULL;
    if (fr != FR_OK) {
        if (!auxout_error) {
            auxout_error = 1;
            util_fatfs_error(fr, "f_close(/AUXOUT.TXT) failed");
        }
        return;
    }
    auxout_error = 0;
}

void auxout(uint8_t c) {
    FRESULT fr;

    if (auxout_filep == NULL) {
        auxout_filep = get_file();
        if (auxout_filep == NULL) {
            if (!auxout_error) {
                auxout_error = 1;
                printf("auxout: can not allocate file\n\r");
            }
            return;
        }
        auxout_error = 0;
        #ifdef CPM_IO_AUX_DEBUG
        printf("%s: open AUXOUT.TXT\n\r", __func__);
        #endif
        fr = f_open(auxout_filep, "/AUXOUT.TXT", FA_WRITE|FA_OPEN_APPEND);
        if (fr != FR_OK) {
            if (!auxout_error) {
                auxout_error = 1;
                util_fatfs_error(fr, "f_open(/AUXOUT.TXT) failed");
            }
            return;
        }
        auxout_error = 0;
    }

    timer_set_relative(&auxout_timer, auxout_timer_callback, 1000);

    if (c == 0x00 || c == '\r') {
        // ignore 00h and 0Dh (Carriage Return)
        return;
    }

    if (c == 0x1a) {
        // 1Ah (EOF)
        timer_expire(&auxout_timer);  // close the file immediately
        return;
    }

    UINT bw;
    fr = f_write(auxout_filep, &c, 1, &bw);
    if (fr != FR_OK || bw != 1) {
        // write error
        if (!auxout_error) {
            auxout_error = 1;
            util_fatfs_error(fr, "f_write(/AUXOUT.TXT) failed");
        }
        return;
    }
    auxout_error = 0;
}

static void auxin_timer_callback(timer_t *timer) {
    FRESULT fr;

    #ifdef CPM_IO_AUX_DEBUG
    printf("%s: close AUXIN.TXT\n\r", __func__);
    #endif
    fr = f_close(auxin_filep);
    put_file(auxin_filep);
    auxin_filep = NULL;
    if (fr != FR_OK) {
        if (!auxin_error) {
            auxin_error = 1;
            util_fatfs_error(fr, "f_close(/AUXIN.TXT) failed");
        }
        return;
    }
    auxin_error = 0;
}

void auxin(uint8_t *c) {
    FRESULT fr;

    if (auxin_filep == NULL) {
        auxin_filep = get_file();
        if (auxin_filep == NULL) {
            if (!auxin_error) {
                auxin_error = 1;
                printf("auxout: can not allocate file\n\r");
            }
            return;
        }
        auxin_error = 0;
        #ifdef CPM_IO_AUX_DEBUG
        printf("%s: open AUXIN.TXT\n\r", __func__);
        #endif
        fr = f_open(auxin_filep, "/AUXIN.TXT", FA_READ|FA_OPEN_ALWAYS);
        if (fr != FR_OK) {
            if (!auxin_error) {
                auxin_error = 1;
                util_fatfs_error(fr, "f_open(/AUXIN.TXT) failed");
            }
            return;
        }
        auxin_error = 0;
        fr = f_lseek(auxin_filep, auxin_offset);
        if (fr != FR_OK) {
            if (!auxin_error) {
                auxin_error = 1;
                util_fatfs_error(fr, "f_lseek(/AUXIN.TXT) failed");
            }
            auxin_offset = 0;
            f_rewind(auxin_filep);
        }
    }

    timer_set_relative(&auxin_timer, auxin_timer_callback, 1000);

    if (auxin_line_feed) {
        // insert LF (\n 0Ah)
        auxin_line_feed = 0;
        *c = '\n';
        return;
    }

    UINT bw;
    fr = f_read(auxin_filep, c, 1, &bw);
    if (fr != FR_OK || bw != 1) {
        if (fr != FR_OK && !auxin_error) {
            // error
            auxin_error = 1;
            util_fatfs_error(fr, "f_read(/AUXIN.TXT) failed");
        }
        if (bw == 0) {
            // reaching end of file
            timer_expire(&auxin_timer);  // close the file immediately
            auxin_offset = 0;  // rewind file position
        }
        *c = 0x1a;  // return EOF at end of file or some error
        return;
    }
    if (*c == '\n') {
        // convert LF (\n 0Ah) to CRLF (\r 0Dh, \n 0Ah)
        *c = '\r';
        auxin_line_feed = 1;
    }
    auxin_offset++;
    auxin_error = 0;
}
