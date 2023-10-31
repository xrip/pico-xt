#pragma once
#include "strings.h"
#include "stdio.h"

#include <stdbool.h>

#define KBD_CLOCK_PIN	(0)
#define KBD_DATA_PIN	(1)
#define KBD_BUFFER_SIZE 128


extern uint8_t kbloop;

void KeyboardHandler();//uint /*gpio*/, uint32_t /*event_mask*/
uint32_t ps2getcode(void);


void Init_kbd(void);
void Deinit_kbd(void);



struct ps2_struct_group
{
    unsigned char character ;
    unsigned char make ;
    unsigned is_char ;
    unsigned char xt_make ;
} ;


static struct ps2_struct_group ps2_group1[] =
        {
                {'a', 0x1C, 1, 0x1E},
                {'b', 0x32, 1, 0x30},
                {'c', 0x21, 1, 0x2E},
                {'d', 0x23, 1, 0x20},
                {'e', 0x24, 1, 0x12},
                {'f', 0x2B, 1, 0x21},
                {'g', 0x34, 1, 0x22},
                {'h', 0x33, 1, 0x23},
                {'i', 0x43, 1, 0x17},
                {'j', 0x3B, 1, 0x24},
                {'k', 0x42, 1, 0x25},
                {'l', 0x4B, 1, 0x26},
                {'m', 0x3A, 1, 0x32},
                {'n', 0x31, 1, 0x31},
                {'o', 0x44, 1, 0x18},
                {'p', 0x4D, 1, 0x19},
                {'q', 0x15, 1, 0x10},
                {'r', 0x2D, 1, 0x13},
                {'s', 0x1B, 1, 0x1F},
                {'t', 0x2C, 1, 0x14},
                {'u', 0x3C, 1, 0x16},
                {'v', 0x2A, 1, 0x2F},
                {'w', 0x1D, 1, 0x11},
                {'x', 0x22, 1, 0x2D},
                {'y', 0x35, 1, 0x15},
                {'z', 0x1A, 1, 0x2C},
                {'0', 0x45, 1, 0x0B},
                {'1', 0x16, 1, 0x02},
                {'2', 0x1E, 1, 0x03},
                {'3', 0x26, 1, 0x04},
                {'4', 0x25, 1, 0x05},
                {'5', 0x2E, 1, 0x06},
                {'6', 0x36, 1, 0x07},
                {'7', 0x3D, 1, 0x08},
                {'8', 0x3E, 1, 0x09},
                {'9', 0x46, 1, 0x0A},
                {'`', 0x0E, 1, 0x29},
                {'-', 0x4E, 1, 0x0C},
                {'=', 0x55, 1, 0x0D},
                {'\\', 0x5D, 1, 0x2B},
                {'\b', 0x66, 0, 0x0E}, // backsapce
                {' ', 0x29, 1, 0x39}, // space
                {'\t', 0x0D, 0, 0x0F}, // tab
                {' ', 0x58, 0, 0x3A}, // caps
                {' ', 0x12, 0, 0x2A}, // left shift
                {' ', 0x14, 0, 0x1D}, // left ctrl
                {' ', 0x11, 0, 0x38}, // left alt
                {' ', 0x59, 0, 0x36}, // right shift
                {'\n', 0x5A, 1, 0x1C}, // enter
                {' ', 0x76, 0, 0x01}, // esc
                {' ', 0x05, 0, 0x3B}, // F1
                {' ', 0x06, 0, 0x3C}, // F2
                {' ', 0x04, 0, 0x3D}, // F3
                {' ', 0x0C, 0, 0x3E}, // F4
                {' ', 0x03, 0, 0x3F}, // F5
                {' ', 0x0B, 0, 0x40}, // F6
                {' ', 0x83, 0, 0x41}, // F7
                {' ', 0x0A, 0, 0x42}, // F8
                {' ', 0x01, 0, 0x43}, // f9
                {' ', 0x09, 0, 0x44}, // f10
                {' ', 0x78, 0, 0x57}, // f11
                {' ', 0x07, 0, 0x58}, // f12
                {' ', 0x7E, 0, 0x46}, // SCROLL
                {'[', 0x54, 1, 0x1A},
                {' ', 0x77, 0, 0x45}, // Num Lock
                {'*', 0x7C, 1, 0x37}, // Keypad *
                {'-', 0x7B, 1, 0x4A}, // Keypad -
                {'+', 0x79, 1, 0x4E}, // Keypad +
                {'.', 0x71, 1, 0x53}, // Keypad .
                {'0', 0x70, 1, 0x52}, // Keypad 0
                {'1', 0x69, 1, 0x4F}, // Keypad 1
                {'2', 0x72, 1, 0x50}, // Keypad 2
                {'3', 0x7A, 1, 0x51}, // Keypad 3
                {'4', 0x6B, 1, 0x4B}, // Keypad 4
                {'5', 0x73, 1, 0x4C}, // Keypad 5
                {'6', 0x74, 1, 0x4D}, // Keypad 6
                {'7', 0x6C, 1, 0x47}, // Keypad 7
                {'8', 0x75, 1, 0x48}, // Keypad 8
                {'9', 0x7D, 1, 0x49}, // Keypad 9
                {']', 0x5B, 1, 0x1B},
                {';', 0x4C, 1, 0x27},
                {'\'', 0x52, 1, 0x28},
                {',', 0x41, 1, 0x33},
                {'.', 0x49, 1, 0x34},
                {'/', 0x4A, 1, 0x35},
        } ;

static struct ps2_struct_group ps2_group2[] =
        {
                {' ', 0x5B, 0, 0x1F}, // left gui
                {' ', 0x1D, 0, 0x14}, // right ctrl
                {' ', 0x5C, 0, 0x27}, // right gui
                {' ', 0x38, 0, 0x11}, // right alt
                {' ', 0x5D, 0, 0x2F}, // apps
                {' ', 0x52, 0, 0x70}, // insert
                {' ', 0x47, 0, 0x6C}, // home
                {' ', 0x49, 0, 0x7D}, // page up
                {' ', 0x53, 0, 0x71}, // delete
                {' ', 0x4F, 0, 0x69}, // end
                {' ', 0x51, 0, 0x7A}, // page down
                {' ', 0x48, 0, 0x75}, // u arrow
                {' ', 0x4B, 0, 0x6B}, // l arrow
                {' ', 0x50, 0, 0x72}, // d arrow
                {' ', 0x4D, 0, 0x74}, // r arrow
                {' ', 0x1C, 0, 0x5A}, // kp en
        } ;


// ===================================
// блок 0 буквы
#define KB_U0_A (1<<0)
#define KB_U0_B (1<<1)
#define KB_U0_C (1<<2)
#define KB_U0_D (1<<3)
#define KB_U0_E (1<<4)
#define KB_U0_F (1<<5)
#define KB_U0_G (1<<6)
#define KB_U0_H (1<<7)
#define KB_U0_I (1<<8)
#define KB_U0_J (1<<9)
#define KB_U0_K (1<<10)
#define KB_U0_L (1<<11)
#define KB_U0_M (1<<12)
#define KB_U0_N (1<<13)
#define KB_U0_O (1<<14)
#define KB_U0_P (1<<15)
#define KB_U0_Q (1<<16)
#define KB_U0_R (1<<17)
#define KB_U0_S (1<<18)
#define KB_U0_T (1<<19)
#define KB_U0_U (1<<20)
#define KB_U0_V (1<<21)
#define KB_U0_W (1<<22)
#define KB_U0_X (1<<23)
#define KB_U0_Y (1<<24)
#define KB_U0_Z (1<<25)

#define KB_U0_SEMICOLON (1<<26)
#define KB_U0_QUOTE (1<<27)
#define KB_U0_COMMA (1<<28)
#define KB_U0_PERIOD (1<<29)
#define KB_U0_LEFT_BR (1<<30)
#define KB_U0_RIGHT_BR (1<<31)

// блок 1 цифры и контролы


#define KB_U1_0 (1<<0)
#define KB_U1_1 (1<<1)
#define KB_U1_2 (1<<2)
#define KB_U1_3 (1<<3)
#define KB_U1_4 (1<<4)
#define KB_U1_5 (1<<5)
#define KB_U1_6 (1<<6)
#define KB_U1_7 (1<<7)
#define KB_U1_8 (1<<8)
#define KB_U1_9 (1<<9)

#define KB_U1_ENTER (1<<10)
#define KB_U1_SLASH (1<<11)
#define KB_U1_MINUS (1<<12)

#define KB_U1_EQUALS (1<<13)
#define KB_U1_BACKSLASH (1<<14)
#define KB_U1_CAPS_LOCK (1<<15)
#define KB_U1_TAB (1<<16)
#define KB_U1_BACK_SPACE (1<<17)
#define KB_U1_ESC (1<<18)
#define KB_U1_TILDE (1<<19)
#define KB_U1_MENU (1<<20)

#define KB_U1_L_SHIFT (1<<21)
#define KB_U1_L_CTRL (1<<22)
#define KB_U1_L_ALT (1<<23)
#define KB_U1_L_WIN (1<<24)
#define KB_U1_R_SHIFT (1<<25)
#define KB_U1_R_CTRL (1<<26)
#define KB_U1_R_ALT (1<<27)
#define KB_U1_R_WIN (1<<28)

#define KB_U1_SPACE (1<<29)




// блок 2 нум клавиши и доп клавиши



#define KB_U2_NUM_0 (1<<0)
#define KB_U2_NUM_1 (1<<1)
#define KB_U2_NUM_2 (1<<2)
#define KB_U2_NUM_3 (1<<3)
#define KB_U2_NUM_4 (1<<4)
#define KB_U2_NUM_5 (1<<5)
#define KB_U2_NUM_6 (1<<6)
#define KB_U2_NUM_7 (1<<7)
#define KB_U2_NUM_8 (1<<8)
#define KB_U2_NUM_9 (1<<9)

#define KB_U2_NUM_ENTER (1<<10)
#define KB_U2_NUM_SLASH (1<<11)
#define KB_U2_NUM_MINUS (1<<12)

#define KB_U2_NUM_PLUS (1<<13)
#define KB_U2_NUM_MULT (1<<14)
#define KB_U2_NUM_PERIOD  (1<<15)
#define KB_U2_NUM_LOCK (1<<16)


#define KB_U2_DELETE (1<<17)
#define KB_U2_SCROLL_LOCK (1<<18)
#define KB_U2_PAUSE_BREAK (1<<19)
#define KB_U2_INSERT (1<<20)
#define KB_U2_HOME (1<<21)
#define KB_U2_PAGE_UP (1<<22)
#define KB_U2_PAGE_DOWN (1<<23)

#define KB_U2_PRT_SCR (1<<24)
#define KB_U2_END (1<<25)
#define KB_U2_UP (1<<26)
#define KB_U2_DOWN (1<<27)
#define KB_U2_LEFT (1<<28)
#define KB_U2_RIGHT (1<<29)

// блок 3 F клавиши и прочие допы


#define KB_U3_ (1<<0)
#define KB_U3_F1 (1<<1)
#define KB_U3_F2 (1<<2)
#define KB_U3_F3 (1<<3)
#define KB_U3_F4 (1<<4)
#define KB_U3_F5 (1<<5)
#define KB_U3_F6 (1<<6)
#define KB_U3_F7 (1<<7)
#define KB_U3_F8 (1<<8)
#define KB_U3_F9 (1<<9)
#define KB_U3_F10 (1<<10)
#define KB_U3_F11 (1<<11)
#define KB_U3_F12 (1<<12)


extern uint32_t kbd_statuses[4];
//макросы сокращения клавиатурных команд
#define KBD_A				kbd_statuses[0]&KB_U0_A
#define KBD_B				kbd_statuses[0]&KB_U0_B
#define KBD_C				kbd_statuses[0]&KB_U0_C
#define KBD_D				kbd_statuses[0]&KB_U0_D
#define KBD_E				kbd_statuses[0]&KB_U0_E
#define KBD_F				kbd_statuses[0]&KB_U0_F
#define KBD_G				kbd_statuses[0]&KB_U0_G
#define KBD_H				kbd_statuses[0]&KB_U0_H
#define KBD_I				kbd_statuses[0]&KB_U0_I
#define KBD_J				kbd_statuses[0]&KB_U0_J
#define KBD_K				kbd_statuses[0]&KB_U0_K
#define KBD_L				kbd_statuses[0]&KB_U0_L
#define KBD_M				kbd_statuses[0]&KB_U0_M
#define KBD_N				kbd_statuses[0]&KB_U0_N
#define KBD_O				kbd_statuses[0]&KB_U0_O
#define KBD_P				kbd_statuses[0]&KB_U0_P
#define KBD_Q				kbd_statuses[0]&KB_U0_Q
#define KBD_R				kbd_statuses[0]&KB_U0_R
#define KBD_S				kbd_statuses[0]&KB_U0_S
#define KBD_T				kbd_statuses[0]&KB_U0_T
#define KBD_U				kbd_statuses[0]&KB_U0_U
#define KBD_V				kbd_statuses[0]&KB_U0_V
#define KBD_W				kbd_statuses[0]&KB_U0_W
#define KBD_X				kbd_statuses[0]&KB_U0_X
#define KBD_Y				kbd_statuses[0]&KB_U0_Y
#define KBD_Z				kbd_statuses[0]&KB_U0_Z

#define KBD_SEMICOLON		kbd_statuses[0]&KB_U0_SEMICOLON
#define KBD_QUOTE			kbd_statuses[0]&KB_U0_QUOTE
#define KBD_COMMA			kbd_statuses[0]&KB_U0_COMMA
#define KBD_PERIOD			kbd_statuses[0]&KB_U0_PERIOD
#define KBD_LEFT_BR			kbd_statuses[0]&KB_U0_LEFT_BR
#define KBD_RIGHT_BR		kbd_statuses[0]&KB_U0_RIGHT_BR


#define KBD_0				kbd_statuses[1]&KB_U1_0
#define KBD_1				kbd_statuses[1]&KB_U1_1
#define KBD_2				kbd_statuses[1]&KB_U1_2
#define KBD_3				kbd_statuses[1]&KB_U1_3
#define KBD_4				kbd_statuses[1]&KB_U1_4
#define KBD_5				kbd_statuses[1]&KB_U1_5
#define KBD_6				kbd_statuses[1]&KB_U1_6
#define KBD_7				kbd_statuses[1]&KB_U1_7
#define KBD_8				kbd_statuses[1]&KB_U1_8
#define KBD_9				kbd_statuses[1]&KB_U1_9

#define KBD_ENTER			kbd_statuses[1]&KB_U1_ENTER
#define KBD_SLASH			kbd_statuses[1]&KB_U1_SLASH
#define KBD_MINUS			kbd_statuses[1]&KB_U1_MINUS
#define KBD_EQUALS			kbd_statuses[1]&KB_U1_EQUALS
#define KBD_BACKSLASH		kbd_statuses[1]&KB_U1_BACKSLASH
#define KBD_CAPS_LOCK		kbd_statuses[1]&KB_U1_CAPS_LOCK
#define KBD_TAB				kbd_statuses[1]&KB_U1_TAB
#define KBD_BACK_SPACE		kbd_statuses[1]&KB_U1_BACK_SPACE
#define KBD_ESC				kbd_statuses[1]&KB_U1_ESC
#define KBD_TILDE			kbd_statuses[1]&KB_U1_TILDE
#define KBD_MENU			kbd_statuses[1]&KB_U1_MENU
#define KBD_L_SHIFT			kbd_statuses[1]&KB_U1_L_SHIFT
#define KBD_L_CTRL			kbd_statuses[1]&KB_U1_L_CTRL
#define KBD_L_ALT			kbd_statuses[1]&KB_U1_L_ALT
#define KBD_L_WIN			kbd_statuses[1]&KB_U1_L_WIN
#define KBD_R_SHIFT			kbd_statuses[1]&KB_U1_R_SHIFT
#define KBD_R_CTRL			kbd_statuses[1]&KB_U1_R_CTRL
#define KBD_R_ALT			kbd_statuses[1]&KB_U1_R_ALT
#define KBD_R_WIN			kbd_statuses[1]&KB_U1_R_WIN
#define KBD_SPACE			kbd_statuses[1]&KB_U1_SPACE


#define KBD_NUM_0			kbd_statuses[2]&KB_U2_NUM_0
#define KBD_NUM_1			kbd_statuses[2]&KB_U2_NUM_1
#define KBD_NUM_2			kbd_statuses[2]&KB_U2_NUM_2
#define KBD_NUM_3			kbd_statuses[2]&KB_U2_NUM_3
#define KBD_NUM_4			kbd_statuses[2]&KB_U2_NUM_4
#define KBD_NUM_5			kbd_statuses[2]&KB_U2_NUM_5
#define KBD_NUM_6			kbd_statuses[2]&KB_U2_NUM_6
#define KBD_NUM_7			kbd_statuses[2]&KB_U2_NUM_7
#define KBD_NUM_8			kbd_statuses[2]&KB_U2_NUM_8
#define KBD_NUM_9			kbd_statuses[2]&KB_U2_NUM_9
#define KBD_NUM_ENTER		kbd_statuses[2]&KB_U2_NUM_ENTER
#define KBD_NUM_SLASH		kbd_statuses[2]&KB_U2_NUM_SLASH
#define KBD_NUM_MINUS		kbd_statuses[2]&KB_U2_NUM_MINUS
#define KBD_NUM_PLUS		kbd_statuses[2]&KB_U2_NUM_PLUS
#define KBD_NUM_MULT		kbd_statuses[2]&KB_U2_NUM_MULT
#define KBD_NUM_PERIOD		kbd_statuses[2]&KB_U2_NUM_PERIOD
#define KBD_NUM_LOCK		kbd_statuses[2]&KB_U2_NUM_LOCK

#define KBD_DELETE			kbd_statuses[2]&KB_U2_DELETE
#define KBD_SCROLL_LOCK		kbd_statuses[2]&KB_U2_SCROLL_LOCK
#define KBD_PAUSE_BREAK		kbd_statuses[2]&KB_U2_PAUSE_BREAK
#define KBD_INSERT			kbd_statuses[2]&KB_U2_INSERT
#define KBD_HOME			kbd_statuses[2]&KB_U2_HOME
#define KBD_PAGE_UP			kbd_statuses[2]&KB_U2_PAGE_UP
#define KBD_PAGE_DOWN		kbd_statuses[2]&KB_U2_PAGE_DOWN
#define KBD_PRT_SCR			kbd_statuses[2]&KB_U2_PRT_SCR
#define KBD_END				kbd_statuses[2]&KB_U2_END
#define KBD_UP				kbd_statuses[2]&KB_U2_UP
#define KBD_DOWN			kbd_statuses[2]&KB_U2_DOWN
#define KBD_LEFT			kbd_statuses[2]&KB_U2_LEFT
#define KBD_RIGHT			kbd_statuses[2]&KB_U2_RIGHT

#define KBD_F1				kbd_statuses[3]&KB_U3_F1
#define KBD_F2				kbd_statuses[3]&KB_U3_F2
#define KBD_F3				kbd_statuses[3]&KB_U3_F3
#define KBD_F4				kbd_statuses[3]&KB_U3_F4
#define KBD_F5				kbd_statuses[3]&KB_U3_F5
#define KBD_F6				kbd_statuses[3]&KB_U3_F6
#define KBD_F7				kbd_statuses[3]&KB_U3_F7
#define KBD_F8				kbd_statuses[3]&KB_U3_F8
#define KBD_F9				kbd_statuses[3]&KB_U3_F9
#define KBD_F10				kbd_statuses[3]&KB_U3_F10
#define KBD_F11				kbd_statuses[3]&KB_U3_F11
#define KBD_F12				kbd_statuses[3]&KB_U3_F12

#define KBD_PRESS			(kbd_statuses[0]!=0)||(kbd_statuses[1]!=0)||(kbd_statuses[2]!=0)||(kbd_statuses[3]!=0)
#define KBD_RELEASE			(kbd_statuses[0]==0)&&(kbd_statuses[1]==0)&&(kbd_statuses[2]==0)&&(kbd_statuses[3]==0)
