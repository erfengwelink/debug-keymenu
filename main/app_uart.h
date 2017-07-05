#ifndef __APP_UART_H_
#define __APP_UART_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

//UART to 485 message process
//UART 1 UART 2, Keyboard & Perpherials
typedef void (*uart_cb)(const char* buffer, int len);
typedef int  (*console_cb)(int argc, char** argv);
typedef struct console_cmd_t {
    char *cmd;
    console_cb callback;
} console_cmd_t;

void app_uart_init(void);
void app_uart_set_callback(uart_cb cb);
int  app_uart_read_data(unsigned char* buf, int len, unsigned int timeout);
int  app_uart_write_data(unsigned char* data, int len);
int  app_uart_exe(char *args);
int  app_uart_console(int argc, char** argv);

#endif //__APP_UART_H_