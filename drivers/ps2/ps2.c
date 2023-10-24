#include "ps2.h"
#include <pico/stdlib.h>
#include <stdbool.h>
#include "string.h"
#include "hardware/irq.h"

volatile bool is_e0=false;
volatile bool is_e1=false;
volatile bool is_f0=false;

volatile uint8_t head;
volatile uint8_t tail;
volatile int bitcount;
volatile uint16_t data;
volatile uint16_t raw_data;
volatile uint8_t parity;
uint8_t kbd_buffer[KBD_BUFFER_SIZE];
volatile uint32_t last_key;

uint32_t kbd_statuses[4] = {};


void keys_to_str(char* str_buf,char s_char){
    char s_str[2];
    s_str[0]=s_char;
    s_str[1]='\0';

    str_buf[0]=0;
    strcat(str_buf,"KEY PRESSED: ");
//0 набор
    if (kbd_statuses[0]&KB_U0_A) {strcat(str_buf,"A");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_B) {strcat(str_buf,"B");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_C) {strcat(str_buf,"C");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_D) {strcat(str_buf,"D");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_E) {strcat(str_buf,"E");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_F) {strcat(str_buf,"F");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_G) {strcat(str_buf,"G");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_H) {strcat(str_buf,"H");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_I) {strcat(str_buf,"I");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_J) {strcat(str_buf,"J");strcat(str_buf,s_str);};

    if (kbd_statuses[0]&KB_U0_K) {strcat(str_buf,"K");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_L) {strcat(str_buf,"L");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_M) {strcat(str_buf,"M");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_N) {strcat(str_buf,"N");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_O) {strcat(str_buf,"O");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_P) {strcat(str_buf,"P");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_Q) {strcat(str_buf,"Q");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_R) {strcat(str_buf,"R");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_S) {strcat(str_buf,"S");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_T) {strcat(str_buf,"T");strcat(str_buf,s_str);};

    if (kbd_statuses[0]&KB_U0_U) {strcat(str_buf,"U");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_V) {strcat(str_buf,"V");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_W) {strcat(str_buf,"W");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_X) {strcat(str_buf,"X");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_Y) {strcat(str_buf,"Y");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_Z) {strcat(str_buf,"Z");strcat(str_buf,s_str);};

    if (kbd_statuses[0]&KB_U0_SEMICOLON) {strcat(str_buf,";");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_QUOTE) {strcat(str_buf,"\"");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_COMMA) {strcat(str_buf,",");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_PERIOD) {strcat(str_buf,".");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_LEFT_BR) {strcat(str_buf,"[");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_RIGHT_BR) {strcat(str_buf,"]");strcat(str_buf,s_str);};
//1 набор
    if (kbd_statuses[1]&KB_U1_0) {strcat(str_buf,"0");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_1) {strcat(str_buf,"1");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_2) {strcat(str_buf,"2");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_3) {strcat(str_buf,"3");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_4) {strcat(str_buf,"4");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_5) {strcat(str_buf,"5");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_6) {strcat(str_buf,"6");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_7) {strcat(str_buf,"7");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_8) {strcat(str_buf,"8");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_9) {strcat(str_buf,"9");strcat(str_buf,s_str);};

    if (kbd_statuses[1]&KB_U1_ENTER) {strcat(str_buf,"ENTER");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_SLASH) {strcat(str_buf,"/");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_MINUS) {strcat(str_buf,"MINUS");strcat(str_buf,s_str);};

    if (kbd_statuses[1]&KB_U1_EQUALS) {strcat(str_buf,"=");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_BACKSLASH) {strcat(str_buf,"BACKSLASH");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_CAPS_LOCK) {strcat(str_buf,"CAPS_LOCK");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_TAB) {strcat(str_buf,"TAB");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_BACK_SPACE) {strcat(str_buf,"BACK_SPACE");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_ESC) {strcat(str_buf,"ESC");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_TILDE) {strcat(str_buf,"TILDE");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_MENU) {strcat(str_buf,"MENU");strcat(str_buf,s_str);};

    if (kbd_statuses[1]&KB_U1_L_SHIFT) {strcat(str_buf,"L_SHIFT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_L_CTRL) {strcat(str_buf,"L_CTRL");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_L_ALT) {strcat(str_buf,"L_ALT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_L_WIN) {strcat(str_buf,"L_WIN");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_SHIFT) {strcat(str_buf,"R_SHIFT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_CTRL) {strcat(str_buf,"R_CTRL");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_ALT) {strcat(str_buf,"R_ALT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_WIN) {strcat(str_buf,"R_WIN");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_SPACE) {strcat(str_buf,"SPACE");strcat(str_buf,s_str);};


//2 набор
    if (kbd_statuses[2]&KB_U2_NUM_0) {strcat(str_buf,"NUM_0");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_1) {strcat(str_buf,"NUM_1");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_2) {strcat(str_buf,"NUM_2");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_3) {strcat(str_buf,"NUM_3");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_4) {strcat(str_buf,"NUM_4");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_5) {strcat(str_buf,"NUM_5");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_6) {strcat(str_buf,"NUM_6");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_7) {strcat(str_buf,"NUM_7");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_8) {strcat(str_buf,"NUM_8");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_9) {strcat(str_buf,"NUM_9");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_NUM_ENTER) {strcat(str_buf,"NUM_ENTER");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_SLASH) {strcat(str_buf,"NUM_/");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_MINUS) {strcat(str_buf,"NUM_MINUS");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_NUM_PLUS) {strcat(str_buf,"NUM_PLUS");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_MULT) {strcat(str_buf,"NUM_MULT");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_PERIOD) {strcat(str_buf,"NUM_PERIOD");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_LOCK) {strcat(str_buf,"NUM_LOCK");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_DELETE) {strcat(str_buf,"DEL");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_SCROLL_LOCK) {strcat(str_buf,"SCROLL_LOCK");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_PAUSE_BREAK) {strcat(str_buf,"PAUSE_BREAK");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_INSERT) {strcat(str_buf,"INSERT");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_HOME) {strcat(str_buf,"HOME");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_PAGE_UP) {strcat(str_buf,"PG_UP");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_PAGE_DOWN) {strcat(str_buf,"PG_DOWN");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_PRT_SCR) {strcat(str_buf,"PRT_SCR");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_END) {strcat(str_buf,"END");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_UP) {strcat(str_buf,"UP");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_DOWN) {strcat(str_buf,"DOWN");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_LEFT) {strcat(str_buf,"LEFT");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_RIGHT) {strcat(str_buf,"RIGHT");strcat(str_buf,s_str);};

//3 набор
    if (kbd_statuses[3]&KB_U3_F1) {strcat(str_buf,"F1");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F2) {strcat(str_buf,"F2");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F3) {strcat(str_buf,"F3");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F4) {strcat(str_buf,"F4");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F5) {strcat(str_buf,"F5");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F6) {strcat(str_buf,"F6");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F7) {strcat(str_buf,"F7");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F8) {strcat(str_buf,"F8");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F9) {strcat(str_buf,"F9");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F10) {strcat(str_buf,"F10");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F11) {strcat(str_buf,"F11");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F12) {strcat(str_buf,"F12");strcat(str_buf,s_str);};

    strcat(str_buf,"\n");

};


void KeyboardHandler(){ //uint /*gpio*/, uint32_t /*event_mask*/
	uint8_t repeat=3;
	if ((bitcount > 10)&&((time_us_32()-last_key)>1000)){
		bitcount=0;
		data = 0x00;
		raw_data = 0x00;
		parity = 0;
		last_key=time_us_32();
		//DEH_printf("RT[%02d][%02X]>%d>%d>%d\n",head,kbd_buffer[head],bitcount,time_us_32(),last_key);
	};

	raw_data >>= 1; //читаем 11 бит в INT16 сдвигая слева на право побитно


//    int p = gpio_get(KBD_DATA_PIN);
//    gpio_put(25,p);
//    DEH_printf("%x\r\n", p);

	if(gpio_get(KBD_DATA_PIN)>0){
		raw_data|= 0x8000;
	}
	if ((bitcount > 0) && (bitcount < 9)){
		parity+=gpio_get(KBD_DATA_PIN);
	}


	if ((bitcount++ == 10)) { //11 бит?
        uint8_t i = head + 1;
       // DEH_printf("%x %i %i\r\n ", raw_data, i, tail);
		if (i >= KBD_BUFFER_SIZE) i = 0;
		if (i != tail) {
			uint32_t temp = raw_data;
			while(repeat>0){
				temp>>=5; //сдвигаем вправо до нулевого принятого бита
				if((temp&0x01)^1){ //стартовый бит=0?
					temp>>=1; //пропускаем стартовый бит
					data=(temp&0xFF); //выколупываем скан код
					temp>>=8; //пропускаем данные до бита чётности и стопового бита
					parity&=1;
					//printf("parity:%d>%d\n",parity,((temp&0x01)^1)); //&&()
					if(((temp&0x02)>0)&&(((temp&0x01)^1)==parity)){ //стоповый бит == 1? И четность совпала?
						kbd_buffer[i] = data; //сохраняем данные
						head = i;
                        //DEH_printf("DATA: %x\r\n", data);
						return;
					} else{
						temp = raw_data<<1; //стоповый бит не равен единице - сдвигаем данные на бит вправо и пытаемся перечитать.
						repeat--; // минус 1  попытка распознавания
					}
				} else{
					temp = raw_data<<1; //стартовый бит не равен нулю - сдвигаем данные на бит вправо и пытаемся перечитать.
					repeat--; // минус 1  попытка распознавания
				}
			}
			//DEH_printf("BU[%02d][%02X]>%d>%d>%d\n",i,kbd_buffer[i],bitcount,time_us_32(),last_key);//4635 us //us_to_ms(time_us_32())
			//data = 0x00;
			//d_sleep_us(25);
			return;
		}
		
	}
}

uint8_t __not_in_flash_func(get_scan_code)(void){ //__not_in_flash_func()
	if((time_us_32()-last_key)<1000) return 0;
	if (tail == head) return 0;
	//d_sleep_us(25);
	uint8_t c, i;
	i = tail;
	i++;
	if (i >= KBD_BUFFER_SIZE) i = 0;
	c = kbd_buffer[i];
	tail = i;
	//printf("SC[%02d][%02d][%02X]\n",head,tail,kbd_buffer[i]);
	return c;
}

bool decode_kbd(){
	//char test_str[128];
	uint8_t scancode=get_scan_code();
	if (scancode==0xe0) {is_e0=true;return false;} //ps2Flags.
	if (scancode==0xe1) {is_e1=true;return false;}
	if (scancode==0xf0) {is_f0=true;return false;}  
	if (scancode){
		//сканкод 
		//получение универсальных кодов из сканкодов PS/2
		translate_scancode(scancode,!is_f0,is_e0,is_e1);
		//keys_to_str(test_str,' ');
		//DEH_printf("is_e0=%d, is_f0=%d, code=0x%02x test_str=%s\n",is_e0,is_f0,scancode,test_str);

		is_e0=false;
		if (is_f0) is_e1=false;
		is_f0=false;
		//преобразование из универсальных сканкодов в матрицу бытрого преобразования кодов для zx клавиатуры
		//zx_kb_decode(zx_keyboard_state);
		
		return true;//произошли изменения
		
	}
	/*if((time_us_32()-last_key)>500){
		bitcount = 0;
	}*/
	return false;
}

void translate_scancode(uint8_t code,bool is_press, bool is_e0,bool is_e1){
    if (is_e1){
        if (code==0x14) {if (is_press) kbd_statuses[2]|=KB_U2_PAUSE_BREAK; else kbd_statuses[2]&=~KB_U2_PAUSE_BREAK;}
        return;
    }

    if (!is_e0)
        switch (code){
            //0
            case 0x1C: if (is_press) kbd_statuses[0]|=KB_U0_A; else kbd_statuses[0]&=~KB_U0_A; break;
            case 0x32: if (is_press) kbd_statuses[0]|=KB_U0_B; else kbd_statuses[0]&=~KB_U0_B; break;
            case 0x21: if (is_press) kbd_statuses[0]|=KB_U0_C; else kbd_statuses[0]&=~KB_U0_C; break;
            case 0x23: if (is_press) kbd_statuses[0]|=KB_U0_D; else kbd_statuses[0]&=~KB_U0_D; break;
            case 0x24: if (is_press) kbd_statuses[0]|=KB_U0_E; else kbd_statuses[0]&=~KB_U0_E; break;
            case 0x2B: if (is_press) kbd_statuses[0]|=KB_U0_F; else kbd_statuses[0]&=~KB_U0_F; break;
            case 0x34: if (is_press) kbd_statuses[0]|=KB_U0_G; else kbd_statuses[0]&=~KB_U0_G; break;
            case 0x33: if (is_press) kbd_statuses[0]|=KB_U0_H; else kbd_statuses[0]&=~KB_U0_H; break;
            case 0x43: if (is_press) kbd_statuses[0]|=KB_U0_I; else kbd_statuses[0]&=~KB_U0_I; break;
            case 0x3B: if (is_press) kbd_statuses[0]|=KB_U0_J; else kbd_statuses[0]&=~KB_U0_J; break;

            case 0x42: if (is_press) kbd_statuses[0]|=KB_U0_K; else kbd_statuses[0]&=~KB_U0_K; break;
            case 0x4B: if (is_press) kbd_statuses[0]|=KB_U0_L; else kbd_statuses[0]&=~KB_U0_L; break;
            case 0x3A: if (is_press) kbd_statuses[0]|=KB_U0_M; else kbd_statuses[0]&=~KB_U0_M; break;
            case 0x31: if (is_press) kbd_statuses[0]|=KB_U0_N; else kbd_statuses[0]&=~KB_U0_N; break;
            case 0x44: if (is_press) kbd_statuses[0]|=KB_U0_O; else kbd_statuses[0]&=~KB_U0_O; break;
            case 0x4D: if (is_press) kbd_statuses[0]|=KB_U0_P; else kbd_statuses[0]&=~KB_U0_P; break;
            case 0x15: if (is_press) kbd_statuses[0]|=KB_U0_Q; else kbd_statuses[0]&=~KB_U0_Q; break;
            case 0x2D: if (is_press) kbd_statuses[0]|=KB_U0_R; else kbd_statuses[0]&=~KB_U0_R; break;
            case 0x1B: if (is_press) kbd_statuses[0]|=KB_U0_S; else kbd_statuses[0]&=~KB_U0_S; break;
            case 0x2C: if (is_press) kbd_statuses[0]|=KB_U0_T; else kbd_statuses[0]&=~KB_U0_T; break;

            case 0x3C: if (is_press) kbd_statuses[0]|=KB_U0_U; else kbd_statuses[0]&=~KB_U0_U; break;
            case 0x2A: if (is_press) kbd_statuses[0]|=KB_U0_V; else kbd_statuses[0]&=~KB_U0_V; break;
            case 0x1D: if (is_press) kbd_statuses[0]|=KB_U0_W; else kbd_statuses[0]&=~KB_U0_W; break;
            case 0x22: if (is_press) kbd_statuses[0]|=KB_U0_X; else kbd_statuses[0]&=~KB_U0_X; break;
            case 0x35: if (is_press) kbd_statuses[0]|=KB_U0_Y; else kbd_statuses[0]&=~KB_U0_Y; break;
            case 0x1A: if (is_press) kbd_statuses[0]|=KB_U0_Z; else kbd_statuses[0]&=~KB_U0_Z; break;

            case 0x54: if (is_press) kbd_statuses[0]|=KB_U0_LEFT_BR; else kbd_statuses[0]&=~KB_U0_LEFT_BR; break;
            case 0x5B: if (is_press) kbd_statuses[0]|=KB_U0_RIGHT_BR; else kbd_statuses[0]&=~KB_U0_RIGHT_BR; break;
            case 0x4C: if (is_press) kbd_statuses[0]|=KB_U0_SEMICOLON; else kbd_statuses[0]&=~KB_U0_SEMICOLON; break;
            case 0x52: if (is_press) kbd_statuses[0]|=KB_U0_QUOTE; else kbd_statuses[0]&=~KB_U0_QUOTE; break;
            case 0x41: if (is_press) kbd_statuses[0]|=KB_U0_COMMA; else kbd_statuses[0]&=~KB_U0_COMMA; break;
            case 0x49: if (is_press) kbd_statuses[0]|=KB_U0_PERIOD; else kbd_statuses[0]&=~KB_U0_PERIOD; break;

                //1 -----------
            case 0x45: if (is_press) kbd_statuses[1]|=KB_U1_0; else kbd_statuses[1]&=~KB_U1_0; break;
            case 0x16: if (is_press) kbd_statuses[1]|=KB_U1_1; else kbd_statuses[1]&=~KB_U1_1; break;
            case 0x1E: if (is_press) kbd_statuses[1]|=KB_U1_2; else kbd_statuses[1]&=~KB_U1_2; break;
            case 0x26: if (is_press) kbd_statuses[1]|=KB_U1_3; else kbd_statuses[1]&=~KB_U1_3; break;
            case 0x25: if (is_press) kbd_statuses[1]|=KB_U1_4; else kbd_statuses[1]&=~KB_U1_4; break;
            case 0x2E: if (is_press) kbd_statuses[1]|=KB_U1_5; else kbd_statuses[1]&=~KB_U1_5; break;
            case 0x36: if (is_press) kbd_statuses[1]|=KB_U1_6; else kbd_statuses[1]&=~KB_U1_6; break;
            case 0x3D: if (is_press) kbd_statuses[1]|=KB_U1_7; else kbd_statuses[1]&=~KB_U1_7; break;
            case 0x3E: if (is_press) kbd_statuses[1]|=KB_U1_8; else kbd_statuses[1]&=~KB_U1_8; break;
            case 0x46: if (is_press) kbd_statuses[1]|=KB_U1_9; else kbd_statuses[1]&=~KB_U1_9; break;

            case 0x4E: if (is_press) kbd_statuses[1]|=KB_U1_MINUS; else kbd_statuses[1]&=~KB_U1_MINUS; break;
            case 0x55: if (is_press) kbd_statuses[1]|=KB_U1_EQUALS; else kbd_statuses[1]&=~KB_U1_EQUALS; break;
            case 0x5D: if (is_press) kbd_statuses[1]|=KB_U1_BACKSLASH; else kbd_statuses[1]&=~KB_U1_BACKSLASH; break;
            case 0x66: if (is_press) kbd_statuses[1]|=KB_U1_BACK_SPACE; else kbd_statuses[1]&=~KB_U1_BACK_SPACE; break;
            case 0x5A: if (is_press) kbd_statuses[1]|=KB_U1_ENTER; else kbd_statuses[1]&=~KB_U1_ENTER; break;
            case 0x4A: if (is_press) kbd_statuses[1]|=KB_U1_SLASH; else kbd_statuses[1]&=~KB_U1_SLASH; break;
            case 0x0E: if (is_press) kbd_statuses[1]|=KB_U1_TILDE; else kbd_statuses[1]&=~KB_U1_TILDE; break;
            case 0x0D: if (is_press) kbd_statuses[1]|=KB_U1_TAB; else kbd_statuses[1]&=~KB_U1_TAB; break;
            case 0x58: if (is_press) kbd_statuses[1]|=KB_U1_CAPS_LOCK; else kbd_statuses[1]&=~KB_U1_CAPS_LOCK; break;
            case 0x76: if (is_press) kbd_statuses[1]|=KB_U1_ESC; else kbd_statuses[1]&=~KB_U1_ESC; break;

            case 0x12: if (is_press) kbd_statuses[1]|=KB_U1_L_SHIFT; else kbd_statuses[1]&=~KB_U1_L_SHIFT; break;
            case 0x14: if (is_press) kbd_statuses[1]|=KB_U1_L_CTRL; else kbd_statuses[1]&=~KB_U1_L_CTRL; break;
            case 0x11: if (is_press) kbd_statuses[1]|=KB_U1_L_ALT; else kbd_statuses[1]&=~KB_U1_L_ALT; break;
            case 0x59: if (is_press) kbd_statuses[1]|=KB_U1_R_SHIFT; else kbd_statuses[1]&=~KB_U1_R_SHIFT; break;

            case 0x29: if (is_press) kbd_statuses[1]|=KB_U1_SPACE; else kbd_statuses[1]&=~KB_U1_SPACE; break;
                //2 -----------
            case 0x70: if (is_press) kbd_statuses[2]|=KB_U2_NUM_0; else kbd_statuses[2]&=~KB_U2_NUM_0; break;
            case 0x69: if (is_press) kbd_statuses[2]|=KB_U2_NUM_1; else kbd_statuses[2]&=~KB_U2_NUM_1; break;
            case 0x72: if (is_press) kbd_statuses[2]|=KB_U2_NUM_2; else kbd_statuses[2]&=~KB_U2_NUM_2; break;
            case 0x7A: if (is_press) kbd_statuses[2]|=KB_U2_NUM_3; else kbd_statuses[2]&=~KB_U2_NUM_3; break;
            case 0x6B: if (is_press) kbd_statuses[2]|=KB_U2_NUM_4; else kbd_statuses[2]&=~KB_U2_NUM_4; break;
            case 0x73: if (is_press) kbd_statuses[2]|=KB_U2_NUM_5; else kbd_statuses[2]&=~KB_U2_NUM_5; break;
            case 0x74: if (is_press) kbd_statuses[2]|=KB_U2_NUM_6; else kbd_statuses[2]&=~KB_U2_NUM_6; break;
            case 0x6C: if (is_press) kbd_statuses[2]|=KB_U2_NUM_7; else kbd_statuses[2]&=~KB_U2_NUM_7; break;
            case 0x75: if (is_press) kbd_statuses[2]|=KB_U2_NUM_8; else kbd_statuses[2]&=~KB_U2_NUM_8; break;
            case 0x7D: if (is_press) kbd_statuses[2]|=KB_U2_NUM_9; else kbd_statuses[2]&=~KB_U2_NUM_9; break;

            case 0x77: if (is_press) kbd_statuses[2]|=KB_U2_NUM_LOCK; else kbd_statuses[2]&=~KB_U2_NUM_LOCK; break;
            case 0x7C: if (is_press) kbd_statuses[2]|=KB_U2_NUM_MULT; else kbd_statuses[2]&=~KB_U2_NUM_MULT; break;
            case 0x7B: if (is_press) kbd_statuses[2]|=KB_U2_NUM_MINUS; else kbd_statuses[2]&=~KB_U2_NUM_MINUS; break;
            case 0x79: if (is_press) kbd_statuses[2]|=KB_U2_NUM_PLUS; else kbd_statuses[2]&=~KB_U2_NUM_PLUS; break;
            case 0x71: if (is_press) kbd_statuses[2]|=KB_U2_NUM_PERIOD; else kbd_statuses[2]&=~KB_U2_NUM_PERIOD; break;
            case 0x7E: if (is_press) kbd_statuses[2]|=KB_U2_SCROLL_LOCK; else kbd_statuses[2]&=~KB_U2_SCROLL_LOCK; break;
                //3 -----------
            case 0x05: if (is_press) kbd_statuses[3]|=KB_U3_F1; else kbd_statuses[3]&=~KB_U3_F1; break;
            case 0x06: if (is_press) kbd_statuses[3]|=KB_U3_F2; else kbd_statuses[3]&=~KB_U3_F2; break;
            case 0x04: if (is_press) kbd_statuses[3]|=KB_U3_F3; else kbd_statuses[3]&=~KB_U3_F3; break;
            case 0x0C: if (is_press) kbd_statuses[3]|=KB_U3_F4; else kbd_statuses[3]&=~KB_U3_F4; break;
            case 0x03: if (is_press) kbd_statuses[3]|=KB_U3_F5; else kbd_statuses[3]&=~KB_U3_F5; break;
            case 0x0B: if (is_press) kbd_statuses[3]|=KB_U3_F6; else kbd_statuses[3]&=~KB_U3_F6; break;
            case 0x83: if (is_press) kbd_statuses[3]|=KB_U3_F7; else kbd_statuses[3]&=~KB_U3_F7; break;
            case 0x0A: if (is_press) kbd_statuses[3]|=KB_U3_F8; else kbd_statuses[3]&=~KB_U3_F8; break;
            case 0x01: if (is_press) kbd_statuses[3]|=KB_U3_F9; else kbd_statuses[3]&=~KB_U3_F9; break;
            case 0x09: if (is_press) kbd_statuses[3]|=KB_U3_F10; else kbd_statuses[3]&=~KB_U3_F10; break;

            case 0x78: if (is_press) kbd_statuses[3]|=KB_U3_F11; else kbd_statuses[3]&=~KB_U3_F11; break;
            case 0x07: if (is_press) kbd_statuses[3]|=KB_U3_F12; else kbd_statuses[3]&=~KB_U3_F12; break;



            default:
                break;
        }
    if (is_e0)
        switch (code){
            //1----------------
            case 0x1F: if (is_press) kbd_statuses[1]|=KB_U1_L_WIN; else kbd_statuses[1]&=~KB_U1_L_WIN; break;
            case 0x14: if (is_press) kbd_statuses[1]|=KB_U1_R_CTRL; else kbd_statuses[1]&=~KB_U1_R_CTRL; break;
            case 0x11: if (is_press) kbd_statuses[1]|=KB_U1_R_ALT; else kbd_statuses[1]&=~KB_U1_R_ALT; break;
            case 0x27: if (is_press) kbd_statuses[1]|=KB_U1_R_WIN; else kbd_statuses[1]&=~KB_U1_R_WIN; break;
            case 0x2F: if (is_press) kbd_statuses[1]|=KB_U1_MENU; else kbd_statuses[1]&=~KB_U1_MENU; break;
                //2------------------
                //для принт скрин обработаем только 1 код

            case 0x7C: if (is_press) kbd_statuses[2]|=KB_U2_PRT_SCR; break;
            case 0x12: if (!is_press) kbd_statuses[2]&=~KB_U2_PRT_SCR; break;

            case 0x4A: if (is_press) kbd_statuses[2]|=KB_U2_NUM_SLASH; else kbd_statuses[2]&=~KB_U2_NUM_SLASH; break;
            case 0x5A: if (is_press) kbd_statuses[2]|=KB_U2_NUM_ENTER; else kbd_statuses[2]&=~KB_U2_NUM_ENTER; break;
            case 0x75: if (is_press) kbd_statuses[2]|=KB_U2_UP; else kbd_statuses[2]&=~KB_U2_UP; break;
            case 0x72: if (is_press) kbd_statuses[2]|=KB_U2_DOWN; else kbd_statuses[2]&=~KB_U2_DOWN; break;
            case 0x74: if (is_press) kbd_statuses[2]|=KB_U2_RIGHT; else kbd_statuses[2]&=~KB_U2_RIGHT; break;
            case 0x6B: if (is_press) kbd_statuses[2]|=KB_U2_LEFT; else kbd_statuses[2]&=~KB_U2_LEFT; break;
            case 0x71: if (is_press) kbd_statuses[2]|=KB_U2_DELETE; else kbd_statuses[2]&=~KB_U2_DELETE; break;
            case 0x69: if (is_press) kbd_statuses[2]|=KB_U2_END; else kbd_statuses[2]&=~KB_U2_END; break;
            case 0x7A: if (is_press) kbd_statuses[2]|=KB_U2_PAGE_DOWN; else kbd_statuses[2]&=~KB_U2_PAGE_DOWN; break;
            case 0x7D: if (is_press) kbd_statuses[2]|=KB_U2_PAGE_UP; else kbd_statuses[2]&=~KB_U2_PAGE_UP; break;

            case 0x6C: if (is_press) kbd_statuses[2]|=KB_U2_HOME; else kbd_statuses[2]&=~KB_U2_HOME; break;
            case 0x70: if (is_press) kbd_statuses[2]|=KB_U2_INSERT; else kbd_statuses[2]&=~KB_U2_INSERT; break;
        }
}

void Init_kbd(void){
	bitcount=0;
	data=0;
	head=0;
	tail=0;
	memset(kbd_buffer,0,KBD_BUFFER_SIZE);
	for (uint8_t i = 0; i < 4; i++){
		kbd_statuses[i]=0;
	}
	is_e0=false;
	is_e1=false;
	is_f0=false;

	//buffer = (uint8_t *)&this->kbd_buffer;
	//printf("[%08X][%08X]\n",*&buffer,&this->kbd_buffer);
	gpio_init(KBD_CLOCK_PIN);
	gpio_disable_pulls(KBD_CLOCK_PIN);
	gpio_set_dir(KBD_CLOCK_PIN,GPIO_IN);
	gpio_init(KBD_DATA_PIN);
	gpio_disable_pulls(KBD_DATA_PIN);
	gpio_set_dir(KBD_DATA_PIN,GPIO_IN);

	//gpio_init(KBD_MIRROR_PIN);
	//gpio_set_dir(KBD_MIRROR_PIN,GPIO_OUT);

    gpio_init(25);
    gpio_set_dir(25,GPIO_OUT);

	gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true, (gpio_irq_callback_t)&KeyboardHandler); //

	//gpio_set_irq_enabled(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true);
	//gpio_set_irq_enabled(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_RISE, true);
	//gpio_set_irq_callback((gpio_irq_callback_t)&KeyboardHandler);
	//irq_set_enabled(IO_IRQ_BANK0, true);



};
void Deinit_kbd(void){
	gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, false, NULL);	//(gpio_irq_callback_t)
    gpio_deinit (KBD_CLOCK_PIN);
    gpio_deinit (KBD_DATA_PIN);	
};



void kbd_to_str(char *str_buf){
    char s_str[2];
    s_str[0]=' ';
    s_str[1]='\0';

    str_buf[0]=0;
    strcat(str_buf,"KEY PRESSED: ");
//0 набор
    if (kbd_statuses[0]&KB_U0_A) {strcat(str_buf,"A");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_B) {strcat(str_buf,"B");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_C) {strcat(str_buf,"C");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_D) {strcat(str_buf,"D");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_E) {strcat(str_buf,"E");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_F) {strcat(str_buf,"F");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_G) {strcat(str_buf,"G");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_H) {strcat(str_buf,"H");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_I) {strcat(str_buf,"I");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_J) {strcat(str_buf,"J");strcat(str_buf,s_str);};

    if (kbd_statuses[0]&KB_U0_K) {strcat(str_buf,"K");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_L) {strcat(str_buf,"L");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_M) {strcat(str_buf,"M");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_N) {strcat(str_buf,"N");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_O) {strcat(str_buf,"O");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_P) {strcat(str_buf,"P");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_Q) {strcat(str_buf,"Q");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_R) {strcat(str_buf,"R");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_S) {strcat(str_buf,"S");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_T) {strcat(str_buf,"T");strcat(str_buf,s_str);};

    if (kbd_statuses[0]&KB_U0_U) {strcat(str_buf,"U");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_V) {strcat(str_buf,"V");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_W) {strcat(str_buf,"W");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_X) {strcat(str_buf,"X");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_Y) {strcat(str_buf,"Y");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_Z) {strcat(str_buf,"Z");strcat(str_buf,s_str);};

    if (kbd_statuses[0]&KB_U0_SEMICOLON) {strcat(str_buf,";");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_QUOTE) {strcat(str_buf,"\"");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_COMMA) {strcat(str_buf,",");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_PERIOD) {strcat(str_buf,".");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_LEFT_BR) {strcat(str_buf,"[");strcat(str_buf,s_str);};
    if (kbd_statuses[0]&KB_U0_RIGHT_BR) {strcat(str_buf,"]");strcat(str_buf,s_str);};
//1 набор
    if (kbd_statuses[1]&KB_U1_0) {strcat(str_buf,"0");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_1) {strcat(str_buf,"1");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_2) {strcat(str_buf,"2");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_3) {strcat(str_buf,"3");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_4) {strcat(str_buf,"4");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_5) {strcat(str_buf,"5");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_6) {strcat(str_buf,"6");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_7) {strcat(str_buf,"7");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_8) {strcat(str_buf,"8");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_9) {strcat(str_buf,"9");strcat(str_buf,s_str);};

    if (kbd_statuses[1]&KB_U1_ENTER) {strcat(str_buf,"ENTER");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_SLASH) {strcat(str_buf,"/");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_MINUS) {strcat(str_buf,"MINUS");strcat(str_buf,s_str);};

    if (kbd_statuses[1]&KB_U1_EQUALS) {strcat(str_buf,"=");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_BACKSLASH) {strcat(str_buf,"BACKSLASH");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_CAPS_LOCK) {strcat(str_buf,"CAPS_LOCK");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_TAB) {strcat(str_buf,"TAB");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_BACK_SPACE) {strcat(str_buf,"BACK_SPACE");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_ESC) {strcat(str_buf,"ESC");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_TILDE) {strcat(str_buf,"TILDE");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_MENU) {strcat(str_buf,"MENU");strcat(str_buf,s_str);};

    if (kbd_statuses[1]&KB_U1_L_SHIFT) {strcat(str_buf,"L_SHIFT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_L_CTRL) {strcat(str_buf,"L_CTRL");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_L_ALT) {strcat(str_buf,"L_ALT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_L_WIN) {strcat(str_buf,"L_WIN");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_SHIFT) {strcat(str_buf,"R_SHIFT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_CTRL) {strcat(str_buf,"R_CTRL");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_ALT) {strcat(str_buf,"R_ALT");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_R_WIN) {strcat(str_buf,"R_WIN");strcat(str_buf,s_str);};
    if (kbd_statuses[1]&KB_U1_SPACE) {strcat(str_buf,"SPACE");strcat(str_buf,s_str);};


//2 набор
    if (kbd_statuses[2]&KB_U2_NUM_0) {strcat(str_buf,"NUM_0");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_1) {strcat(str_buf,"NUM_1");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_2) {strcat(str_buf,"NUM_2");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_3) {strcat(str_buf,"NUM_3");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_4) {strcat(str_buf,"NUM_4");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_5) {strcat(str_buf,"NUM_5");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_6) {strcat(str_buf,"NUM_6");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_7) {strcat(str_buf,"NUM_7");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_8) {strcat(str_buf,"NUM_8");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_9) {strcat(str_buf,"NUM_9");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_NUM_ENTER) {strcat(str_buf,"NUM_ENTER");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_SLASH) {strcat(str_buf,"NUM_/");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_MINUS) {strcat(str_buf,"NUM_MINUS");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_NUM_PLUS) {strcat(str_buf,"NUM_PLUS");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_MULT) {strcat(str_buf,"NUM_MULT");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_PERIOD) {strcat(str_buf,"NUM_PERIOD");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_NUM_LOCK) {strcat(str_buf,"NUM_LOCK");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_DELETE) {strcat(str_buf,"DEL");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_SCROLL_LOCK) {strcat(str_buf,"SCROLL_LOCK");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_PAUSE_BREAK) {strcat(str_buf,"PAUSE_BREAK");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_INSERT) {strcat(str_buf,"INSERT");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_HOME) {strcat(str_buf,"HOME");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_PAGE_UP) {strcat(str_buf,"PG_UP");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_PAGE_DOWN) {strcat(str_buf,"PG_DOWN");strcat(str_buf,s_str);};

    if (kbd_statuses[2]&KB_U2_PRT_SCR) {strcat(str_buf,"PRT_SCR");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_END) {strcat(str_buf,"END");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_UP) {strcat(str_buf,"UP");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_DOWN) {strcat(str_buf,"DOWN");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_LEFT) {strcat(str_buf,"LEFT");strcat(str_buf,s_str);};
    if (kbd_statuses[2]&KB_U2_RIGHT) {strcat(str_buf,"RIGHT");strcat(str_buf,s_str);};

//3 набор
    if (kbd_statuses[3]&KB_U3_F1) {strcat(str_buf,"F1");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F2) {strcat(str_buf,"F2");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F3) {strcat(str_buf,"F3");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F4) {strcat(str_buf,"F4");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F5) {strcat(str_buf,"F5");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F6) {strcat(str_buf,"F6");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F7) {strcat(str_buf,"F7");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F8) {strcat(str_buf,"F8");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F9) {strcat(str_buf,"F9");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F10) {strcat(str_buf,"F10");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F11) {strcat(str_buf,"F11");strcat(str_buf,s_str);};
    if (kbd_statuses[3]&KB_U3_F12) {strcat(str_buf,"F12");strcat(str_buf,s_str);};

    strcat(str_buf,"\n");

};