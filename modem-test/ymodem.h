#include <stdint.h>

int ymodem_receive(int (*tx)(uint8_t), int (*rx)(uint8_t *, int timeout_ms));
