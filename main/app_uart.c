#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "soc/uart_struct.h"

#include "app_uart.h"
#include "argparse.h"
//#include "app_loop.h"
//#include "app_main.h"
//#include "app_ctrl.h"
//#include "fota.h"
//#include "nfc.h"
#include "keyparser.h"
//#include "NEW_FSM.h"

static const char *TAG = "UART";

#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)

static QueueHandle_t uart0_queue;
static uart_cb s_uart_cb = NULL;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.
                in this example, we don't process data in event, but read data outside.*/
                case UART_DATA:
                    uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                    ESP_LOGI(TAG, "data, len: %d; buffered len: %d", event.size, buffered_size);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow\n");
                    //If fifo overflow happened, you should consider adding flow control for your application.
                    //We can read data out out the buffer, or directly flush the rx buffer.
                    uart_flush(EX_UART_NUM);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full\n");
                    //If buffer full happened, you should consider encreasing your buffer size
                    //We can read data out out the buffer, or directly flush the rx buffer.
                    uart_flush(EX_UART_NUM);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break\n");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error\n");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error\n");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "uart pattern detected\n");
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d\n", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void app_uart_set_callback(uart_cb cb)
{
	s_uart_cb = cb;
}

void app_uart_init(void)
{
    uart_config_t uart_config = {
       .baud_rate = 115200,
       .data_bits = UART_DATA_8_BITS,
       .parity = UART_PARITY_DISABLE,
       .stop_bits = UART_STOP_BITS_1,
       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
       .rx_flow_ctrl_thresh = 122,
    };
    //Set UART parameters
    uart_param_config(EX_UART_NUM, &uart_config);
    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart0_queue, 0);

    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_intr(EX_UART_NUM, '+', 3, 10000, 10, 10);
    //Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
#if 0
    //process data
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    do {
        int len = uart_read_bytes(EX_UART_NUM, data, BUF_SIZE, 100 / portTICK_RATE_MS);
        if(len > 0) {
            ESP_LOGI(TAG, "uart read : %d", len);
            uart_write_bytes(EX_UART_NUM, (const char*)data, len);
        }
    } while(1);
#endif
}

int app_uart_write_data(unsigned char* data, int len)
{
    int length = 0;

    //ESP_LOGI(TAG, "%s buf: %s", __func__, data);
    length = uart_write_bytes(EX_UART_NUM,(char*)data, len);
    return length;
}

int app_uart_read_data(unsigned char *buf, int len, unsigned int timeout)
{
    TickType_t ticks_to_wait = timeout;
    unsigned char *data = NULL;
    size_t size = 0;

    if (len == 0) {
        return 0;
    }

    //ESP_LOGI(TAG, "%s", __func__);

    if (buf == NULL) {
        if (len == -1) {
            if (ESP_OK != uart_get_buffered_data_len(EX_UART_NUM, &size)) {
                return -1;
            }
            len = size;
        }

        if (len == 0) {
            return 0;
        }

        data = (uint8_t *)malloc(len);
        if (data) {
            len = uart_read_bytes(EX_UART_NUM,data,len,ticks_to_wait);
            free(data);
            return len;
        } else {
            return -1;
        }
    } else {
        return uart_read_bytes(EX_UART_NUM,buf,len,ticks_to_wait);
    }

    return len;
}

static const int argc_MAX = 5;
#define isspace(c)           (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')

/** 
 * Parse out the next non-space word from a string.
 * @note No nullptr protection
 * @param str  [IN]   Pointer to pointer to the string. Nested pointer to string will be changed.
 * @param word [OUT]  Pointer to pointer of next word. To be filled.
 * @return  pointer to string - current cursor. Check it for '\0' to stop calling this function   
 */
static char* splitArgv(char **str, char **word)
{
	const char QUOTE = '\'';
	bool inquotes = false;
	
	// optimization
	if( **str == 0 )
		return NULL;
	
	// Skip leading spaces.
	while (**str && isspace(**str)) 
		(*str)++;
	
	if( **str == '\0')
		return NULL;
	
	// Phrase in quotes is one arg
	if( **str == QUOTE ){
		(*str)++;
		inquotes = true;
	}
	
	// Set phrase begining
	*word = *str;
	
	// Skip all chars if in quotes
	if( inquotes ){
		while( **str && **str!=QUOTE )
			(*str)++;
		//if( **str!= QUOTE )
	}else{
		// Skip non-space characters.
		while( **str && !isspace(**str) )
			(*str)++;
	}
	// Null terminate the phrase and set `str` pointer to next symbol
	if(**str)
		*(*str)++ = '\0';
	
	return *str;
}


/// To support standart convetion last `argv[argc]` will be set to `NULL`
///\param[IN]  str : Input string. Will be changed - splitted to substrings
///\param[IN]  argc_MAX : Maximum a rgc, in other words size of input array \p argv
///\param[OUT] argc : Number of arguments to be filled
///\param[OUT] argv : Array of c-string pointers to be filled. All of these strings are substrings of \p str
///\return Pointer to the rest of string. Check if for '\0' and 
///know if there is still something to parse. 
char* parseStrToArgcArgvInsitu( char *str, const int argc_MAX, int *argc, char* argv[] )
{
	*argc = 0;
	while( *argc<argc_MAX-1  &&  splitArgv(&str, &argv[*argc]) ){
		++(*argc);
		if( *str == '\0' )
			break;
	}
	argv[*argc] = NULL;
	return str;
};


void parseAndPrintOneString(char *input)
{
	char* v[argc_MAX];// = {0};
	int c=0;
	
	char* rest = NULL;
	
	memset(v, 0, sizeof(v));
	rest = parseStrToArgcArgvInsitu(input,argc_MAX,&c,v);
	if( *rest!='\0' ) {
        ESP_LOGI(TAG, "%s There is still something to parse. argc_MAX is too small.", __func__);
    }
	
    ESP_LOGI(TAG, "%s argc %d", __func__, c);
	//cout << "argc : "<< c << endl;

	for( int i=0; i<c; i++ )
        ESP_LOGI(TAG, "%s argv[%d]: %s", __func__, i, v[i]);

}

static inline bool is_eq(const char* src, const char* dst)
{
    return (strcasecmp (src, dst) == 0) ? true : false;
}

static int xt_exe_key(int argc, char** argv)
{
    const char *keystr = NULL;
    int keychar = 0;
    int keyaction = 0;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('s', "string", &keystr, "string buffer"),
        OPT_INTEGER('c', "char", &keychar, "is char"),
        OPT_INTEGER('a', "action", &keyaction, "key action"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);
    if (keychar != 0)
        ESP_LOGI(TAG, "keychar %d", keychar);

    if (keyaction != 0)
        ESP_LOGI(TAG, "keyaction %d", keyaction);

    if (keystr != NULL)
        ESP_LOGI(TAG, "keybuffer %s", keystr);

    if (keyaction > 0 && keychar > 0 && keystr) {
        //app_ev_t ev;
        //init_key_ext_event(&ev, keyaction, (int)keystr[0]);
        //app_event_send(&ev);
    }

	return 0;
}
 
static int xt_exe_beep(int argc, char** argv)
{
    int freq = 0;
    int ms = 0;
    int repeat = 1;
    int interval = 0;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_INTEGER('f', "freq", &freq, "freq"),
        OPT_INTEGER('m', "ms", &ms, "duration"),
        OPT_INTEGER('r', "repeat", &repeat, "repeat"),
        OPT_INTEGER('i', "interval", &interval, "interval"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);

    if (repeat <= 0) {
        repeat =  1;
    }

    for (int i = 0; i < repeat; i++) {
        if (freq > 0 && ms > 0) {
            //beep(freq, ms);
            vTaskDelay(interval);
        }
    }

	return 0;
}

static int xt_exe_fuse_set(int argc, char** argv)
{
	ESP_LOGI(TAG, "%s argc %d", __func__, argc);
	return 0;
}

static int xt_exe_nvs_set(int argc, char** argv)
{
	ESP_LOGI(TAG, "%s argc %d", __func__, argc);
	return 0;
}

static int xt_exe_cloud_checkversion(int argc, char** argv)
{
	ESP_LOGI(TAG, "%s argc %d", __func__, argc);
	return 0;
}

static int xt_exe_cloud_ota(int argc, char** argv)
{
    char* path = NULL;
    char* data = NULL;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('p', "path", &path, "path"),
        OPT_STRING('s', "string", &data, "params"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);

    if ((path == NULL) || (data == NULL)) {
        return 0;
    }

    //do_ota_upgrade(path, data);
    //wait for reboot
	return 0;
}

static int xt_exe_nfc_read(int argc, char** argv)
{
    char* ctype = 0; //1, M1(A), 2. CPU CARD(A4). 3. B(ID CARD)
    char* city = 0; //0755, 020
    int   etype = 0;
    int   ecity = 0;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('t', "type", &ctype, "type"),
        OPT_STRING('c', "city", &city, "city"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);

    if ((ctype == NULL) || (city == NULL)) {
        return 0;
    }

    if (is_eq(ctype, "A")) {
        etype = 0;
    }
    else if (is_eq(ctype, "A4")) {
        etype = 1;
    }
    else if (is_eq(ctype, "B")) {
        etype = 1;
    }

    if (is_eq(city, "0755")) { //SZ
        etype = 1;
    }
    else if (is_eq(city, "020")) { //GZ
        etype = 1;
    }
    else if (is_eq(city, "010")) { //BJ
        etype = 1;
    }
    else if (is_eq(city, "021")) { //SH
        etype = 1;
    }
    else if (is_eq(city, "0571")) { //HZ
        etype = 1;
    }

	return 0;
}

static int xt_exe_unittest(int argc, char** argv)
{
	ESP_LOGI(TAG, "%s argc %d", __func__, argc);
	return 0;
}

static int xt_exe_keypadcmd(int argc, char** argv)
{	
	char *keyValue = 0;
	int keyCmd = KCMD_NULL;
	int keyCode = KCODE_NULL;
	int i = 0;
	ESP_LOGI(TAG, "%s argc %d", __func__, argc);
	struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('k', "keyValue", &keyValue, "keyValue"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);
	
	if (keyValue == NULL){
			return 0;
	}
	else{
		printf("\ninput keyValue: %s\n",keyValue);
		
		for(;keyValue[i]!='\0';i++){
			keyparser_process_char(keyValue[i],&keyCmd,&keyCode);
			ESP_LOGI(TAG, " \nkeyCmd: %d|keyCode:%d\n", keyCmd,keyCode);
			keyValue[i] = '\0';
		}
		printf("\nkeyparser_buffer: %s\n",keyparser_buffer());
		
	}
	return 0;
}


static console_cmd_t cons_cmds[] = {
	//APP
    {"XKEY",      xt_exe_key},
    // {"+XPROPSET",  xt_exe_prop_set},
    // {"+XPROPGET",  xt_exe_prop_get},
    // {"+XNODESET",  xt_exe_node_set},
    // {"+XNODEGET",  xt_exe_node_get},
    {"XBEEP",     xt_exe_beep},
    {"XFUSESET",  xt_exe_fuse_set},
    {"XNVSSET",   xt_exe_nvs_set},
	// {"+XROMGET",   xt_exe_rom_get},
    {"XVER",      xt_exe_cloud_checkversion},
	{"XOTA",      xt_exe_cloud_ota},
    {"XNFC",      xt_exe_nfc_read},
    {"XKEYPADCMD",      xt_exe_keypadcmd},
	//{"XECHO",     xt_exe_echo},
	{"XTEST",     xt_exe_unittest}
};


//ipconfig -f -all
int  app_uart_exe(char *args)
{
	char* v[16];
	int c = 0;
	char* rest = NULL;
	memset(v, 0, sizeof(v));
	rest = parseStrToArgcArgvInsitu(args, 16, &c, v);
	
	if( *rest != '\0' ) {
        ESP_LOGI(TAG, "%s There is still something to parse. argc_MAX is too small.", __func__);
        return 0;
    }

	ESP_LOGI(TAG, "%s argc %d", __func__, c);
	for(int i = 0; i < c; i++)
        ESP_LOGI(TAG, "%s argv[%d]: %s", __func__, i, v[i]);

    return app_uart_console(c, v);
}

int  app_uart_console(int argc, char** argv)
{
    char *cmd;
    console_cb callback;
    int len = sizeof(cons_cmds) / sizeof(console_cmd_t);

    //ESP_LOGI(TAG, "%s argc %d, len %d", __func__, argc, len);

    for(int i = 0; i < len; i++) {
        cmd = cons_cmds[i].cmd;
        callback = cons_cmds[i].callback;
        if ((strcmp(argv[0], cmd) == 0) && callback){
            return (*callback)(argc, argv);
        }
    } 

    return 0;
}
