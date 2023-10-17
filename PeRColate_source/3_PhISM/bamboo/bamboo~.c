/* bamboo~ from PeRColate - ported to PD by Olaf Matthes */

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

#define BAMB_SOUND_DECAY 0.95
#define BAMB_SYSTEM_DECAY 0.99995
#define BAMB_NUM_TUBES 5
#define BAMB_BASE_FREQ 2800

static t_class *bamboo_class;

typedef struct _bamboo {
    t_object x_obj;
    float shakeEnergy;
    float input, output[2];
    float coeffs[2];
    float input1, output1[2];
    float coeffs1[2];
    float input2, output2[2];
    float coeffs2[2];
    float sndLevel;
    float gain, gain1, gain2;
    float freq_rand;
    float soundDecay;
    float systemDecay;

    long num_objects;
    float shake_damp;
    float shake_max;
    float res_freq;

    long num_objectsSave; // number of beans
    float res_freqSave;   // resonance
    float shake_dampSave; // damping
    float shake_maxSave;
    float res_spread, res_random, res_spreadConnected, res_randomConnected;
    float res_spreadSave, res_randomSave;
    float srate, one_over_srate;
} t_bamboo;

/* -------------------------------- Pure Data --------------------------------
 */

int my_random(int max) {
    // https://stackoverflow.com/a/18386648
    return rand() / (RAND_MAX / (max + 1) + 1);
}

// noise maker
static t_float noise_tick(void) {
    t_float output;
    output = (t_float)rand() - 16384.;
    output *= ONE_OVER_RANDLIMIT;
    return output;
}

void bamboo_setup(t_bamboo *x) {
    x->num_objects = BAMB_NUM_TUBES;
    x->num_objectsSave = BAMB_NUM_TUBES;
    x->soundDecay = BAMB_SOUND_DECAY;
    x->systemDecay = BAMB_SYSTEM_DECAY;
    x->gain = 4.0 / (float)x->num_objects;
    x->coeffs[0] = -0.995 * 2.0 * cos(BAMB_BASE_FREQ * TWO_PI / x->srate);
    x->coeffs[1] = 0.995 * 0.995;
    x->coeffs1[0] =
        -0.995 * 2.0 * cos(BAMB_BASE_FREQ * 0.8 * TWO_PI / x->srate);
    x->coeffs1[1] = 0.995 * 0.995;
    x->coeffs2[0] =
        -0.995 * 2.0 * cos(BAMB_BASE_FREQ * 1.2 * TWO_PI / x->srate);
    x->coeffs2[1] = 0.995 * 0.995;
}

static t_float bamboo_tick(t_bamboo *x) {
    float data;
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
    return data;
}

static t_int *bamboo_perform(t_int *w) {
    t_bamboo *x = (t_bamboo *)(w[1]);

    float num_objects = x->num_objects;
    float res_freq = x->res_freq;
    float shake_damp = x->shake_damp;
    float shake_max = x->shake_max;
    float res_spread = x->res_spread;
    float res_random = x->res_random;

    t_float *out = (t_float *)(w[2]);
    long n = w[3];

    t_float lastOutput, temp;
    long temp2;

    if (num_objects != x->num_objectsSave) {
        if (num_objects < 1.)
            num_objects = 1.;
        x->num_objects = (long)num_objects;
        x->num_objectsSave = (long)num_objects;
        x->gain = log(num_objects) * 30. / (float)num_objects;
    }

    if (res_freq != x->res_freqSave) {
        x->res_freqSave = x->res_freq = res_freq;
        // temp = 900. * pow(1.015,res_freq);
        // x->coeffs[0] = -0.96 * 2.0 * cos(temp * TWO_PI / x->srate);
        // x->coeffs[1] = 0.96*0.96;
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
        x->res_spreadSave = x->res_spread = res_spread;
    }

    if (res_random != x->res_randomSave) {
        x->res_randomSave = x->res_random = res_random;
    }

    while (n--) {
        lastOutput = bamboo_tick(x);
        *out++ = lastOutput;
    }
    return w + 4;
}

static void bamboo_dsp(t_bamboo *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;

    dsp_add(bamboo_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void bamboo_float(t_bamboo *x, t_floatarg f) { x->num_objects = f; }

static void bamboo_res_freq(t_bamboo *x, t_floatarg f) { x->res_freq = f; }

static void bamboo_shake_damp(t_bamboo *x, t_floatarg f) { x->shake_damp = f; }

static void bamboo_shake_max(t_bamboo *x, t_floatarg f) { x->shake_max = f; }

static void bamboo_res_spread(t_bamboo *x, t_floatarg f) { x->res_spread = f; }

static void bamboo_res_random(t_bamboo *x, t_floatarg f) { x->res_random = f; }

static void bamboo_bang(t_bamboo *x) {
    int i;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output1[i] = 0.;
        x->output2[i] = 0.;
    }
    x->input = 0.0;
    x->input1 = 0.0;
    x->input2 = 0.0;
    x->num_objects = BAMB_NUM_TUBES;
    x->gain = 4.0 / (float)x->num_objects;
    x->sndLevel = 0.;
}

static void *bamboo_new(void) {
    unsigned int i;

    t_bamboo *x = (t_bamboo *)pd_new(bamboo_class);
    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object); i < sizeof(t_bamboo); i++)
            ((char *)x)[i] = 0;
    }
    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_damp"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_max"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_spread"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_random"));

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    x->shakeEnergy = 0.0;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output1[i] = 0.;
        x->output2[i] = 0.;
    }
    x->input = 0.0;
    x->input1 = 0.0;
    x->input2 = 0.0;
    x->sndLevel = 0.0;
    x->gain = 0.0;
    x->gain1 = 0;
    x->gain2 = 0;
    x->freq_rand = 0.0;
    x->soundDecay = 0.0;
    x->systemDecay = 0.0;

    x->shake_damp = 0.9;
    x->shake_max = 0.;
    x->res_freq = 4000.;

    x->res_freqSave = x->res_freq;     // resonance
    x->shake_dampSave = x->shake_damp; // damping
    x->shake_maxSave = x->shake_max;

    x->res_spread = 0.;
    x->res_random = 0.;
    x->res_spreadSave = 0.;
    x->res_randomSave = 0.;

    bamboo_setup(x);

    srand(0.54);

    return (x);
}

void bamboo_tilde_setup(void) {
    bamboo_class = class_new(gensym("bamboo~"), (t_newmethod)bamboo_new, 0,
                             sizeof(t_bamboo), 0, 0);
    class_addmethod(bamboo_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(bamboo_class, (t_method)bamboo_dsp, gensym("dsp"), A_NULL);
    class_addfloat(bamboo_class, (t_method)bamboo_float);
    class_addbang(bamboo_class, (t_method)bamboo_bang);
    class_addmethod(bamboo_class, (t_method)bamboo_res_freq, gensym("res_freq"),
                    A_FLOAT, A_NULL);
    class_addmethod(bamboo_class, (t_method)bamboo_shake_damp,
                    gensym("shake_damp"), A_FLOAT, A_NULL);
    class_addmethod(bamboo_class, (t_method)bamboo_shake_max,
                    gensym("shake_max"), A_FLOAT, A_NULL);
    class_addmethod(bamboo_class, (t_method)bamboo_res_spread,
                    gensym("res_spread"), A_FLOAT, A_NULL);
    class_addmethod(bamboo_class, (t_method)bamboo_res_random,
                    gensym("res_random"), A_FLOAT, A_NULL);
}
