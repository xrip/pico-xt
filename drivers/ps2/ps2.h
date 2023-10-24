#pragma once
#include "strings.h"
#include "stdio.h"

#include <stdbool.h>

#define KBD_CLOCK_PIN	(0)
#define KBD_DATA_PIN	(1)
#define KBD_BUFFER_SIZE 46

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

uint8_t get_scan_code(void);
void translate_scancode(uint8_t code, bool is_press, bool is_e0, bool is_e1);
void KeyboardHandler();//uint /*gpio*/, uint32_t /*event_mask*/


void kbd_to_str(char *str_buf);

void Init_kbd(void);
void Deinit_kbd(void);
bool decode_kbd();


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


enum {
    SDL_SCANCODE_UNKNOWN = 0,

    /**
     *  \name Usage page 0x07
     *
     *  These values are from usage page 0x07 (USB keyboard page).
     */
    /* @{ */

    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_B = 5,
    SDL_SCANCODE_C = 6,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9,
    SDL_SCANCODE_G = 10,
    SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12,
    SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14,
    SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16,
    SDL_SCANCODE_N = 17,
    SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20,
    SDL_SCANCODE_R = 21,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24,
    SDL_SCANCODE_V = 25,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,

    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_2 = 31,
    SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35,
    SDL_SCANCODE_7 = 36,
    SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38,
    SDL_SCANCODE_0 = 39,

    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,

    SDL_SCANCODE_MINUS = 45,
    SDL_SCANCODE_EQUALS = 46,
    SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48,
    SDL_SCANCODE_BACKSLASH = 49, /**< Located at the lower left of the return
                                  *   key on ISO keyboards and at the right end
                                  *   of the QWERTY row on ANSI keyboards.
                                  *   Produces REVERSE SOLIDUS (backslash) and
                                  *   VERTICAL LINE in a US layout, REVERSE
                                  *   SOLIDUS and VERTICAL LINE in a UK Mac
                                  *   layout, NUMBER SIGN and TILDE in a UK
                                  *   Windows layout, DOLLAR SIGN and POUND SIGN
                                  *   in a Swiss German layout, NUMBER SIGN and
                                  *   APOSTROPHE in a German layout, GRAVE
                                  *   ACCENT and POUND SIGN in a French Mac
                                  *   layout, and ASTERISK and MICRO SIGN in a
                                  *   French Windows layout.
                                  */
    SDL_SCANCODE_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
                                  *   instead of 49 for the same key, but all
                                  *   OSes I've seen treat the two codes
                                  *   identically. So, as an implementor, unless
                                  *   your keyboard generates both of those
                                  *   codes and your OS treats them differently,
                                  *   you should generate SDL_SCANCODE_BACKSLASH
                                  *   instead of this code. As a user, you
                                  *   should not rely on this code because SDL
                                  *   will never generate it with most (all?)
                                  *   keyboards.
                                  */
    SDL_SCANCODE_SEMICOLON = 51,
    SDL_SCANCODE_APOSTROPHE = 52,
    SDL_SCANCODE_GRAVE = 53, /**< Located in the top left corner (on both ANSI
                              *   and ISO keyboards). Produces GRAVE ACCENT and
                              *   TILDE in a US Windows layout and in US and UK
                              *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                              *   and NOT SIGN in a UK Windows layout, SECTION
                              *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                              *   layouts on ISO keyboards, SECTION SIGN and
                              *   DEGREE SIGN in a Swiss German layout (Mac:
                              *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                              *   DEGREE SIGN in a German layout (Mac: only on
                              *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                              *   French Windows layout, COMMERCIAL AT and
                              *   NUMBER SIGN in a French Mac layout on ISO
                              *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                              *   SIGN in a Swiss German, German, or French Mac
                              *   layout on ANSI keyboards.
                              */
    SDL_SCANCODE_COMMA = 54,
    SDL_SCANCODE_PERIOD = 55,
    SDL_SCANCODE_SLASH = 56,

    SDL_SCANCODE_CAPSLOCK = 57,

    SDL_SCANCODE_F1 = 58,
    SDL_SCANCODE_F2 = 59,
    SDL_SCANCODE_F3 = 60,
    SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62,
    SDL_SCANCODE_F6 = 63,
    SDL_SCANCODE_F7 = 64,
    SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66,
    SDL_SCANCODE_F10 = 67,
    SDL_SCANCODE_F11 = 68,
    SDL_SCANCODE_F12 = 69,

    SDL_SCANCODE_PRINTSCREEN = 70,
    SDL_SCANCODE_SCROLLLOCK = 71,
    SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
                                   does send code 73, not 117) */
    SDL_SCANCODE_HOME = 74,
    SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_DELETE = 76,
    SDL_SCANCODE_END = 77,
    SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,

    SDL_SCANCODE_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
                                     */
    SDL_SCANCODE_KP_DIVIDE = 84,
    SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86,
    SDL_SCANCODE_KP_PLUS = 87,
    SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_2 = 90,
    SDL_SCANCODE_KP_3 = 91,
    SDL_SCANCODE_KP_4 = 92,
    SDL_SCANCODE_KP_5 = 93,
    SDL_SCANCODE_KP_6 = 94,
    SDL_SCANCODE_KP_7 = 95,
    SDL_SCANCODE_KP_8 = 96,
    SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98,
    SDL_SCANCODE_KP_PERIOD = 99,

    SDL_SCANCODE_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
                                        *   keyboards have over ANSI ones,
                                        *   located between left shift and Y.
                                        *   Produces GRAVE ACCENT and TILDE in a
                                        *   US or UK Mac layout, REVERSE SOLIDUS
                                        *   (backslash) and VERTICAL LINE in a
                                        *   US or UK Windows layout, and
                                        *   LESS-THAN SIGN and GREATER-THAN SIGN
                                        *   in a Swiss German, German, or French
                                        *   layout. */
    SDL_SCANCODE_APPLICATION = 101, /**< windows contextual menu, compose */
    SDL_SCANCODE_POWER = 102, /**< The USB document says this is a status flag,
                               *   not a physical key - but some Mac keyboards
                               *   do have a power key. */
    SDL_SCANCODE_KP_EQUALS = 103,
    SDL_SCANCODE_F13 = 104,
    SDL_SCANCODE_F14 = 105,
    SDL_SCANCODE_F15 = 106,
    SDL_SCANCODE_F16 = 107,
    SDL_SCANCODE_F17 = 108,
    SDL_SCANCODE_F18 = 109,
    SDL_SCANCODE_F19 = 110,
    SDL_SCANCODE_F20 = 111,
    SDL_SCANCODE_F21 = 112,
    SDL_SCANCODE_F22 = 113,
    SDL_SCANCODE_F23 = 114,
    SDL_SCANCODE_F24 = 115,
    SDL_SCANCODE_EXECUTE = 116,
    SDL_SCANCODE_HELP = 117,
    SDL_SCANCODE_MENU = 118,
    SDL_SCANCODE_SELECT = 119,
    SDL_SCANCODE_STOP = 120,
    SDL_SCANCODE_AGAIN = 121,   /**< redo */
    SDL_SCANCODE_UNDO = 122,
    SDL_SCANCODE_CUT = 123,
    SDL_SCANCODE_COPY = 124,
    SDL_SCANCODE_PASTE = 125,
    SDL_SCANCODE_FIND = 126,
    SDL_SCANCODE_MUTE = 127,
    SDL_SCANCODE_VOLUMEUP = 128,
    SDL_SCANCODE_VOLUMEDOWN = 129,
/* not sure whether there's a reason to enable these */
/*     SDL_SCANCODE_LOCKINGCAPSLOCK = 130,  */
/*     SDL_SCANCODE_LOCKINGNUMLOCK = 131, */
/*     SDL_SCANCODE_LOCKINGSCROLLLOCK = 132, */
    SDL_SCANCODE_KP_COMMA = 133,
    SDL_SCANCODE_KP_EQUALSAS400 = 134,

    SDL_SCANCODE_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    SDL_SCANCODE_INTERNATIONAL2 = 136,
    SDL_SCANCODE_INTERNATIONAL3 = 137, /**< Yen */
    SDL_SCANCODE_INTERNATIONAL4 = 138,
    SDL_SCANCODE_INTERNATIONAL5 = 139,
    SDL_SCANCODE_INTERNATIONAL6 = 140,
    SDL_SCANCODE_INTERNATIONAL7 = 141,
    SDL_SCANCODE_INTERNATIONAL8 = 142,
    SDL_SCANCODE_INTERNATIONAL9 = 143,
    SDL_SCANCODE_LANG1 = 144, /**< Hangul/English toggle */
    SDL_SCANCODE_LANG2 = 145, /**< Hanja conversion */
    SDL_SCANCODE_LANG3 = 146, /**< Katakana */
    SDL_SCANCODE_LANG4 = 147, /**< Hiragana */
    SDL_SCANCODE_LANG5 = 148, /**< Zenkaku/Hankaku */
    SDL_SCANCODE_LANG6 = 149, /**< reserved */
    SDL_SCANCODE_LANG7 = 150, /**< reserved */
    SDL_SCANCODE_LANG8 = 151, /**< reserved */
    SDL_SCANCODE_LANG9 = 152, /**< reserved */

    SDL_SCANCODE_ALTERASE = 153, /**< Erase-Eaze */
    SDL_SCANCODE_SYSREQ = 154,
    SDL_SCANCODE_CANCEL = 155,
    SDL_SCANCODE_CLEAR = 156,
    SDL_SCANCODE_PRIOR = 157,
    SDL_SCANCODE_RETURN2 = 158,
    SDL_SCANCODE_SEPARATOR = 159,
    SDL_SCANCODE_OUT = 160,
    SDL_SCANCODE_OPER = 161,
    SDL_SCANCODE_CLEARAGAIN = 162,
    SDL_SCANCODE_CRSEL = 163,
    SDL_SCANCODE_EXSEL = 164,

    SDL_SCANCODE_KP_00 = 176,
    SDL_SCANCODE_KP_000 = 177,
    SDL_SCANCODE_THOUSANDSSEPARATOR = 178,
    SDL_SCANCODE_DECIMALSEPARATOR = 179,
    SDL_SCANCODE_CURRENCYUNIT = 180,
    SDL_SCANCODE_CURRENCYSUBUNIT = 181,
    SDL_SCANCODE_KP_LEFTPAREN = 182,
    SDL_SCANCODE_KP_RIGHTPAREN = 183,
    SDL_SCANCODE_KP_LEFTBRACE = 184,
    SDL_SCANCODE_KP_RIGHTBRACE = 185,
    SDL_SCANCODE_KP_TAB = 186,
    SDL_SCANCODE_KP_BACKSPACE = 187,
    SDL_SCANCODE_KP_A = 188,
    SDL_SCANCODE_KP_B = 189,
    SDL_SCANCODE_KP_C = 190,
    SDL_SCANCODE_KP_D = 191,
    SDL_SCANCODE_KP_E = 192,
    SDL_SCANCODE_KP_F = 193,
    SDL_SCANCODE_KP_XOR = 194,
    SDL_SCANCODE_KP_POWER = 195,
    SDL_SCANCODE_KP_PERCENT = 196,
    SDL_SCANCODE_KP_LESS = 197,
    SDL_SCANCODE_KP_GREATER = 198,
    SDL_SCANCODE_KP_AMPERSAND = 199,
    SDL_SCANCODE_KP_DBLAMPERSAND = 200,
    SDL_SCANCODE_KP_VERTICALBAR = 201,
    SDL_SCANCODE_KP_DBLVERTICALBAR = 202,
    SDL_SCANCODE_KP_COLON = 203,
    SDL_SCANCODE_KP_HASH = 204,
    SDL_SCANCODE_KP_SPACE = 205,
    SDL_SCANCODE_KP_AT = 206,
    SDL_SCANCODE_KP_EXCLAM = 207,
    SDL_SCANCODE_KP_MEMSTORE = 208,
    SDL_SCANCODE_KP_MEMRECALL = 209,
    SDL_SCANCODE_KP_MEMCLEAR = 210,
    SDL_SCANCODE_KP_MEMADD = 211,
    SDL_SCANCODE_KP_MEMSUBTRACT = 212,
    SDL_SCANCODE_KP_MEMMULTIPLY = 213,
    SDL_SCANCODE_KP_MEMDIVIDE = 214,
    SDL_SCANCODE_KP_PLUSMINUS = 215,
    SDL_SCANCODE_KP_CLEAR = 216,
    SDL_SCANCODE_KP_CLEARENTRY = 217,
    SDL_SCANCODE_KP_BINARY = 218,
    SDL_SCANCODE_KP_OCTAL = 219,
    SDL_SCANCODE_KP_DECIMAL = 220,
    SDL_SCANCODE_KP_HEXADECIMAL = 221,

    SDL_SCANCODE_LCTRL = 224,
    SDL_SCANCODE_LSHIFT = 225,
    SDL_SCANCODE_LALT = 226, /**< alt, option */
    SDL_SCANCODE_LGUI = 227, /**< windows, command (apple), meta */
    SDL_SCANCODE_RCTRL = 228,
    SDL_SCANCODE_RSHIFT = 229,
    SDL_SCANCODE_RALT = 230, /**< alt gr, option */
    SDL_SCANCODE_RGUI = 231, /**< windows, command (apple), meta */

    SDL_SCANCODE_MODE = 257,    /**< I'm not sure if this is really not covered
                                 *   by any of the above, but since there's a
                                 *   special KMOD_MODE for it I'm adding it here
                                 */

    /* @} *//* Usage page 0x07 */

    /**
     *  \name Usage page 0x0C
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     */
    /* @{ */

    SDL_SCANCODE_AUDIONEXT = 258,
    SDL_SCANCODE_AUDIOPREV = 259,
    SDL_SCANCODE_AUDIOSTOP = 260,
    SDL_SCANCODE_AUDIOPLAY = 261,
    SDL_SCANCODE_AUDIOMUTE = 262,
    SDL_SCANCODE_MEDIASELECT = 263,
    SDL_SCANCODE_WWW = 264,
    SDL_SCANCODE_MAIL = 265,
    SDL_SCANCODE_CALCULATOR = 266,
    SDL_SCANCODE_COMPUTER = 267,
    SDL_SCANCODE_AC_SEARCH = 268,
    SDL_SCANCODE_AC_HOME = 269,
    SDL_SCANCODE_AC_BACK = 270,
    SDL_SCANCODE_AC_FORWARD = 271,
    SDL_SCANCODE_AC_STOP = 272,
    SDL_SCANCODE_AC_REFRESH = 273,
    SDL_SCANCODE_AC_BOOKMARKS = 274,

    /* @} *//* Usage page 0x0C */

    /**
     *  \name Walther keys
     *
     *  These are values that Christian Walther added (for mac keyboard?).
     */
    /* @{ */

    SDL_SCANCODE_BRIGHTNESSDOWN = 275,
    SDL_SCANCODE_BRIGHTNESSUP = 276,
    SDL_SCANCODE_DISPLAYSWITCH = 277, /**< display mirroring/dual display
                                           switch, video mode switch */
    SDL_SCANCODE_KBDILLUMTOGGLE = 278,
    SDL_SCANCODE_KBDILLUMDOWN = 279,
    SDL_SCANCODE_KBDILLUMUP = 280,
    SDL_SCANCODE_EJECT = 281,
    SDL_SCANCODE_SLEEP = 282,

    SDL_SCANCODE_APP1 = 283,
    SDL_SCANCODE_APP2 = 284,

    /* @} *//* Walther keys */

    /* Add any other keys here. */

    SDL_NUM_SCANCODES = 512 /**< not a key, just marks the number of scancodes
                                 for array bounds */
};
