#include "emulator.h"

#define PIT_MODE_LATCHCOUNT  0
#define PIT_MODE_LOBYTE 1
#define PIT_MODE_HIBYTE 2
#define PIT_MODE_TOGGLE 3

struct i8253_s i8253;

void out8253(uint16_t portnum, uint8_t value) {
    uint8_t curbyte = 0;
    portnum &= 3;
    switch (portnum) {
        case 0:
        case 1:
        case 2: //channel data
            if ((i8253.accessmode[portnum] == PIT_MODE_LOBYTE) ||
                ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0))) {
                curbyte = 0;
            } else if ((i8253.accessmode[portnum] == PIT_MODE_HIBYTE) ||
                     ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1))) {
                curbyte = 1;
            }

            if (curbyte == 0) { //low byte
                i8253.chandata[portnum] = (i8253.chandata[portnum] & 0xFF00) | value;
            } else {   //high byte
                i8253.chandata[portnum] = (i8253.chandata[portnum] & 0x00FF) | ((uint16_t) value << 8);
            }

            if (i8253.chandata[portnum] == 0) {
                i8253.effectivedata[portnum] = 65536;
            } else {
                i8253.effectivedata[portnum] = i8253.chandata[portnum];
#if PICO_ON_DEVICE
                pwm_config_set_wrap(&config, i8253.effectivedata[portnum]);
                pwm_config_set_clkdiv(&config, 127);
                uint slice_num = pwm_gpio_to_slice_num(26);
                pwm_init(slice_num, &config, true);

                if (portram[0x61]&2){
                    pwm_set_gpio_level(26, 127);                    // set 50% (127) duty cycle ==> Sound output on
                } else {
                    pwm_set_gpio_level(26, 0);                      // set 0% (0) duty clcle ==> Sound output off
                }
#endif
            }

            i8253.active[portnum] = 1;
            if (i8253.accessmode[portnum] == PIT_MODE_TOGGLE) {
                i8253.bytetoggle[portnum] = (~i8253.bytetoggle[portnum]) & 1;
            }

            i8253.chanfreq[portnum] = (float) ((uint32_t) (((float) 1193182.0 / (float) i8253.effectivedata[portnum]) *
                                                           (float) 1000.0));
#if 1
            if (portnum == 0) {
              uint8_t period = 1;
              // Timer freq 1,193,180
              period += ((uint32_t) ((float)1000000.0 / ( ( (float) 1193182.0 / (float) i8253.effectivedata[portnum]))) / 1000);
     //         myTimer.begin(timer_isr, period);
                printf("TIMER %i SET?!\r\n ", period);
                timer_period = period;
            }
#endif
            break;
        case 3: //mode/command
            i8253.accessmode[value >> 6] = (value >> 4) & 3;
            if (i8253.accessmode[value >> 6] == PIT_MODE_TOGGLE) i8253.bytetoggle[value >> 6] = 0;
            break;
    }
}

uint8_t in8253(uint16_t portnum) {
    uint8_t curbyte;
    portnum &= 3;
    switch (portnum) {
        case 0:
        case 1:
        case 2: //channel data
            if ((i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_LOBYTE) ||
                ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0)))
                curbyte = 0;
            else if ((i8253.accessmode[portnum] == PIT_MODE_HIBYTE) ||
                     ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1)))
                curbyte = 1;
            if ((i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_LOBYTE) ||
                ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 0)))
                curbyte = 0;
            else if ((i8253.accessmode[portnum] == PIT_MODE_HIBYTE) ||
                     ((i8253.accessmode[portnum] == PIT_MODE_TOGGLE) && (i8253.bytetoggle[portnum] == 1)))
                curbyte = 1;
            if ((i8253.accessmode[portnum] == 0) || (i8253.accessmode[portnum] == PIT_MODE_TOGGLE))
                i8253.bytetoggle[portnum] = (~i8253.bytetoggle[portnum]) & 1;
            if (curbyte == 0) { //low byte
                if (i8253.counter[portnum] < 10) i8253.counter[portnum] = i8253.chandata[portnum];
                i8253.counter[portnum] -= 10;
                return ((uint8_t) i8253.counter[portnum]);
            } else {   //high byte
                return ((uint8_t) (i8253.counter[portnum] >> 8));
            }
            break;
    }
    return (0);
}

void init8253() {
    memset(&i8253, 0, sizeof(i8253));
#if 0
    myTimer.begin(timer_isr, 54925);
#endif
}

