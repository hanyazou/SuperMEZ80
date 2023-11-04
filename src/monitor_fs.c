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

static const unsigned int MON_MAX_PATH_LEN = TMP_BUF_SIZE;
#define MAX_FILE_NAME_LEN 13
static FIL file;
static char save_file_name[MAX_FILE_NAME_LEN];
static char * const msgbuf = (char *)tmp_buf[1];
static unsigned int msglen = 0;
static const unsigned int msgbuf_size = TMP_BUF_SIZE;

static void mon_fatfs_error(FRESULT fres, char *msg);

int mon_cmd_recv(int argc, char *args[])
{
    int raw = set_key_input_raw(1);

    msglen = 0;
    save_file_name[0] = '\0';
    if (ymodem_receive(tmp_buf[0]) != 0) {
        printf("\n\rymodem_receive() failed\n\r");
    }
    if (save_file_name[0]) {
        f_sync(&file);
        f_close(&file);
        save_file_name[0] = '\0';
    }
    set_key_input_raw(raw);
    printf("\n\r%s", msgbuf);

    return MON_CMD_OK;
}

int mon_cmd_pwd(int argc, char *args[])
{
    FRESULT fr;
    char * const buf = (char *)tmp_buf[1];
    const unsigned int bufsize = TMP_BUF_SIZE;

    fr = f_getcwd(buf, bufsize);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_getcwd() failed");
        return MON_CMD_OK;
    }

    printf("%s\n\r", buf);

    return MON_CMD_OK;
}

int mon_cmd_cd(int argc, char *args[])
{
    FRESULT fr;
    char *dir = "/";

    if (args[0] != NULL && *args[0] != '\0')
        dir = args[0];

    fr = f_chdir(dir);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_chdir() failed");
        return MON_CMD_OK;
    }

    return MON_CMD_OK;
}

static void show_fileinfo(FILINFO *fileinfo, uint8_t in_detail)
{
    if (in_detail) {
        printf("%cr%c %c%c %8ld %u-%02u-%02u %02u:%02u %s\n\r",
               (fileinfo->fattrib & AM_DIR) ? 'd' : '-',
               (fileinfo->fattrib & AM_RDO) ? '-' : 'w',
               (fileinfo->fattrib & AM_HID) ? 'h' : '-',
               (fileinfo->fattrib & AM_SYS) ? 's' : '-',
               (uint32_t)fileinfo->fsize,
               (fileinfo->fdate >> 9) + 1980,
               (fileinfo->fdate >> 5) & 15,
               fileinfo->fdate & 31,
               fileinfo->ftime >> 11,
               (fileinfo->ftime >> 5) & 63,
               fileinfo->fname);
    } else {
        printf("%s%s\n\r", fileinfo->fname, (fileinfo->fattrib & AM_DIR) ? "/" : "");
    }
}

int mon_cmd_ls(int argc, char *args[])
{
    FRESULT fr;
    DIR fsdir;
    FILINFO fileinfo;
    uint8_t in_detail = 0;
    char *dir = ".";

    int i = 0;
    if (args[i] != NULL && strcmp(args[i], "-l") == 0) {
        i++;
        in_detail = 1;
    }
    if (args[i] != NULL && *args[i] != '\0') {
        dir = args[i];
    }

    if (strcmp(dir, ".") != 0) {
        fr = f_stat(dir, &fileinfo);
        if (fr != FR_OK) {
            mon_fatfs_error(fr, "f_stat() failed");
            return MON_CMD_OK;
        }
        if (!(fileinfo.fattrib & AM_DIR)) {
            show_fileinfo(&fileinfo, in_detail);
            return MON_CMD_OK;
        }
    }

    fr = f_opendir(&fsdir, dir);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_opendir() failed");
        return MON_CMD_OK;
    }

    while ((fr = f_readdir(&fsdir, &fileinfo)) == FR_OK && fileinfo.fname[0] != 0) {
        show_fileinfo(&fileinfo, in_detail);
    }
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_readdir() failed");
    }
    fr = f_closedir(&fsdir);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_closedir() failed");
    }

    return MON_CMD_OK;
}

int mon_cmd_mkdir(int argc, char *args[])
{
    FRESULT fr;

    if (args[0] == NULL || *args[0] == '\0') {
        printf("usage: mkdir directory\n\r");
        return MON_CMD_OK;
    }

    fr = f_mkdir(args[0]);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_mkdir() failed");
        return MON_CMD_OK;
    }

    return MON_CMD_OK;
}

int mon_cmd_rm(int argc, char *args[])
{
    FRESULT fr;
    DIR fsdir;
    FILINFO fileinfo;
    uint8_t recursive = 0;
    char *file;

    int i = 0;
    if (args[i] != NULL && strcmp(args[i], "-r") == 0) {
        i++;
        recursive = 1;
    }
    if (args[i] == NULL || args[i] == 0) {
        printf("usage: rm [-r] file or directory\n\r");
        return MON_CMD_OK;
    }
    file = args[i];

    int nest;
    int removed;
    if (recursive) {
    redo:
        nest = 1;
        for (char *p = file; *p != '\0'; p++) {
            if (*p == '/') {
                nest++;
            }
        }
        fr = f_chdir(file);
        if (fr != FR_OK) {
            mon_fatfs_error(fr, "f_chdir() failed");
            return MON_CMD_OK;
        }

    remove_files_in_current_directory:
        removed = 0;
        fr = f_opendir(&fsdir, ".");
        if (fr != FR_OK) {
            mon_fatfs_error(fr, "f_opendir() failed");
            return MON_CMD_OK;
        }

        while (1) {
            fr = f_readdir(&fsdir, &fileinfo);
            if (fr != FR_OK) {
                mon_fatfs_error(fr, "f_readdir() failed");
                break;
            }
            if (fileinfo.fname[0] == 0) {
                break;
            }
            removed++;
            fr = f_unlink(fileinfo.fname);
            if (fr != FR_OK) {
                if (!(fileinfo.fattrib & AM_DIR)) {
                    mon_fatfs_error(fr, "f_unlink() failed");
                    break;
                }
                fr = f_closedir(&fsdir);
                if (fr != FR_OK) {
                    mon_fatfs_error(fr, "f_closedir() failed");
                    break;
                }
                fr = f_chdir(fileinfo.fname);
                if (fr != FR_OK) {
                    mon_fatfs_error(fr, "f_chdir() failed");
                    break;
                }
                nest++;
                goto remove_files_in_current_directory;
            }
        }
        fr = f_closedir(&fsdir);
        if (fr != FR_OK) {
            mon_fatfs_error(fr, "f_closedir() failed");
        }

        for (int i = 0; i < nest; i++) {
            fr = f_chdir("..");
            if (fr != FR_OK) {
                mon_fatfs_error(fr, "f_chdir() failed");
                return MON_CMD_OK;
            }
        }
        if (removed) {
            goto redo;
        }
    }

    fr = f_unlink(file);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_unlink() failed");
    }

    return MON_CMD_OK;
}

int mon_cmd_mv(int argc, char *args[])
{
    FRESULT fr;

    if (args[0] == NULL || *args[0] == '\0' || args[1] == NULL || *args[1] == '\0') {
        printf("usage: mv old_name new_name\n\r");
        return MON_CMD_OK;
    }

    fr = f_rename(args[0], args[1]);
    if (fr != FR_OK) {
        mon_fatfs_error(fr, "f_rename() failed");
        return MON_CMD_OK;
    }

    return MON_CMD_OK;
}

static void mon_fatfs_error(FRESULT fres, char *msg)
{
    struct {
        FRESULT fres;
        char *errmsg;
    } errmsgs[] = {
        { FR_OK,                    "OK" },
        { FR_DISK_ERR,              "DISK_ERR" },
        { FR_INT_ERR,               "INT_ERR" },
        { FR_NOT_READY,             "NOT_READY" },
        { FR_NO_FILE,               "NO_FILE" },
        { FR_NO_PATH,               "NO_PATH" },
        { FR_INVALID_NAME,          "INVALID_NAME" },
        { FR_DENIED,                "DENIED" },
        { FR_EXIST,                 "EXIST" },
        { FR_INVALID_OBJECT,        "INVALID_OBJECT" },
        { FR_WRITE_PROTECTED,       "WRITE_PROTECTED" },
        { FR_TIMEOUT,               "TIMEOUT" },
        { FR_TOO_MANY_OPEN_FILES,   "TOO_MANY_OPEN_FILES" },
        { FR_INVALID_PARAMETER,     "INVALID_PARAMETER" },
    };

    int i;
    for (i = 0; i < UTIL_ARRAYSIZEOF(errmsgs); i++) {
        if (errmsgs[i].fres == fres)
            break;
    }

    if (i < UTIL_ARRAYSIZEOF(errmsgs)) {
        printf("%s, %s\n\r", msg, errmsgs[i].errmsg);
    } else {
        printf("%s, %d\n\r", msg, fres);
    }
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

void modem_xfer_printf(int log_level, const char *format, ...)
{
    char *buf = (char *)&tmp_buf[0][MODEM_XFER_BUF_SIZE];
    const unsigned int bufsize = TMP_BUF_SIZE - MODEM_XFER_BUF_SIZE;
    va_list ap;
    va_start (ap, format);
    vsnprintf(buf, bufsize, format, ap);
    va_end (ap);
    buf[bufsize - 2] = '\n';
    unsigned int len = strlen(buf);
    buf[len] = '\r';
    len++;
    buf[len] = '\0';
    if (msgbuf_size <= msglen + len) {
        unsigned int ofs = (msglen + len + 1) - msgbuf_size;
        for (unsigned int i = 0; i < msgbuf_size - ofs; i++) {
            msgbuf[i] = msgbuf[ofs + i];
        }
        msglen -= ofs;
    }
    memcpy(&msgbuf[msglen], buf, len + 1);
    msglen += len;
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
            fres = f_sync(&file);
            if (f_close(&file) != FR_OK || fres != FR_OK) {
                return -1;
            }
        }
        // open new file
        if (f_open(&file, tmp, FA_WRITE|FA_OPEN_ALWAYS) != FR_OK) {
            save_file_name[0] = '\0';
            return -2;
        }
        strncpy(save_file_name, tmp, MAX_FILE_NAME_LEN);
    }

    // move file pointer to the offset
    if ((fres = f_lseek(&file, offset)) != FR_OK) {
        return -3;
    }

    if (buf != NULL && size != 0) {
        // write to the file
        if (f_write(&file, buf, size, &n) != FR_OK || n != size) {
            return -4;
        }
    } else {
        // truncate the file
        if (f_truncate(&file) != FR_OK) {
            return -5;
        }
    }

    return 0;
}
