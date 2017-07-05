#include "keyparser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "string.h"

static char* TAG = "keyparser:";

#define NORMALADDR 	0
#define ADMINADDR	0
#define AMENDADDR 	1
#define PWDLEN		8

//LOWER~HIGHER LIMIT:
#define ALARM_OFF_TIME_LOW 	0//48~57
#define ALARM_OFF_TIME_HIGH	3

#define AUTO_OFF_TIME_LOW 	0
#define AUTO_OFF_TIME_HIGH 99


//BUFFER_SIZ:

#define MAX_KEYBUFFER_SIZE 32
#define MAX_AUTHKEYBUFFER_SIZE 10

#define IS_NUM_CHAR(ch) 	(((ch)>47&&(ch)<58)?true:false)

#define KSTAR	'*'
#define KSHARP	'#'
#define KZERO 	'0'
#define KONE 	'1'
#define KTWO 	'2'
#define KTHREE 	'3'
#define KFUOR 	'4'
#define KFIVE 	'5'
#define KSIX 	'6'
#define KSEVEN 	'7'
#define KEIGHT 	'8'
#define KNINE 	'9'

typedef enum key_mode{
	NORMAL_MODE = 0,
	CRITICAL_MODE ,
	ADMIN_MODE
}k_mode_t;

typedef enum admin_oprt
{	NONE_OP = KSTAR,
	AMEND_PWD_OP = KZERO,
    ADD_USR_OP ,
    DELETE_USR_OP ,
    OPEN_MODE_OP ,
    DOOR_OFF_T_OP ,
    ALARM_T_OP ,
    ALARM_SW_OP ,
    MAIN_MODE_OP ,
    LED_SW_OP ,
    CANCLE_ALARM_OP ,
    MAX_OP
}admin_op;

typedef struct _fsm{
	k_mode_t _mode;
	admin_op _admin_op;
	int _counter;
}fsm_t;

static volatile fsm_t app_fsm = {NORMAL_MODE,NONE_OP,0};

static char keyBuffer[MAX_KEYBUFFER_SIZE];
static char keyBuffer_bak[MAX_KEYBUFFER_SIZE];
static char auth_keyBuffer[MAX_AUTHKEYBUFFER_SIZE];
static char admin_PWD[9] = "99999999";


void fsm_set_mode(k_mode_t mode)
{
	app_fsm._mode = mode;
}

static k_mode_t fsm_get_mode(void)
{
	return app_fsm._mode;
}

static admin_op fsm_get_admin_op(void)
{
	return (admin_op)keyBuffer[0];
}

static int fsm_get_counter(void)
{
	return app_fsm._counter;
}

static void clear_fsm_counter(void)
{
	app_fsm._counter = 0;
}

static int clearKeyBuffer(void)
{
	memset(keyBuffer,0,MAX_KEYBUFFER_SIZE);
	return 0;
}

static int clearKeyBufferBak(void)
{
	memset(keyBuffer_bak,0,MAX_KEYBUFFER_SIZE);
	return 0;
}

int clearAuthKeyBuffer(void)
{
	memset(auth_keyBuffer,0,MAX_AUTHKEYBUFFER_SIZE);
	return 0;
}

int keyparser_init(void)
{
	clearKeyBuffer();
    clearKeyBufferBak();
	clearAuthKeyBuffer();
	clear_fsm_counter();
	return 0;
}

static void writeKeyBuffer(char ch)
{	
	if(app_fsm._counter > MAX_KEYBUFFER_SIZE - 1){
		keyparser_init();
		return;
	}
 	keyBuffer[app_fsm._counter] = ch;
	app_fsm._counter++;
}

/*
auth_keyBuffer

*/
void pushtoAuthBuffer(int index,int cnt)
{	
	clearAuthKeyBuffer();
	memcpy(auth_keyBuffer,&keyBuffer_bak[index],cnt);
}

static void generalUserPWDcheck(int* cmd, int* code)
{
	int cnt = fsm_get_counter();
	if(cnt==9){
		pushtoAuthBuffer(NORMALADDR ,PWDLEN);
		* cmd = KCMD_PASSWORD_OK;
		* code = KCODE_NULL;
	}else{
		* cmd = KCMD_PASSWORD_FAILED;
		* code = KCODE_NULL;
	}
	clearKeyBufferBak();
}

static void check_admin_pwd(int* cmd, int* code)
{
	int cnt = fsm_get_counter();
	if(cnt == 9){
		pushtoAuthBuffer(ADMINADDR ,PWDLEN);
		if(0==memcmp(admin_PWD,auth_keyBuffer,8)){
			printf("\n\n>>>>>>KCMD_ENTER_ADMIN_OK\n");
			* cmd = KCMD_ENTER_ADMIN_OK;
			* code = KCODE_NULL;
			fsm_set_mode(ADMIN_MODE);
		}else{
			printf("\n\n>>>>>>cannot KCMD_ENTER_ADMIN\n");
			* cmd = KCMD_ENTER_ADMIN_ERROR;
			* code = KCODE_NULL;
			fsm_set_mode(NORMAL_MODE);
		}
	}else{
		* cmd = KCMD_ENTER_ADMIN_FAILED;
		* code = KCODE_NULL;
		fsm_set_mode(NORMAL_MODE);
	}
	// note::if pwd can't match set mode back to NORMAL_MODE
	clearKeyBufferBak();
}

static void checkNewPWD(int* cmd, int* code)
{
	uint8_t i = 1;
	bool isAllNum = true;
	char *fst_pwd = &keyBuffer_bak[1];
	char *sec_pwd = &keyBuffer_bak[10];
	for(;i<9;i++){
		if(!IS_NUM_CHAR(keyBuffer_bak[i]))
			isAllNum = false;
	}
	if(isAllNum && memcmp(fst_pwd,sec_pwd,8)==0){
		ESP_LOGI(TAG, "%s amend pwd ok! |cur mode:%c | \n", __func__,fsm_get_mode());
		pushtoAuthBuffer(AMENDADDR ,PWDLEN);
		* cmd = KCMD_ADMIN_CHANGED_OK;
		* code = KCODE_NULL;
	}else{
		ESP_LOGI(TAG, "%s amend input  invalid fst_pwd != sec_pwd |cur mode:%c | \n", __func__,fsm_get_mode());
		* cmd = KCMD_ADMIN_CHANGED_FAIL;
		* code = KCODE_NULL;
	}
}

static void amend_pwd_check(int* cmd, int* code)
{
	int cnt = fsm_get_counter();
	switch(cnt){
		case 19:
			checkNewPWD(cmd, code);
		break;
		default:
		break;
	}
}

void timeSet_cmd_check(int* cmd, int* code, uint8_t low, uint8_t high)
{	uint8_t temp = KCODE_NULL;
	int cnt = fsm_get_counter();
	switch(cnt){
		case 3:
		if(IS_NUM_CHAR(keyBuffer_bak[1])){
			temp = keyBuffer_bak[1] - KZERO;
			if(temp >= low && temp <= high){
				if(keyBuffer_bak[0] == KFUOR)
					* cmd = KCMD_AUTO_OFF_TIME;
				else
					* cmd = KCMD_ALARM_OFF_TIME;
				* code = (int)temp;
			}
		}
		break;
		case 4:
		if(IS_NUM_CHAR(keyBuffer_bak[1]) && IS_NUM_CHAR(keyBuffer_bak[2]))
		{
			temp = 10*(keyBuffer_bak[1] - KZERO) + keyBuffer_bak[2] - KZERO;
			if(temp >= low && temp <= high){
				* cmd = KCMD_AUTO_OFF_TIME;
				* code = (int)temp;
			}
		}
		break;	
		default:

		break;
	}
}

void simple_cmd_check(int* cmd, int* code)
{
	int temp = KCMD_NULL;
	int cnt = fsm_get_counter();
	switch(cnt){
		case 3:
		if(IS_NUM_CHAR(keyBuffer_bak[1])){
			
			switch (keyBuffer_bak[0])
				{
					case KSIX:
						if(keyBuffer_bak[1]==KZERO)
							temp = KCMD_DOOR_ALARM_OFF;
						else if(keyBuffer_bak[1]==KONE)
							temp = KCMD_DOOR_ALARM_ON;
						* cmd = temp;
						* code = KCODE_NULL;
					break;
					case KSEVEN:
						if(keyBuffer_bak[1]==KZERO)
							temp = KCMD_SERCURE_NORMAL_MODE;
						else if(keyBuffer_bak[1]==KONE)
							temp = KCMD_SERCURE_LOCK_MODE;
						else if(keyBuffer_bak[1]==KTWO)
							temp = KCMD_SERCURE_ALARM_MODE;
						* cmd = temp;
						* code = KCODE_NULL;
					break;
					case KEIGHT:
						if(keyBuffer_bak[1]==KZERO)
							temp = KCMD_KEYBOARD_LED_ON;
						else if(keyBuffer_bak[1]==KONE)
							temp = KCMD_KEYBOARD_LED_OFF;
						else if(keyBuffer_bak[1]==KTWO)
							temp = KCMD_KEYBOARD_LED_AUTO;
						* cmd = temp;
						* code = KCODE_NULL;
					break;
					case KNINE:
						if(keyBuffer_bak[1]==KZERO)
							temp = KCMD_ALARMING_CARD_CANCLE;
						else if(keyBuffer_bak[1]==KONE)
							temp = KCMD_ALARMING_USRPWD_CANCLE;
						else if(keyBuffer_bak[1]==KTWO)
							temp = KCMD_ALARMING_CLOSEDOOR_CANCLE;
						* cmd = temp;
						* code = KCODE_NULL;
					break;
					default:
					break;
				}
		}
		break;	
		case 4:
		if(IS_NUM_CHAR(keyBuffer_bak[1]) && IS_NUM_CHAR(keyBuffer_bak[2]))
		{	
			if(keyBuffer_bak[0] == KTHREE){
			switch (keyBuffer_bak[1])
				{
					case KONE:
						* cmd = (keyBuffer_bak[2]==KONE)?KCMD_CARD_MODE_ON:(keyBuffer_bak[2]==KZERO)?KCMD_CARD_MODE_OFF:KCMD_NULL;
						* code = KCODE_NULL;
					break;
					case KTWO:
						* cmd = (keyBuffer_bak[2]==KONE)?KCMD_PASSWORD_MODE_ON:(keyBuffer_bak[2]==KZERO)?KCMD_PASSWORD_MODE_OFF:KCMD_NULL;
						* code = KCODE_NULL;
					break;
					case KTHREE:
						* cmd = (keyBuffer_bak[2]==KONE)?KCMD_BT_MODE_ON:(keyBuffer_bak[2]==KZERO)?KCMD_BT_MODE_OFF:KCMD_NULL;
						* code = KCODE_NULL;
					break;
					default:
					break;
				}
			}
		}
		break;
		default:
		break;
	}
}

void add_user_check()
{

}

void delete_user_check()
{


}

static void check_opt_cmd(int* cmd, int* code)
{	
	admin_op oprt = fsm_get_admin_op();
	
	switch(oprt)
	{
		case AMEND_PWD_OP:
			amend_pwd_check(cmd, code);
		break;
		case ADD_USR_OP:
			//add_user_check();
		break;
		case DELETE_USR_OP:
			//delete_user_check();
		break;
		case OPEN_MODE_OP:
			simple_cmd_check(cmd, code);
		break;
		case DOOR_OFF_T_OP:
			timeSet_cmd_check(cmd, code, AUTO_OFF_TIME_LOW, AUTO_OFF_TIME_HIGH);
		break;
		case ALARM_T_OP:
			timeSet_cmd_check(cmd, code, ALARM_OFF_TIME_LOW, ALARM_OFF_TIME_HIGH);
		break;
		case ALARM_SW_OP:
			simple_cmd_check(cmd, code);
		break;
		case MAIN_MODE_OP:
			simple_cmd_check(cmd, code);
		break;
		case LED_SW_OP:
			simple_cmd_check(cmd, code);
		break;
		case CANCLE_ALARM_OP:
			simple_cmd_check(cmd, code);
		break;
		default:
			//ESP_LOGI(TAG, "cur mode:%c |err oprt:%c \n", fsm_get_mode(),oprt);
		break;
	}
	
	clearKeyBufferBak();
}


static void checkBak(int* cmd, int* code)
{
	k_mode_t mode = fsm_get_mode();
	switch(mode)
	{
		case NORMAL_MODE:
		generalUserPWDcheck(cmd, code);
		break;
		case CRITICAL_MODE:
		check_admin_pwd(cmd, code);
		break;	
		case ADMIN_MODE:
		check_opt_cmd(cmd, code);
		break;
		default:

		break;
	}
}

void copykeyBuffertoBak(int* cmd, int* code)
{
	admin_op oprt = fsm_get_admin_op();
	int cnt = fsm_get_counter();
	switch(oprt){
		case NONE_OP:// input '*' again in admin mode.
			*cmd = KCMD_INVALID_OP;
			*code = KCODE_NULL;
		break;
		case AMEND_PWD_OP:
			if(cnt < 11){
			return;
			}else if(cnt != 19){
				*cmd = KCMD_INVALID_OP;
				*code = KCODE_NULL;
			}
		break;
		case OPEN_MODE_OP:
			if(cnt != 4){
				*cmd = KCMD_INVALID_OP;
				*code = KCODE_NULL;
			}
		break;
		case DOOR_OFF_T_OP:
			if(cnt < 3 || cnt > 4){
				*cmd = KCMD_INVALID_OP;
				*code = KCODE_NULL;
			}
		break;
		case ALARM_T_OP:
		case ALARM_SW_OP:
		case MAIN_MODE_OP:
		case LED_SW_OP:
		case CANCLE_ALARM_OP:
			if(cnt != 3){
				*cmd = KCMD_INVALID_OP;
				*code = KCODE_NULL;
			}
		break;		
		default:
		break;
	}
	memcpy(keyBuffer_bak,keyBuffer,cnt);
	checkBak(cmd, code);
	clear_fsm_counter();
	clearKeyBuffer();
}

void handle_star_input(char ch, int* cmd, int* code)
{	
	int cnt = fsm_get_counter();
	k_mode_t mode = fsm_get_mode();
	switch(mode)
	{
		case NORMAL_MODE:
			if(cnt == 0){
				fsm_set_mode(CRITICAL_MODE);
				*cmd = KCMD_ENTER_ADMIN_START;
				*code = KCODE_NULL;
			}
		break;
		case CRITICAL_MODE:
		case ADMIN_MODE:
		*cmd = KCMD_INVALID_OP;
		*code = KCODE_NULL;
		break;
		
		default:
		break;
	}
}

void handle_sharp_input(char keyInput,int* cmd, int* code)
{	
	k_mode_t mode = fsm_get_mode();
	int cnt = fsm_get_counter();
	if(cnt<8 && mode != ADMIN_MODE){
		*cmd = KCMD_INVALID_OP;
		*code = KCODE_NULL;
		fsm_set_mode(NORMAL_MODE);
		keyparser_init();
		return ;
	}
	writeKeyBuffer(keyInput);
	copykeyBuffertoBak(cmd , code);
}

void handle_numb_input(char keyInput, int* cmd, int* code)
{	
	k_mode_t mode = fsm_get_mode();
	int cnt = fsm_get_counter();
	switch(mode)
	{
		case NORMAL_MODE:
			if(cnt == 0){
				* cmd = KCMD_PASSWORD_START;
				* code = KCODE_NULL;
			}else{
				* cmd = KCMD_NULL;
				* code = KCODE_NULL;
			}
			
		break;
		case CRITICAL_MODE:
			* cmd = KCMD_NULL;
			* code = KCODE_NULL;
		break;
		case ADMIN_MODE:
			if(keyInput == KZERO && cnt == 0){
				*cmd = KCMD_ADMIN_CHANGED_START;
				*code = KCODE_NULL;
			}else{
				* cmd = KCMD_NULL;
				* code = KCODE_NULL;
			}
		break;
		default:

		break;
	}
	writeKeyBuffer(keyInput);
}

void keyparser_process_char(char ch, int* cmd, int* code)
{	
	switch(ch)
	{
		case KSTAR:
			handle_star_input(ch,cmd,code);
			//ESP_LOGI(TAG, " *** %s \n\n", __func__);
		break;
		case KSHARP:
			handle_sharp_input(ch,cmd,code);
			//ESP_LOGI(TAG, " ### %s \n\n", __func__);
		break;
		default:
			handle_numb_input(ch,cmd,code);
			//ESP_LOGI(TAG, " num %s \n\n", __func__);
		break;
	}
}

const char* keyparser_buffer(void)
{
	return (const char*)auth_keyBuffer;
}

void keyparser_set_admin_pwd(const char* pwd,int size)
{
	if(size!=8){
			return;
	}else{
		memcpy(admin_PWD , pwd ,8);
		admin_PWD[8] = '\0';
		//printf("adminpwd:%s\n",admin_PWD);
	}
}

bool keyparser_is_admin(void)
{

	return false;
}

int  keyparser_errcode(void)
{
	return 0;
}

int  keyparser_reset(void)
{
	app_fsm._admin_op = NONE_OP;
	app_fsm._mode = NORMAL_MODE;
	keyparser_init();
	return 0;
}

void keyparser_set_card_id(const char* kid)
{


}

const char* keyparser_get_card_id(void)
{
	return NULL;
}

