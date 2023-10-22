#include <ymodem.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define REQ  'C'
#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18

#define BUFSIZE 128
#define SOH_SIZE 128
#define STX_SIZE 1024

static int (*tx)(uint8_t);
static int (*rx)(uint8_t *, int timeout_ms);

//#define DEBUG
//#define DEBUG_VERBOSE

#ifdef DEBUG
#define dprintf(args) do { printf args; } while(0)
#else
#define dprintf(args) do { } while(0)
#endif

static int discard(void)
{
    int res = 0;
    uint8_t rxb;
    while (rx(&rxb, 300) == 1) {
        res++;
    }

    return res;
}

static int recv_bytes(uint8_t *buf, int n, int timeout_ms)
{
    int i;
    int res;

    for (i = 0; i < n; i++) {
        res = rx(&buf[i], timeout_ms);
        if (res == 0) {
            //  time out (there might be no sender)
            return i;
        }
        if (res < 0) {
            // error
            return res;
        }
    }

    return i;
}

static int hex_dump(uint8_t *buf, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if ((i % 16) == 0) {
            printf("%04X:", i);
        }
        printf(" %02X", buf[i]);
        if ((i % 16) == 15) {
            printf("\n");
        }
    }
    if ((i % 16) != 0) {
        printf("\n");
    }
}

static uint16_t crc16(uint16_t crc, const void *buf, unsigned int count)
{
    uint8_t *p = (uint8_t*)buf;
    uint8_t *endp = p + count;

    while (p < endp) {
        crc = (crc >> 8)|(crc << 8);
        crc ^= *p++;
        crc ^= ((crc & 0xff) >> 4);
        crc ^= (crc << 12);
        crc ^= ((crc & 0xff) << 5);
    }

    return crc;
}

int ymodem_receive(int (*__tx)(uint8_t), int (*__rx)(uint8_t *, int timeout_ms))
{
    int res, retry, files;
    uint8_t first_block;
    uint8_t seqno;
    uint8_t buf[3];
    tx = __tx;
    rx = __rx;
    uint8_t tmpbuf[1][BUFSIZE];
    uint8_t *payload = tmpbuf[0];
    uint16_t crc;

    files = 0;

 recv_file:
    retry = 0;
    seqno = 0;
    first_block = 1;

    tx(REQ);
    while (1) {
        /*
         * receive block herader
         */
        if (recv_bytes(buf, 1, 10000) != 1) {
            return -1;
        }
        if (buf[0] == EOT) {
            dprintf(("%02X: EOT\n", seqno));
            tx(NAK);
            recv_bytes(&buf[0], 1, 1000);
            if (buf[0] != EOT) {
                printf("WARNING: EOT expected but received %02X\n", buf[0]);
            }
            tx(ACK);
            files++;
            goto recv_file;
        }
        if (buf[0] != STX && buf[0] != SOH) {
            printf("%02X: invalid header %02X\n", seqno, buf[0]);
            goto retry;
        }

        /*
         * receive sequence number
         */
        if (recv_bytes(&buf[1], 2, 300) != 2) {
            printf("%02X: timeout\n", seqno);
            goto retry;
        }
        dprintf(("%02X: %02X %02X %02X\n", seqno, buf[0], buf[1], buf[1]));
        if (buf[1] != seqno && buf[2] != ((~seqno) + 1)) {
            printf("%02X: invalid sequence number\n", seqno);
            goto retry;
        }

        /*
         * receive payload
         */
        crc = 0;
        for (int i = 0; i < (buf[0] == STX ? STX_SIZE/BUFSIZE : SOH_SIZE/BUFSIZE); i++) {
            if (recv_bytes(payload, BUFSIZE, 1000) != BUFSIZE) {
                goto retry;
            }
            dprintf(("%02X: %d bytes received\n", seqno, BUFSIZE));
            #ifdef DEBUG_VERBOSE
            hex_dump(payload, BUFSIZE);
            #endif
            crc = crc16(crc, payload, BUFSIZE);
        }

        /*
         * receive and check CRC
         */
        if (recv_bytes(buf, 2, 1000) != 2) {
            goto retry;
        }
        dprintf(("%02X: crc16: %04x %s %04x\n", seqno, buf[0] * 256 + buf[1],
                 (buf[0] * 256 + buf[1]) == crc ? "==" : "!=", crc));
        if ((buf[0] * 256 + buf[1]) != crc) {
            goto retry;
        }
        tx(ACK);
        retry = 0;

        /*
         * process received block
         */
        if (first_block) {
            if (payload[0] == 0x00) {
                dprintf(("total %d file%s received\n", files, 1 < files ? "s" : ""));
                tx(ACK);
                return 0;
            }
            printf("receive file: '%s' %s\n", &payload[0], &payload[strlen(&payload[0]) + 1]);
            tx(REQ);
            first_block = 0;
        }
        seqno++;
        continue;

    retry:
        res = discard();
        dprintf(("%02X: discard %d bytes\n", seqno, res));
        tx(NAK);
        if (5 <= ++retry) {
            tx(CAN);
            tx(CAN);
            rx(buf, 1000);
            return -1;
        }
    }

    return 0;
}

