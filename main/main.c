#include "keyparser.h"
//#include "NEW_FSM.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "app_uart.h"
#include "argparse.h"





#define UART_DEFAULT_BUF_SIZE 128
#define UART_DEFAULT_TIMEOUT 100

static char s_uart_buffer[UART_DEFAULT_BUF_SIZE] = {0};

static char* TAG = "uart_debug";

void debug_task(void *p)
{
	do{
        int len = app_uart_read_data((unsigned char*)s_uart_buffer, UART_DEFAULT_BUF_SIZE, UART_DEFAULT_TIMEOUT);
		int ret = 0;
        if(len > 0) {
            ESP_LOGI(TAG, "uart read : %d", len);
            ret = app_uart_exe(s_uart_buffer);
			if(ret){
				app_uart_write_data((unsigned char*)s_uart_buffer,len);
			}
        }
	}while(1);	
	vTaskDelete(NULL);
}

void app_main()
{	
	app_uart_init();
	keyparser_init();
	//keyparser_set_admin_pwd("12345678",8);
	xTaskCreate(&debug_task, "debug_task", 8192, NULL, 5, NULL);

}

