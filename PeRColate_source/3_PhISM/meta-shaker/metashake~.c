/* ported to Pure-Data by Olaf Matthes <olaf.matthes@gmx.de> */

#include "m_pd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define TWO_PI 6.283185307
#define ONE_OVER_RANDLIMIT 0.00006103516 // constant = 1. / 16384.0
#define ATTACK 0
#define DECAY 1
#define SUSTAIN 2
#define RELEASE 3
#define MAX_RANDOM 32768
#define MAX_SHAKE 1.0

#define NUMOBJECTS_SCALE 1000.
#define FREQ_SCALE 20000.

#define SLEI_SOUND_DECAY 0.97
#define SLEI_SYSTEM_DECAY 0.9994
#define SLEI_NUM_BELLS 32.
#define SLEI_CYMB_FREQ0 2500.
#define SLEI_CYMB_FREQ1 5300.
#define SLEI_CYMB_FREQ2 6500.
#define SLEI_CYMB_FREQ3 8300.
#define SLEI_CYMB_FREQ4 9800.
#define SLEI_CYMB_RESON 0.99

#define BAMB_SOUND_DECAY 0.95
#define BAMB_SYSTEM_DECAY 0.99995
#define BAMB_NUM_TUBES 5.
#define BAMB_BASE_FREQ 2800.

#define GUIR_SOUND_DECAY 0.95
#define GUIR_NUM_RATCHETS 128.
#define GUIR_GOURD_FREQ 2500.0
#define GUIR_GOURD_RESON 0.97
#define GUIR_GOURD_FREQ2 4000.0
#define GUIR_GOURD_RESON2 0.97

#define TAMB_SOUND_DECAY 0.95
#define TAMB_SYSTEM_DECAY 0.9985
#define TAMB_NUM_TIMBRELS 32.
#define TAMB_SHELL_FREQ 2300.
#define TAMB_SHELL_GAIN 0.1
#define TAMB_CYMB_FREQ 5600.
#define TAMB_CYMB_FREQ2 8100.
#define TAMB_CYMB_RESON 0.99

#define SEKE_SOUND_DECAY 0.96
#define SEKE_SYSTEM_DECAY 0.999
#define SEKE_NUM_BEANS 64.

#define CABA_SOUND_DECAY 0.95
#define CABA_SYSTEM_DECAY 0.997
#define CABA_NUM_BEADS 512.

#define MARA_SOUND_DECAY 0.95
#define MARA_SYSTEM_DECAY 0.999
#define MARA_NUM_BEANS 25.

#define SLEIGH 10
#define BAMBOO 11
#define GUIRO 12
#define TAMB 13
#define SEKE 14
#define CABA 15
#define MARA 16
#define OFF 17

static t_class *metashake_class;

typedef struct _metashake {
    // header
    t_object x_obj;

    // user controlled vars

    t_float guiroScrape;
    t_float shakeEnergy;
    t_float input, output[2];
    t_float coeffs[2];
    t_float input1, output1[2];
    t_float coeffs1[2];
    t_float input2, output2[2];
    t_float coeffs2[2];
    t_float input3, output3[2];
    t_float coeffs3[2];
    t_float input4, output4[2];
    t_float coeffs4[2];
    t_float sndLevel;
    t_float gain, gain1, gain2;
    t_float freq_rand;
    t_float soundDecay;
    t_float systemDecay;
    t_float cymb_rand;

    t_int shakertype;

    t_float num_objects;
    t_float shake_damp;
    t_float shake_max;
    t_float res_freq;

    t_float num_objectsSave; // number of beans
    t_float res_freqSave;    // resonance
    t_float shake_dampSave;  // damping
    t_float shake_maxSave;
    t_float res_freq1, res_freq2, res_freq1Connected, res_freq2Connected;
    t_float res_freq1Save, res_freq2Save;
    t_float res_freq3, res_freq4, res_freq3Connected, res_freq4Connected;
    t_float res_freq3Save, res_freq4Save;
    t_float res_spread, res_random, res_spreadConnected, res_randomConnected;
    t_float res_spreadSave, res_randomSave;
    t_float scrapeVelConnected;
    t_float scrapeVelSave;

    t_float collLikely, scrapeVel;
    t_float totalEnergy;
    t_float ratchet, ratchetDelta;
    t_float finalZ[3];

    t_float srate, one_over_srate;
    t_int power;
} t_metashake;

/****PROTOTYPES****/

// noise maker
static t_float noise_tick(void);
static t_int my_random(t_int max);

static void metashake_clear_dlines(t_metashake *x);

/****FUNCTIONS****/
static void maraca_setup(t_metashake *x) {
    x->num_objects = MARA_NUM_BEANS;
    x->num_objectsSave = x->num_objects;
    x->gain = log(x->num_objects) / log(4.0) * 40. / (t_float)x->num_objects;
    x->coeffs[0] = -0.96 * 2.0 * cos(3200.0 * TWO_PI / x->srate);
    x->coeffs[1] = 0.96 * 0.96;
    x->soundDecay = MARA_SOUND_DECAY;
    x->systemDecay = MARA_SYSTEM_DECAY;
}

static t_float maraca_tick(t_metashake *x) {
    t_float data;
    x->shakeEnergy *= x->systemDecay;            // Exponential system decay
    if (my_random(1024) < x->num_objects)        // If collision
        x->sndLevel += x->gain * x->shakeEnergy; //   add energy
    x->input = x->sndLevel * noise_tick();       // Actual Sound is Random
    x->sndLevel *= x->soundDecay;                // Exponential Sound decay
    x->input -= x->output[0] * x->coeffs[0];     // Do gourd
    x->input -= x->output[1] * x->coeffs[1];     //   resonance
    x->output[1] = x->output[0];                 //     filter
    x->output[0] = x->input;                     //       calculations
    data = x->output[0] - x->output[1];

    return 40. * data;
}

static void cabasa_setup(t_metashake *x) {
    x->num_objects = CABA_NUM_BEADS;
    x->num_objectsSave = x->num_objects;
    x->gain = log(x->num_objects) / log(4.0) * 120.0 / (t_float)x->num_objects;
    x->coeffs[0] = -0.7 * 2.0 * cos(3000.0 * TWO_PI / x->srate);
    x->coeffs[1] = 0.7 * 0.7;
    x->soundDecay = CABA_SOUND_DECAY;
    x->systemDecay = CABA_SYSTEM_DECAY;
}

static t_float cabasa_tick(t_metashake *x) {
    t_float data;
    x->shakeEnergy *= x->systemDecay;            // Exponential system decay
    if (my_random(1024) < x->num_objects)        // If collision
        x->sndLevel += x->gain * x->shakeEnergy; //   add energy
    x->input = x->sndLevel * noise_tick();       // Actual Sound is Random
    x->sndLevel *= x->soundDecay;                // Exponential Sound decay
    x->input -= x->output[0] * x->coeffs[0];     // Do gourd
    x->input -= x->output[1] * x->coeffs[1];     //   resonance
    x->output[1] = x->output[0];                 //     filter
    x->output[0] = x->input;                     //       calculations
    data = x->output[0] - x->output[1];

    return 10. * data;
}

static void sekere_setup(t_metashake *x) {
    x->num_objects = SEKE_NUM_BEANS;
    x->num_objectsSave = x->num_objects;
    x->gain = log(x->num_objects) / log(4.0) * 40.0 / (t_float)x->num_objects;
    x->coeffs[0] = -0.6 * 2.0 * cos(5500.0 * TWO_PI / x->srate);
    x->coeffs[1] = 0.6 * 0.6;
    x->soundDecay = SEKE_SOUND_DECAY;
    x->systemDecay = SEKE_SYSTEM_DECAY;
}

static t_float sekere_tick(t_metashake *x) {
    t_float data;
    x->shakeEnergy *= x->systemDecay;            // Exponential system decay
    if (my_random(1024) < x->num_objects)        // If collision
        x->sndLevel += x->gain * x->shakeEnergy; //   add energy
    x->input = x->sndLevel * noise_tick();       // Actual Sound is Random
    x->sndLevel *= x->soundDecay;                // Exponential Sound decay
    x->input -= x->output[0] * x->coeffs[0];     // Do gourd
    x->input -= x->output[1] * x->coeffs[1];     //   resonance
    x->output[1] = x->output[0];                 //     filter
    x->output[0] = x->input;                     //       calculations
    x->finalZ[2] = x->finalZ[1];
    x->finalZ[1] = x->finalZ[0];
    x->finalZ[0] = x->output[1];
    data = x->finalZ[0] - x->finalZ[2];

    return 10. * data;
}

static void tamb_setup(t_metashake *x) {
    x->num_objects = TAMB_NUM_TIMBRELS;
    x->num_objectsSave = x->num_objects;
    x->soundDecay = TAMB_SOUND_DECAY;
    x->systemDecay = TAMB_SYSTEM_DECAY;
    x->gain = 24.0 / x->num_objects;
    x->coeffs[0] = -0.96 * 2.0 * cos(TAMB_SHELL_FREQ * TWO_PI / x->srate);
    x->coeffs[1] = 0.96 * 0.96;
    x->coeffs1[0] =
        -TAMB_CYMB_RESON * 2.0 * cos(TAMB_CYMB_FREQ * TWO_PI / x->srate);
    x->coeffs1[1] = TAMB_CYMB_RESON * TAMB_CYMB_RESON;
    x->coeffs2[0] =
        -TAMB_CYMB_RESON * 2.0 * cos(TAMB_CYMB_FREQ2 * TWO_PI / x->srate);
    x->coeffs2[1] = TAMB_CYMB_RESON * TAMB_CYMB_RESON;
}

static t_float tamb_tick(t_metashake *x) {
    t_float data;
    x->shakeEnergy *= x->systemDecay; // Exponential System Decay
    if (my_random(1024) < x->num_objects) {
        x->sndLevel += x->gain * x->shakeEnergy;
        x->cymb_rand = noise_tick() * x->res_freq1 * x->res_random; // * 0.05;
        x->coeffs1[0] = -TAMB_CYMB_RESON * 2.0 *
                        cos((x->res_freq1 + x->cymb_rand) * TWO_PI / x->srate);
        x->cymb_rand = noise_tick() * x->res_freq2 * x->res_random; //* 0.05;
        x->coeffs2[0] = -TAMB_CYMB_RESON * 2.0 *
                        cos((x->res_freq2 + x->cymb_rand) * TWO_PI / x->srate);
    }
    x->input = x->sndLevel;
    x->input *= noise_tick(); // Actual Sound is Random
    x->input1 = x->input * 0.8;
    x->input2 = x->input;
    x->input *= TAMB_SHELL_GAIN;

    x->sndLevel *= x->soundDecay; // Each (all) event(s)
    // decay(s) exponentially
    x->input -= x->output[0] * x->coeffs[0];
    x->input -= x->output[1] * x->coeffs[1];
    x->output[1] = x->output[0];
    x->output[0] = x->input;
    data = x->output[0];
    x->input1 -= x->output1[0] * x->coeffs1[0];
    x->input1 -= x->output1[1] * x->coeffs1[1];
    x->output1[1] = x->output1[0];
    x->output1[0] = x->input1;
    data += x->output1[0];
    x->input2 -= x->output2[0] * x->coeffs2[0];
    x->input2 -= x->output2[1] * x->coeffs2[1];
    x->output2[1] = x->output2[0];
    x->output2[0] = x->input2;
    data += x->output2[0];

    x->finalZ[2] = x->finalZ[1];
    x->finalZ[1] = x->finalZ[0];
    x->finalZ[0] = data;
    data = x->finalZ[2] - x->finalZ[0];

    return 100. * data; // tambourine is quiet...
}

static void guiro_setup(t_metashake *x) {
    x->num_objects = GUIR_NUM_RATCHETS;
    x->num_objectsSave = x->num_objects;
    x->soundDecay = GUIR_SOUND_DECAY;
    x->coeffs[0] =
        -GUIR_GOURD_RESON * 2.0 * cos(GUIR_GOURD_FREQ * TWO_PI / x->srate);
    x->coeffs[1] = GUIR_GOURD_RESON * GUIR_GOURD_RESON;
    x->coeffs1[0] =
        -GUIR_GOURD_RESON2 * 2.0 * cos(GUIR_GOURD_FREQ2 * TWO_PI / x->srate);
    x->coeffs1[1] = GUIR_GOURD_RESON2 * GUIR_GOURD_RESON2;
    x->ratchet = 0.;
    x->guiroScrape = 0.;
}

static t_float guiro_tick(t_metashake *x) {
    t_float data;
    if (my_random(1024) < x->num_objects) {
        x->sndLevel += 512. * x->ratchet * x->totalEnergy;
    }
    x->input = x->sndLevel;
    x->input *= noise_tick() * x->ratchet;
    x->sndLevel *= x->soundDecay;

    x->input2 = x->input;
    x->input -= x->output[0] * x->coeffs[0];
    x->input -= x->output[1] * x->coeffs[1];
    x->output[1] = x->output[0];
    x->output[0] = x->input;
    x->input2 -= x->output2[0] * x->coeffs1[0];
    x->input2 -= x->output2[1] * x->coeffs1[1];
    x->output2[1] = x->output2[0];
    x->output2[0] = x->input2;

    x->finalZ[2] = x->finalZ[1];
    x->finalZ[1] = x->finalZ[0];
    x->finalZ[0] = x->output[1] + x->output2[1];
    data = x->finalZ[0] - x->finalZ[2];

    return 0.05 * data; // guiro is LOUD
}

static void bamboo_setup(t_metashake *x) {
    x->num_objects = BAMB_NUM_TUBES;
    x->num_objectsSave = x->num_objects;
    x->soundDecay = BAMB_SOUND_DECAY;
    x->systemDecay = BAMB_SYSTEM_DECAY;
    x->gain = 4.0 / (t_float)x->num_objects;
    x->coeffs[0] = -0.995 * 2.0 * cos(BAMB_BASE_FREQ * TWO_PI / x->srate);
    x->coeffs[1] = 0.995 * 0.995;
    x->coeffs1[0] =
        -0.995 * 2.0 * cos(BAMB_BASE_FREQ * 0.8 * TWO_PI / x->srate);
    x->coeffs1[1] = 0.995 * 0.995;
    x->coeffs2[0] =
        -0.995 * 2.0 * cos(BAMB_BASE_FREQ * 1.2 * TWO_PI / x->srate);
    x->coeffs2[1] = 0.995 * 0.995;
}

static t_float bamboo_tick(t_metashake *x) {
    t_float data;
    x->shakeEnergy *= x->systemDecay; // Exponential System Decay
    if (my_random(4096) < x->num_objects) {
        x->sndLevel += x->gain * x->shakeEnergy;
        x->freq_rand = x->res_freq * (1.0 + (x->res_random * noise_tick()));
        x->coeffs[0] = -0.995 * 2.0 * cos(x->freq_rand * TWO_PI / x->srate);
        x->freq_rand =
            x->res_freq * (1. - x->res_spread + (x->res_random * noise_tick()));
        x->coeffs1[0] = -0.995 * 2.0 * cos(x->freq_rand * TWO_PI / x->srate);
        x->freq_rand = x->res_freq * (1. + 2. * x->res_spread +
                                      (2. * x->res_random * noise_tick()));
        x->coeffs2[0] = -0.995 * 2.0 * cos(x->freq_rand * TWO_PI / x->srate);
    }
    x->input = x->sndLevel;
    x->input *= noise_tick(); // Actual Sound is Random
    x->input1 = x->input;
    x->input2 = x->input;

    x->sndLevel *= x->soundDecay; // Each (all) event(s)
    // decay(s) exponentially
    x->input -= x->output[0] * x->coeffs[0];
    x->input -= x->output[1] * x->coeffs[1];
    x->output[1] = x->output[0];
    x->output[0] = x->input;
    data = x->output[0];
    x->input1 -= x->output1[0] * x->coeffs1[0];
    x->input1 -= x->output1[1] * x->coeffs1[1];
    x->output1[1] = x->output1[0];
    x->output1[0] = x->input1;
    data += x->output1[0];
    x->input2 -= x->output2[0] * x->coeffs2[0];
    x->input2 -= x->output2[1] * x->coeffs2[1];
    x->output2[1] = x->output2[0];
    x->output2[0] = x->input2;
    data += x->output2[0];
    return 5. * data;
}

static void sleigh_setup(t_metashake *x) {
    x->num_objects = SLEI_NUM_BELLS;
    x->num_objectsSave = x->num_objects;
    x->soundDecay = SLEI_SOUND_DECAY;
    x->systemDecay = SLEI_SYSTEM_DECAY;
    /*x->res_freq = SLEI_CYMB_FREQ0;
    x->res_freq1 = SLEI_CYMB_FREQ1;
    x->res_freq2 = SLEI_CYMB_FREQ2;
    x->res_freq3 = SLEI_CYMB_FREQ3;
    x->res_freq4 = SLEI_CYMB_FREQ4;*/

    // x->gain = 8.0 / x->num_objects;
    x->gain = log(x->num_objects) * 30. / (t_float)x->num_objects;

    x->coeffs[1] = SLEI_CYMB_RESON * SLEI_CYMB_RESON;
    x->coeffs1[1] = SLEI_CYMB_RESON * SLEI_CYMB_RESON;
    x->coeffs2[1] = SLEI_CYMB_RESON * SLEI_CYMB_RESON;
    x->coeffs3[1] = SLEI_CYMB_RESON * SLEI_CYMB_RESON;
    x->coeffs4[1] = SLEI_CYMB_RESON * SLEI_CYMB_RESON;
}

static t_float sleigh_tick(t_metashake *x) {
    t_float data;
    x->shakeEnergy *= x->systemDecay; // Exponential System Decay
    if (my_random(1024) < x->num_objects) {
        x->sndLevel += x->gain * x->shakeEnergy;
        x->cymb_rand = noise_tick() * x->res_freq * 0.05;
        x->coeffs[0] =
            -SLEI_CYMB_RESON * 2.0 *
            cos((x->res_freq + x->cymb_rand) * TWO_PI * x->one_over_srate);
        x->cymb_rand = noise_tick() * x->res_freq1 * 0.03;
        x->coeffs1[0] =
            -SLEI_CYMB_RESON * 2.0 *
            cos((x->res_freq1 + x->cymb_rand) * TWO_PI * x->one_over_srate);
        x->cymb_rand = noise_tick() * x->res_freq2 * 0.03;
        x->coeffs2[0] =
            -SLEI_CYMB_RESON * 2.0 *
            cos((x->res_freq2 + x->cymb_rand) * TWO_PI * x->one_over_srate);
        x->cymb_rand = noise_tick() * x->res_freq3 * 0.03;
        x->coeffs3[0] =
            -SLEI_CYMB_RESON * 2.0 *
            cos((x->res_freq3 + x->cymb_rand) * TWO_PI * x->one_over_srate);
        x->cymb_rand = noise_tick() * x->res_freq4 * 0.03;
        x->coeffs4[0] =
            -SLEI_CYMB_RESON * 2.0 *
            cos((x->res_freq4 + x->cymb_rand) * TWO_PI * x->one_over_srate);
    }
    x->input = x->sndLevel;
    x->input *= noise_tick(); // Actual Sound is Random
    x->input1 = x->input;
    x->input2 = x->input;
    x->input3 = x->input * 0.5;
    x->input4 = x->input * 0.3;

    x->sndLevel *= x->soundDecay; // Each (all) event(s)
    // decay(s) exponentially
    x->input -= x->output[0] * x->coeffs[0];
    x->input -= x->output[1] * x->coeffs[1];
    x->output[1] = x->output[0];
    x->output[0] = x->input;
    data = x->output[0];

    x->input1 -= x->output1[0] * x->coeffs1[0];
    x->input1 -= x->output1[1] * x->coeffs1[1];
    x->output1[1] = x->output1[0];
    x->output1[0] = x->input1;
    data += x->output1[0];

    x->input2 -= x->output2[0] * x->coeffs2[0];
    x->input2 -= x->output2[1] * x->coeffs2[1];
    x->output2[1] = x->output2[0];
    x->output2[0] = x->input2;
    data += x->output2[0];

    x->input3 -= x->output3[0] * x->coeffs3[0];
    x->input3 -= x->output3[1] * x->coeffs3[1];
    x->output3[1] = x->output3[0];
    x->output3[0] = x->input3;
    data += x->output3[0];

    x->input4 -= x->output4[0] * x->coeffs4[0];
    x->input4 -= x->output4[1] * x->coeffs4[1];
    x->output4[1] = x->output4[0];
    x->output4[0] = x->input4;
    data += x->output4[0];

    x->finalZ[2] = x->finalZ[1];
    x->finalZ[1] = x->finalZ[0];
    x->finalZ[0] = data;
    data = x->finalZ[2] - x->finalZ[0];

    return 5. * data;
}

static t_int my_random(t_int max) { //  Return Random Int Between 0 and max
    unsigned long temp;
    temp = (unsigned long)rand();
    temp *= (unsigned long)max;
    temp >>= 15;
    return (t_int)temp;
}

// noise maker
static t_float noise_tick(void) {
    t_float output;
    output = (t_float)rand() - 16384.;
    output *= ONE_OVER_RANDLIMIT;
    return output;
}

static t_int *metashake_perform(t_int *w) {
    t_metashake *x = (t_metashake *)(w[1]);

    t_float num_objects = x->num_objects;
    t_float shake_damp = x->shake_damp;
    t_float shake_max = x->shake_max;
    t_float res_freq = x->res_freq;
    t_float res_freq1 = x->res_freq1;
    t_float res_freq2 = x->res_freq2;
    t_float res_freq3 = x->res_freq3;
    t_float res_freq4 = x->res_freq4;

    t_float scrapeVel = x->scrapeVel;
    t_float res_spread = x->res_spread;
    t_float res_random = x->res_random;

    t_float *out = (t_float *)(w[2]);
    t_int n = w[3];

    t_float lastOutput, temp;

    // if(num_objects < 1.) num_objects *= NUMOBJECTS_SCALE;

    if (!x->power) {
        while (n--)
            *out++ = 0.;
    } else if (x->shakertype == MARA) {

        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            x->gain =
                log(num_objects) / log(4.0) * 120.0 / (t_float)num_objects;
        }

        if (res_freq != x->res_freqSave) {
            // res_freq *= FREQ_SCALE;
            x->res_freqSave = x->res_freq = res_freq;
            x->coeffs[0] = -0.96 * 2.0 * cos(res_freq * TWO_PI / x->srate);
        }

        if (shake_damp != x->shake_dampSave) {
            x->shake_dampSave = x->shake_damp = shake_damp;
            x->systemDecay = .998 + (shake_damp * .002);
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
            if (x->shakeEnergy > MAX_SHAKE)
                x->shakeEnergy = MAX_SHAKE;
        }

        while (n--) {
            lastOutput = maraca_tick(x);
            *out++ = lastOutput;
        }
    }

    else if (x->shakertype == CABA) {

        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            x->gain =
                log(num_objects) / log(4.0) * 120.0 / (t_float)num_objects;
        }
        if (res_freq != x->res_freqSave) {
            // res_freq *= FREQ_SCALE;
            x->res_freqSave = x->res_freq = res_freq;
            x->coeffs[0] = -0.7 * 2.0 * cos(res_freq * TWO_PI / x->srate);
        }

        if (shake_damp != x->shake_dampSave) {
            x->shake_dampSave = x->shake_damp = shake_damp;
            x->systemDecay = .998 + (shake_damp * .002);
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
            if (x->shakeEnergy > MAX_SHAKE)
                x->shakeEnergy = MAX_SHAKE;
        }

        while (n--) {
            lastOutput = cabasa_tick(x);
            *out++ = 10. * lastOutput;
        }
    }

    else if (x->shakertype == SEKE) {

        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            // x->gain = log(num_objects) * 30. / (t_float)num_objects;
            x->gain = log(num_objects) / log(4.0) * 40.0 / (t_float)num_objects;
        }

        if (res_freq != x->res_freqSave) {
            // res_freq *= FREQ_SCALE;
            x->res_freqSave = x->res_freq = res_freq;
            x->coeffs[0] = -0.6 * 2.0 * cos(res_freq * TWO_PI / x->srate);
        }

        if (shake_damp != x->shake_dampSave) {
            x->shake_dampSave = x->shake_damp = shake_damp;
            x->systemDecay = .998 + (shake_damp * .002);
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
            if (x->shakeEnergy > MAX_SHAKE)
                x->shakeEnergy = MAX_SHAKE;
        }

        while (n--) {
            lastOutput = sekere_tick(x);
            *out++ = 10. * lastOutput;
        }
    }

    else if (x->shakertype == GUIRO) {
        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            x->gain = log(num_objects) * 30.0 / (t_float)num_objects;
        }

        if (res_freq != x->res_freqSave) {
            // res_freq *= FREQ_SCALE;
            x->res_freqSave = x->res_freq = res_freq;
            x->coeffs[0] =
                -GUIR_GOURD_RESON * 2.0 * cos(res_freq * TWO_PI / x->srate);
        }

        if (scrapeVel != x->scrapeVelSave) {
            x->scrapeVelSave = x->scrapeVel = scrapeVel;
            // x->scrapeVel = shake_damp;
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->guiroScrape = shake_max;
        }

        if (res_freq1 != x->res_freq1Save) {
            // res_freq1 *= FREQ_SCALE;
            x->res_freq1Save = x->res_freq1 = res_freq1;
            x->coeffs1[0] =
                -GUIR_GOURD_RESON2 * 2.0 * cos(res_freq1 * TWO_PI / x->srate);
        }

        while (n--) {
            if (x->guiroScrape < 1.0) {
                x->guiroScrape += x->scrapeVel;
                x->totalEnergy = x->guiroScrape;
                x->ratchet -= (x->ratchetDelta + (0.002 * x->totalEnergy));
                if (x->ratchet < 0.0)
                    x->ratchet = 1.0;
                lastOutput = guiro_tick(x);
            } else
                lastOutput = 0.0;
            *out++ = lastOutput;
        }
    }

    else if (x->shakertype == TAMB) {

        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            x->gain = 24.0 / x->num_objects;
        }

        if (res_freq != x->res_freqSave) {
            // res_freq *= FREQ_SCALE;
            x->res_freqSave = x->res_freq = res_freq;
            x->coeffs[0] = -0.96 * 2.0 * cos(res_freq * TWO_PI / x->srate);
        }

        if (shake_damp != x->shake_dampSave) {
            x->shake_dampSave = x->shake_damp = shake_damp;
            x->systemDecay = .998 + (shake_damp * .002);
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
            if (x->shakeEnergy > MAX_SHAKE)
                x->shakeEnergy = MAX_SHAKE;
        }

        if (res_freq1 != x->res_freq1Save) {
            // res_freq1 *= FREQ_SCALE;
            x->res_freq1Save = x->res_freq1 = res_freq1;
            // x->coeffs1[0] = -TAMB_CYMB_RESON * 2.0 * cos(res_freq1 * TWO_PI /
            // x->srate);
        }

        if (res_freq2 != x->res_freq2Save) {
            // res_freq2 *= FREQ_SCALE;
            x->res_freq2Save = x->res_freq2 = res_freq2;
            // x->coeffs2[0] = -TAMB_CYMB_RESON * 2.0 * cos(res_freq2 * TWO_PI /
            // x->srate);
        }
        if (res_random != x->res_randomSave) {
            x->res_random = x->res_randomSave = res_random;
            if (x->res_random > .4)
                x->res_random = .4;
            if (x->res_random < 0.)
                x->res_random = 0.;
        }

        while (n--) {
            lastOutput = tamb_tick(x);
            *out++ = lastOutput;
        }

    }

    else if (x->shakertype == SLEIGH) {
        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            x->gain = log(num_objects) * 30. / (t_float)num_objects;
        }

        if (res_freq != x->res_freqSave) {
            x->res_freqSave = x->res_freq = res_freq; //*FREQ_SCALE;
        }

        if (shake_damp != x->shake_dampSave) {
            x->shake_dampSave = x->shake_damp = shake_damp;
            x->systemDecay = .998 + (shake_damp * .002);
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
            if (x->shakeEnergy > MAX_SHAKE)
                x->shakeEnergy = MAX_SHAKE;
        }

        if (res_freq1 != x->res_freq1Save) {
            x->res_freq1Save = x->res_freq1 = res_freq1; //*FREQ_SCALE;
        }

        if (res_freq2 != x->res_freq2Save) {
            x->res_freq2Save = x->res_freq2 = res_freq2; //*FREQ_SCALE;
        }

        if (res_freq3 != x->res_freq3Save) {
            x->res_freq3Save = x->res_freq3 = res_freq3; //*FREQ_SCALE;
        }

        if (res_freq4 != x->res_freq4Save) {
            x->res_freq4Save = x->res_freq4 = res_freq4; //*FREQ_SCALE;
        }
        while (n--) {
            lastOutput = sleigh_tick(x);
            *out++ = lastOutput;
        }
    } else if (x->shakertype == BAMBOO) {
        if (num_objects != x->num_objectsSave) {
            if (num_objects < 1.)
                num_objects = 1.;
            x->num_objects = num_objects;
            x->num_objectsSave = num_objects;
            x->gain = log(num_objects) * 30. / (t_float)num_objects;
        }

        if (res_freq != x->res_freqSave) {
            x->res_freqSave = x->res_freq = res_freq; //*FREQ_SCALE;
        }

        if (shake_damp != x->shake_dampSave) {
            x->shake_dampSave = x->shake_damp = shake_damp;
            x->systemDecay = .998 + (shake_damp * .002);
        }

        if (shake_max != x->shake_maxSave) {
            x->shake_maxSave = x->shake_max = shake_max;
            x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
            if (x->shakeEnergy > MAX_SHAKE)
                x->shakeEnergy = MAX_SHAKE;
        }

        if (res_spread != x->res_spreadSave) {
            x->res_spread = x->res_spreadSave = res_spread;
            if (x->res_spread > .4)
                x->res_spread = .4;
            if (x->res_spread < 0.)
                x->res_spread = 0.;
        }

        if (res_random != x->res_randomSave) {
            x->res_random = x->res_randomSave = res_random;
            if (x->res_random > .4)
                x->res_random = .4;
            if (x->res_random < 0.)
                x->res_random = 0.;
        }

        while (n--) {
            lastOutput = bamboo_tick(x);
            *out++ = lastOutput;
        }
    }

    else if (x->shakertype == OFF) {
        while (n--) {
            lastOutput = 0.;
            *out++ = lastOutput;
        }
    }

    return w + 4;
}

static void metashake_dsp(t_metashake *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    /* outlet     vectorsize */
    dsp_add(metashake_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void metashake_setsleigh(t_metashake *x) {
    post("metashaker: sleigh bells");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = SLEIGH;
    sleigh_setup(x);
}

static void metashake_setoff(t_metashake *x) {
    post("metashaker: turning off");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = OFF;
}

static void metashake_setseke(t_metashake *x) {
    post("metashaker: sekere");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = SEKE;
    sekere_setup(x);
}

static void metashake_settamb(t_metashake *x) {
    post("metashaker: tambourine");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = TAMB;
    tamb_setup(x);
}

static void metashake_setguiro(t_metashake *x) {
    post("metashaker: guiro");
    metashake_clear_dlines(x);

    x->guiroScrape = 0;
    x->scrapeVel = 0.00015;

    x->shakertype = GUIRO;
    guiro_setup(x);
}

static void metashake_setbamboo(t_metashake *x) {
    post("metashaker: bamboo chimes");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = BAMBOO;
    bamboo_setup(x);
}

static void metashake_setcabasa(t_metashake *x) {
    post("metashaker: cabasa");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = CABA;
    cabasa_setup(x);
}

static void metashake_setmaraca(t_metashake *x) {
    post("metashaker: maraca");
    metashake_clear_dlines(x);

    x->guiroScrape = 1.;
    x->totalEnergy = 0.;
    x->sndLevel = 0.;

    x->shakertype = MARA;
    maraca_setup(x);
}

/* float inlets */
static void metashake_float(t_metashake *x, t_floatarg f) {
    x->num_objects = f;
}
static void metashake_float1(t_metashake *x, t_floatarg f) {
    x->shake_damp = f;
}
static void metashake_float2(t_metashake *x, t_floatarg f) { x->shake_max = f; }
static void metashake_float3(t_metashake *x, t_floatarg f) { x->res_freq = f; }
static void metashake_float4(t_metashake *x, t_floatarg f) { x->res_freq1 = f; }
static void metashake_float5(t_metashake *x, t_floatarg f) { x->res_freq2 = f; }
void metashake_float6(t_metashake *x, t_floatarg f) { x->res_freq3 = f; }
static void metashake_float7(t_metashake *x, t_floatarg f) { x->res_freq4 = f; }
static void metashake_float8(t_metashake *x, t_floatarg f) { x->scrapeVel = f; }
static void metashake_float9(t_metashake *x, t_floatarg f) {
    x->res_spread = f;
}
static void metashake_float10(t_metashake *x, t_floatarg f) {
    x->res_random = f;
}

static void metashake_bang(t_metashake *x) {
    int i;
    post("metashake: zeroing delay lines");
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output1[i] = 0.;
        x->output2[i] = 0.;
        x->output3[i] = 0.;
        x->output4[i] = 0.;
    }
    x->input = 0.0;
    x->input1 = 0.0;
    x->input2 = 0.0;
    x->input3 = 0.0;
    x->input4 = 0.0;
    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }
}

static void setpower(t_metashake *x, t_symbol *s, t_int argc, t_atom *argv) {
    short i;
    int temp;
    for (i = 0; i < argc; i++) {
        switch (argv[i].a_type) {
        case A_FLOAT:
            temp = (int)argv[i].a_w.w_float;
            x->power = temp;
            post("shake: setting power: %d", temp);
            break;
        }
    }
}

static void metashake_clear_dlines(t_metashake *x) {
    int i;
    post("metashake: zeroing delay lines");
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output1[i] = 0.;
        x->output2[i] = 0.;
        x->output3[i] = 0.;
        x->output4[i] = 0.;
    }
    x->input = 0.0;
    x->input1 = 0.0;
    x->input2 = 0.0;
    x->input3 = 0.0;
    x->input4 = 0.0;
    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }
}

static void *metashake_new(void) {
    unsigned int i;

    t_metashake *x = (t_metashake *)pd_new(metashake_class);

    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object); i < sizeof(t_metashake); i++)
            ((char *)x)[i] = 0;
    }

    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_damp"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_max"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq1"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq2"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq3"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq4"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("scrapeVel"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_spread"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_random"));

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;
    x->power = 1;

    x->guiroScrape = 0;
    x->shakeEnergy = 0.0;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output1[i] = 0.;
        x->output2[i] = 0.;
        x->output3[i] = 0.;
        x->output4[i] = 0.;
    }
    x->input = 0.0;
    x->input1 = 0.0;
    x->input2 = 0.0;
    x->input3 = 0.0;
    x->input4 = 0.0;
    x->sndLevel = 0.0;
    x->gain = 0.0;
    x->gain1 = 0;
    x->gain2 = 0;
    x->freq_rand = 0.0;
    x->soundDecay = 0.0;
    x->systemDecay = 0.0;
    x->cymb_rand = 0.0;
    x->num_objects = x->num_objectsSave = 20;
    x->totalEnergy = 0.;
    x->res_spread = x->res_spreadSave = 0.;
    x->res_random = x->res_randomSave = 0.;
    x->res_freq = x->res_freq1 = x->res_freq2 = x->res_freq3 = x->res_freq4 =
        2220.;
    x->res_freqSave = x->res_freq1Save = x->res_freq2Save = x->res_freq3Save =
        x->res_freqSave = 2220.;
    x->collLikely = 0.;
    x->scrapeVel = x->scrapeVelSave = 0.00015;
    x->ratchet = 0.0;
    x->ratchetDelta = 0.0005;
    x->shake_damp = x->shake_dampSave = 0.9;
    x->shake_max = x->shake_maxSave = 0.9;
    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }

    x->shakertype = SLEIGH;

    sleigh_setup(x);

    srand(23);

    return (x);
}

void metashake_tilde_setup(void) {
    metashake_class =
        class_new(gensym("metashake~"), (t_newmethod)metashake_new, 0,
                  sizeof(t_metashake), 0, 0);
    class_addmethod(metashake_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_dsp, gensym("dsp"),
                    A_NULL);
    /* messages for leftmost inlet */
    class_addfloat(metashake_class, (t_method)metashake_float);
    class_addbang(metashake_class, (t_method)metashake_bang);
    class_addmethod(metashake_class, (t_method)metashake_setsleigh,
                    gensym("sleigh"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_setbamboo,
                    gensym("bamboo"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_setguiro,
                    gensym("guiro"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_settamb,
                    gensym("tambourine"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_setseke,
                    gensym("sekere"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_setcabasa,
                    gensym("cabasa"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_setmaraca,
                    gensym("maraca"), A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_setoff, gensym("off"),
                    A_NULL);
    class_addmethod(metashake_class, (t_method)setpower, gensym("power"),
                    A_GIMME, A_NULL);
    /* float inlets */
    class_addmethod(metashake_class, (t_method)metashake_float1,
                    gensym("shake_damp"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float2,
                    gensym("shake_max"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float3,
                    gensym("res_freq"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float4,
                    gensym("res_freq1"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float5,
                    gensym("res_freq2"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float6,
                    gensym("res_freq3"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float7,
                    gensym("res_freq4"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float8,
                    gensym("scrapeVel"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float9,
                    gensym("res_spread"), A_FLOAT, A_NULL);
    class_addmethod(metashake_class, (t_method)metashake_float10,
                    gensym("res_random"), A_FLOAT, A_NULL);
    class_sethelpsymbol(metashake_class, gensym("help-metashake~.pd"));
}
