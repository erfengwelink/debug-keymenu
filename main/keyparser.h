#ifndef KEY_PARSER
#define KEY_PARSER

#include <stdbool.h>
#include <stdint.h>

#define KCODE_NULL 	0xff


typedef enum key_cmd_t {
    KCMD_NULL,
		
    KCMD_PASSWORD_START, 	//Normal
    KCMD_PASSWORD_OK, 		//Normal	--->need to read buffer
    KCMD_PASSWORD_FAILED, 	//Normal
    
    KCMD_INVALID_OP,
    
    KCMD_ENTER_ADMIN_START,
    KCMD_ENTER_ADMIN_ERROR,				//wrong pwd
    KCMD_ENTER_ADMIN_OK,				//ok pwd	//--->need to read buffer
    KCMD_ENTER_ADMIN_FAILED,			//pwd bytes not right
    
    KCMD_ADMIN_CHANGED_START,
    KCMD_ADMIN_CHANGED_OK,				//--->need to read buffer //10
    KCMD_ADMIN_CHANGED_FAIL,			//head & tail cannot match
    
    KCMD_ADD_CARD_START,
    KCMD_ADD_CARD_ID, //TBD
    KCMD_ADD_CARD_END,
    
    KCMD_DEL_CARD_START,
    KCMD_DEL_CARD_ID,
    KCMD_DEL_CARD_END,
    KCMD_DEL_ALL_CARD,
    
    KCMD_CARD_MODE_OFF,
    KCMD_CARD_MODE_ON, 			//311#, 310#..3*2
    KCMD_PASSWORD_MODE_OFF,
    KCMD_PASSWORD_MODE_ON, 		//321#, 320#
    KCMD_BT_MODE_OFF,
    KCMD_BT_MODE_ON,   			//331#, 330#
    
    KCMD_AUTO_OFF_TIME,			//40#~499#		
    
    KCMD_ALARM_OFF_TIME,		//50#~53#
    
    KCMD_DOOR_ALARM_OFF,		//60#
    KCMD_DOOR_ALARM_ON,			//61#..2
    
    KCMD_SERCURE_NORMAL_MODE,	//70#..3
    KCMD_SERCURE_LOCK_MODE,		//71#
    KCMD_SERCURE_ALARM_MODE,	//72#
    
    KCMD_KEYBOARD_LED_ON,		//80#..3
    KCMD_KEYBOARD_LED_OFF,		//81#
    KCMD_KEYBOARD_LED_AUTO,		//82#
    
    KCMD_ALARMING_CARD_CANCLE,	//90#..3
    KCMD_ALARMING_USRPWD_CANCLE,	//91#
    KCMD_ALARMING_CLOSEDOOR_CANCLE,	//92#
} key_cmd_t;


int keyparser_init(void);
void keyparser_process_char(char ch, int* cmd, int* code);
const char* keyparser_buffer(void);

void keyparser_set_admin_pwd(const char* pwd,int size);
bool keyparser_is_admin(void);
int  keyparser_errcode(void);
int  keyparser_reset(void); //New

void keyparser_set_card_id(const char* kid); //TBD
const char* keyparser_get_card_id(void); //TBD


#endif//
