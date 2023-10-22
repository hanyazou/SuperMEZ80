
#include "ymodem.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/select.h>

static int tx_fd = -1;
static int rx_fd = -1;

int open_port(void)
{
    const char *TX = "/tmp/modem-test-tx";
    const char *RX = "/tmp/modem-test-rx";

    mkfifo(TX, 0660);
    tx_fd = open(TX, O_RDWR);
    if(tx_fd < 0) {
        printf("open(%s) failed (errno=%d)\n", TX, errno);
    }
    mkfifo(RX, 0660);
    rx_fd = open(RX, O_RDWR);
    if(rx_fd < 0) {
        printf("open(%s) failed (errno=%d)\n", RX, errno);
    }

    return (0 <= tx_fd) && (0 <= rx_fd) ? 0 : -1;
}

void close_port(void)
{
    if (0 <= tx_fd)
        close(tx_fd);
    if (0 <= rx_fd)
        close(rx_fd);
}

int tx_func(uint8_t c)
{
    return write(tx_fd, &c, 1);
}

int rx_func(uint8_t *c, int timeout_ms)
{
    fd_set set;
    struct timeval tv;
    FD_ZERO(&set);
    FD_SET(rx_fd, &set);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int res = select(rx_fd + 1, &set, NULL, NULL, &tv);
    if(res < 0) {
        printf("select() failed (errno=%d)\n", errno);
        return -errno;
    }
    if(res == 0) {
        /* timeout occured */
        return 0;
    }

    return read(rx_fd, c, 1);
}

int main(int ac, char *av[])
{
    if (open_port() != 0) {
        printf("open_port() failed\n");
        exit(1);
    }
    if (ymodem_receive(tx_func, rx_func) != 0) {
        printf("ymodem_receive() failed\n");
    }
    close_port();

    return 0;
}
