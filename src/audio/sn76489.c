/****************************************************************************

  emu76489.c -- SN76489 emulator by Mitsutaka Okazaki 2001-2016

  2001 08-13 : Version 1.00
  2001 10-03 : Version 1.01 -- Added SNG_set_quality().
  2004 05-23 : Version 1.10 -- Implemented GG stereo mode by RuRuRu
  2004 06-07 : Version 1.20 -- Improved the noise emulation.
  2015 12-13 : Version 1.21 -- Changed own integer types to C99 stdint.h types.
  2016 09-06 : Version 1.22 -- Support per-channel output.

  References: 
    SN76489 data sheet   
    sn76489.c   -- from MAME
    sn76489.txt -- from http://www.smspower.org/

*****************************************************************************/
// https://github.com/mamedev/mame/blob/master/src/devices/sound/sn76496.cpp
#include <stdint.h>

int32_t output;

uint32_t clock = 3579545;;
uint32_t samplerate = 44100;
uint32_t base_incr, quality = 0;

uint32_t count[3];
uint32_t volume[3];
uint32_t freq[3];
uint32_t edge[3];
uint32_t mute[3];

uint32_t noise_seed;
uint32_t noise_count;
uint32_t noise_freq;
uint32_t noise_volume;
uint32_t noise_mode;
uint32_t noise_fref;

uint32_t base_count;

/* rate converter */
uint32_t realstep;
uint32_t sngtime;
uint32_t sngstep;

uint32_t adr;

uint32_t stereo;

int16_t channel_sample[4];
// } sng = { 0 };


static const uint32_t voltbl[16] = {
    0xff, 0xcb, 0xa1, 0x80, 0x65, 0x50, 0x40, 0x33, 0x28, 0x20, 0x19, 0x14, 0x10, 0x0c, 0x0a, 0x00
};

#define GETA_BITS 24

void sn76489_reset() {
    if (quality) {
        base_incr = 1 << GETA_BITS;
        realstep = (uint32_t)((1 << 31) / samplerate);
        sngstep = (uint32_t)((1 << 31) / (clock / 16));
        sngtime = 0;
    }
    else {
        base_incr = (uint32_t)((double)clock * (1 << GETA_BITS) / (16 * samplerate));
    }

    for (int i = 0; i < 3; i++) {
        count[i] = 0;
        freq[i] = 0;
        edge[i] = 0;
        volume[i] = 0x0f;
        mute[i] = 0;
    }

    adr = 0;

    noise_seed = 0x8000;
    noise_count = 0;
    noise_freq = 0;
    noise_volume = 0x0f;
    noise_mode = 0;
    noise_fref = 0;

    output = 0;
    stereo = 0xFF;

    channel_sample[0] = channel_sample[1] = channel_sample[2] = channel_sample[3] = 0;
}

void sn76489_out(uint16_t value) {
    if (value & 0x80) {
        //printf("OK");
        adr = (value & 0x70) >> 4;
        switch (adr) {
            case 0: // tone 0: frequency
            case 2: // tone 1: frequency
            case 4: // tone 2: frequency
                freq[adr >> 1] = (freq[adr >> 1] & 0x3F0) | (value & 0x0F);
                break;

            case 1: // tone 0: volume
            case 3: // tone 1: volume
            case 5: // tone 2: volume
                volume[(adr - 1) >> 1] = value & 0xF;
                break;

            case 6: // noise: frequency, mode
                noise_mode = (value & 4) >> 2;

                if ((value & 0x03) == 0x03) {
                    noise_freq = freq[2];
                    noise_fref = 1;
                }
                else {
                    noise_freq = 32 << (value & 0x03);
                    noise_fref = 0;
                }

                if (noise_freq == 0)
                    noise_freq = 1;

                noise_seed = 0x8000;
                break;

            case 7: // noise: volume
                noise_volume = value & 0x0f;
                break;
        }
    }
    else {
        freq[adr >> 1] = ((value & 0x3F) << 4) | (freq[adr >> 1] & 0x0F);
    }
}

static inline int parity(int value) {
    value ^= value >> 8;
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return value & 1;
};

static inline void update_output() {
    base_count += base_incr;
    uint32_t incr = (base_count >> GETA_BITS);
    base_count &= (1 << GETA_BITS) - 1;

    /* Noise */
    noise_count += incr;
    if (noise_count & 0x400) {
        if (noise_mode) /* White */
            noise_seed = (noise_seed >> 1) | (parity(noise_seed & 0x0009) << 15);
        else /* Periodic */
            noise_seed = (noise_seed >> 1) | ((noise_seed & 1) << 15);

        if (noise_fref)
            noise_count -= freq[2];
        else
            noise_count -= noise_freq;
    }

    if (noise_seed & 1) {
        channel_sample[3] += voltbl[noise_volume] << 4;
    }
    channel_sample[3] >>= 1;

    /* Tone */
    for ( int i = 0; i < 3; i++) {
        count[i] += incr;
        if (count[i] & 0x400) {
            if (freq[i] > 1) {
                edge[i] = !edge[i];
                count[i] -= freq[i];
            }
            else {
                edge[i] = 1;
            }
        }

        if (edge[i] && !mute[i]) {
            channel_sample[i] += voltbl[volume[i]] << 4;
        }

        channel_sample[i] >>= 1;
    }
}

static inline int16_t mix_output() {
    output = channel_sample[0] + channel_sample[1] + channel_sample[2] + channel_sample[3];
    return (int16_t)output;
}

int16_t sn76489_sample() {
    if (!quality) {
        update_output();
        return mix_output();
    }

    /* Simple rate converter */
    while (realstep > sngtime) {
        sngtime += sngstep;
        update_output();
    }

    sngtime = sngtime - realstep;

    return mix_output();
}

static inline void mix_output_stereo(int32_t out[2]) {
    out[0] = out[1] = 0;
    if ((stereo >> 4) & 0x08) {
        out[0] += channel_sample[3];
    }
    if (stereo & 0x08) {
        out[1] += channel_sample[3];
    }

    for (int i = 0; i < 3; i++) {
        if ((stereo >> (i + 4)) & 0x01) {
            out[0] += channel_sample[i];
        }
        if ((stereo >> i) & 0x01) {
            out[1] += channel_sample[i];
        }
    }
}

void sn76489_sample_stereo(int32_t out[2]) {
    if (!quality) {
        update_output();
        mix_output_stereo(out);
        return;
    }

    while (realstep > sngtime) {
        sngtime += sngstep;
        update_output();
    }

    sngtime = sngtime - realstep;
    mix_output_stereo(out);
}
