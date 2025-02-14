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

#define TAMB_SOUND_DECAY 0.95
#define TAMB_SYSTEM_DECAY 0.9985
#define TAMB_NUM_TIMBRELS 32
#define TAMB_SHELL_FREQ 2300
#define TAMB_SHELL_GAIN 0.1
#define TAMB_CYMB_FREQ 5600
#define TAMB_CYMB_FREQ2 8100
#define TAMB_CYMB_RESON 0.99

void *tamb_class;

typedef struct _tamb {
    t_object x_obj;
    float shakeEnergy;
    float input, output[2];
    float coeffs[2];
    float input1, output1[2];
    float coeffs1[2];
    float input2, output2[2];
    float coeffs2[2];
    float sndLevel;
    float gain;
    float soundDecay;
    float systemDecay;
    float cymb_rand;

    long num_objects;
    float shake_damp;
    float shake_max;
    float res_freq;

    long num_objectsSave; // number of beans
    float res_freqSave;   // resonance
    float shake_dampSave; // damping
    float shake_maxSave;
    float res_freq1, res_freq2, res_freq1Connected, res_freq2Connected;
    float res_freq1Save, res_freq2Save;

    float totalEnergy;
    float finalZ[3];

    // signals connected? or controls...
    short num_objectsConnected;
    short res_freqConnected;
    short shake_dampConnected;
    short shake_maxConnected;

    float srate, one_over_srate;
} t_tamb;

/****PROTOTYPES****/

// setup funcs
static void *tamb_new(double val);
static void tamb_dsp(t_tamb *x, t_signal **sp, short *count);
static void tamb_float(t_tamb *x, double f);
static void tamb_int(t_tamb *x, int f);
static void tamb_bang(t_tamb *x);
static t_int *tamb_perform(t_int *w);
static void tamb_assist(t_tamb *x, void *b, long m, long a, char *s);

static void tamb_setup(t_tamb *x);
static float tamb_tick(t_tamb *x);

// noise maker
static float noise_tick(void);
static int my_random(int max);

/****FUNCTIONS****/

static void tamb_setup(t_tamb *x) {

    x->num_objects = x->num_objectsSave = TAMB_NUM_TIMBRELS;
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

static float tamb_tick(t_tamb *x) {
    float data;
    x->shakeEnergy *= x->systemDecay; // Exponential System Decay
    if (my_random(1024) < x->num_objects) {
        x->sndLevel += x->gain * x->shakeEnergy;
        x->cymb_rand = noise_tick() * x->res_freq1 * 0.05;
        x->coeffs1[0] = -TAMB_CYMB_RESON * 2.0 *
                        cos((x->res_freq + x->cymb_rand) * TWO_PI / x->srate);
        x->cymb_rand = noise_tick() * x->res_freq2 * 0.05;
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

    return data;
}

static int my_random(int max) { //  Return Random Int Between 0 and max
    unsigned long temp;
    temp = (unsigned long)rand();
    temp *= (unsigned long)max;
    temp >>= 15;
    return (int)temp;
}

// noise maker
static float noise_tick() {
    float output;
    output = (float)rand() - 16384.;
    output *= ONE_OVER_RANDLIMIT;
    return output;
}

static void tamb_res_freq(t_tamb *x, double f);
static void tamb_shake_dump(t_tamb *x, double f);
static void tamb_shake_max(t_tamb *x, double f);
static void tamb_res_freq1(t_tamb *x, double f);
static void tamb_res_freq2(t_tamb *x, double f);

void tamb_tilde_setup(void) {
    tamb_class = class_new(gensym("tamb~"), (t_newmethod)tamb_new, 0,
                           (short)sizeof(t_tamb), 0L, A_DEFFLOAT, 0);
    class_addmethod(tamb_class, (t_method)tamb_dsp, gensym("dsp"), 0);
    class_addfloat(tamb_class, (t_method)tamb_float);
    class_addmethod(tamb_class, (t_method)tamb_res_freq, gensym("res_freq"),
                    A_FLOAT, 0);
    class_addmethod(tamb_class, (t_method)tamb_res_freq1, gensym("res_freq1"),
                    A_FLOAT, 0);
    class_addmethod(tamb_class, (t_method)tamb_res_freq2, gensym("res_freq2"),
                    A_FLOAT, 0);
    class_addmethod(tamb_class, (t_method)tamb_shake_dump, gensym("shake_dump"),
                    A_FLOAT, 0);
    class_addmethod(tamb_class, (t_method)tamb_shake_max, gensym("shake_max"),
                    A_FLOAT, 0);
    class_addbang(tamb_class, (t_method)tamb_bang);
}

static void tamb_float(t_tamb *x, double f) { x->num_objects = (long)f; }

static void tamb_res_freq(t_tamb *x, double f) { x->res_freq = f; }

static void tamb_shake_dump(t_tamb *x, double f) { x->shake_damp = f; }

static void tamb_shake_max(t_tamb *x, double f) { x->shake_max = f; }

static void tamb_res_freq1(t_tamb *x, double f) { x->res_freq1 = f; }

static void tamb_res_freq2(t_tamb *x, double f) { x->res_freq2 = f; }

static void tamb_bang(t_tamb *x) {
    int i;
    post("tamb: zeroing delay lines");
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output1[i] = 0.;
        x->output2[i] = 0.;
        // x->output3[i] = 0.;
        // x->output4[i] = 0.;
    }
    x->input = 0.0;
    x->input1 = 0.0;
    x->input2 = 0.0;
    // x->input3 = 0.0;
    // x->input4 = 0.0;
    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }
}

static void *tamb_new(double initial_coeff) {
    unsigned int i;

    t_tamb *x = (t_tamb *)pd_new(tamb_class);

    if (x) {
        for (i = sizeof(t_object); i < sizeof(t_tamb); i++)
            ((char *)x)[i] = 0;
    }
    outlet_new((t_object *)x, gensym("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_dump"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_max"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq1"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq2"));

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
    x->soundDecay = 0.0;
    x->systemDecay = 0.0;
    x->cymb_rand = 0.0;
    x->shake_damp = x->shake_dampSave = 0.9;
    x->shake_max = x->shake_maxSave = 0.9;
    x->totalEnergy = 0.;
    x->res_freq = x->res_freq1 = x->res_freq2 = 3000.;
    x->res_freqSave = x->res_freq1Save = x->res_freq2Save = 3000.;

    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }

    tamb_setup(x);

    srand(0.54);

    return (x);
}

static void tamb_dsp(t_tamb *x, t_signal **sp, short *count) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;

    dsp_add(tamb_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
}

static t_int *tamb_perform(t_int *w) {
    t_tamb *x = (t_tamb *)(w[1]);

    float num_objects = x->num_objects;
    float shake_damp = x->shake_damp;
    float shake_max = x->shake_max;
    float res_freq = x->res_freq;
    float res_freq1 = x->res_freq1;
    float res_freq2 = x->res_freq2;

    float *out = (float *)(w[2]);
    long n = w[3];

    float lastOutput, temp;
    long temp2;

    if (num_objects != x->num_objectsSave) {
        if (num_objects < 1.)
            num_objects = 1.;
        x->num_objects = (long)num_objects;
        x->num_objectsSave = (long)num_objects;
        x->gain = 24.0 / x->num_objects;
    }

    if (res_freq != x->res_freqSave) {
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
        x->res_freq1Save = x->res_freq1 = res_freq1;
        // x->coeffs1[0] = -TAMB_CYMB_RESON * 2.0 * cos(res_freq1 * TWO_PI /
        // x->srate);
    }

    if (res_freq2 != x->res_freq2Save) {
        x->res_freq2Save = x->res_freq2 = res_freq2;
        // x->coeffs2[0] = -TAMB_CYMB_RESON * 2.0 * cos(res_freq2 * TWO_PI /
        // x->srate);
    }

    while (n--) {
        lastOutput = tamb_tick(x);
        *out++ = lastOutput;
    }
    return w + 4;
}
