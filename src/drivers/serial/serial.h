#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

// COM port addresses
#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

// Initialize serial port (default: COM1)
void serial_init(uint16_t port);

// Write a single character
void serial_putchar(uint16_t port, char c);

// Write a string
void serial_puts(uint16_t port, const char *str);

// Check if transmit buffer is empty
int serial_transmit_empty(uint16_t port);

// Read a character (blocking)
char serial_getchar(uint16_t port);

// Check if data is available
int serial_received(uint16_t port);

#endif // SERIAL_H
