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

#define SEKE_SOUND_DECAY 0.96
#define SEKE_SYSTEM_DECAY 0.999
#define SEKE_NUM_BEANS 64

void *sekere_class;

typedef struct _sekere {
    t_object x_obj;
    // user controlled vars
    float shakeEnergy;
    float input, output[2];
    float coeffs[2];

    float sndLevel;
    float gain;

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
    float finalZ[3];

    // signals connected? or controls...
    short num_objectsConnected;
    short res_freqConnected;
    short shake_dampConnected;
    short shake_maxConnected;

    float srate, one_over_srate;
} t_sekere;

/****PROTOTYPES****/

// setup funcs
static void *sekere_new(double val);
static void sekere_dsp(t_sekere *x, t_signal **sp, short *count);
static void sekere_float(t_sekere *x, double f);
// static void sekere_int(t_sekere *x, int f);
static void sekere_bang(t_sekere *x);
static t_int *sekere_perform(t_int *w);
// static void sekere_assist(t_sekere *x, void *b, long m, long a, char *s);

static void sekere_setup(t_sekere *x);
static float sekere_tick(t_sekere *x);

// noise maker
static float noise_tick(void);
static int my_random(int max);

/****FUNCTIONS****/

static void sekere_setup(t_sekere *x) {
    x->num_objects = x->num_objectsSave = SEKE_NUM_BEANS;
    x->gain = log(x->num_objects) / log(4.0) * 40.0 / (float)x->num_objects;
    x->coeffs[0] = -0.6 * 2.0 * cos(5500.0 * TWO_PI / x->srate);
    x->coeffs[1] = 0.6 * 0.6;
    x->soundDecay = SEKE_SOUND_DECAY;
    x->systemDecay = SEKE_SYSTEM_DECAY;
}

static float sekere_tick(t_sekere *x) {
    float data;
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
static float noise_tick(void) {
    float output;
    output = (float)rand() - 16384.;
    output *= ONE_OVER_RANDLIMIT;
    return output;
}
static void sekere_res_coeff(t_sekere *x, double f);
static void sekere_shake_dump(t_sekere *x, double f);
static void sekere_shake_max(t_sekere *x, double f);

void sekere_tilde_setup(void) {
    sekere_class = class_new(gensym("sekere~"), (t_newmethod)sekere_new, 0,
                             sizeof(t_sekere), 0, A_DEFFLOAT, 0);

    class_addmethod(sekere_class, (t_method)sekere_dsp, gensym("dsp"), 0);
    class_addfloat(sekere_class, (t_method)sekere_float);
    class_addmethod(sekere_class, (t_method)sekere_res_coeff,
                    gensym("res_coeff"), A_FLOAT, 0);
    class_addmethod(sekere_class, (t_method)sekere_shake_dump,
                    gensym("shake_dump"), A_FLOAT, 0);
    class_addmethod(sekere_class, (t_method)sekere_shake_max,
                    gensym("shake_max"), A_FLOAT, 0);
    class_addbang(sekere_class, (t_method)sekere_bang);
}
static void sekere_float(t_sekere *x, double f) { x->num_objects = (long)f; }

static void sekere_res_coeff(t_sekere *x, double f) {
    x->res_freq = f;
    x->coeffs[0] = -0.6 * 2.0 * cos(x->res_freq * TWO_PI / x->srate);
}

static void sekere_shake_dump(t_sekere *x, double f) { x->shake_damp = f; }

static void sekere_shake_max(t_sekere *x, double f) { x->shake_max = f; }

static void sekere_bang(t_sekere *x) {
    int i;
    // post("sekere: zeroing delay lines");
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        // x->output1[i] = 0.;
        // x->output2[i] = 0.;
        // x->output3[i] = 0.;
        // x->output4[i] = 0.;
    }
    x->input = 0.0;
    // x->input1 = 0.0;
    // x->input2 = 0.0;
    // x->input3 = 0.0;
    // x->input4 = 0.0;
    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }
}

static void *sekere_new(double initial_coeff) {
    (void)initial_coeff;
    unsigned int i;

    t_sekere *x = (t_sekere *)pd_new(sekere_class);

    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object); i < sizeof(t_sekere); i++)
            ((char *)x)[i] = 0;
    }
    outlet_new((t_object *)x, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_coeff"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_dump"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_max"));

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    x->shakeEnergy = 0.0;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
    }
    x->input = 0.0;
    x->sndLevel = 0.0;
    x->gain = 0.0;
    x->soundDecay = 0.0;
    x->systemDecay = 0.0;
    x->shake_damp = x->shake_dampSave = 0.9;
    x->shake_max = x->shake_maxSave = 0.9;
    x->res_freq = x->res_freqSave = 3000.;

    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }

    sekere_setup(x);

    srand(54);

    return (x);
}

void sekere_dsp(t_sekere *x, t_signal **sp, short *count) {
    (void)count;
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    dsp_add(sekere_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
}

t_int *sekere_perform(t_int *w) {
    t_sekere *x = (t_sekere *)(w[1]);
    float num_objects = x->num_objects;
    float shake_damp = x->shake_damp;
    float shake_max = x->shake_max;
    float res_freq = x->res_freq;

    float *out = (float *)(w[2]);
    long n = w[3];
    float lastOutput;
    // long temp2;

    if (num_objects != x->num_objectsSave) {
        if (num_objects < 1.)
            num_objects = 1.;
        x->num_objects = (long)num_objects;
        x->num_objectsSave = (long)num_objects;
        // x->gain = log(num_objects) * 30. / (float)num_objects;
        x->gain = log(num_objects) / log(4.0) * 40.0 / (float)num_objects;
    }

    if (res_freq != x->res_freqSave) {
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
        *out++ = lastOutput;
    }
    return w + 4;
}
