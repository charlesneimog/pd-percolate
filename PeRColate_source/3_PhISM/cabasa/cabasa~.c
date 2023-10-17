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

#define CABA_SOUND_DECAY 0.95
#define CABA_SYSTEM_DECAY 0.997
#define CABA_NUM_BEADS 512

static t_class *cabasa_class;

typedef struct _cabasa {
    // header
    t_object x_obj;

    // user controlled vars
    float shakeEnergy;
    float input, output[2];
    float coeffs[2];
    float sndLevel;
    float gain; //, gain1, gain2;
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

    // signals connected? or controls...
    short num_objectsConnected;
    short res_freqConnected;
    short shake_dampConnected;
    short shake_maxConnected;

    float srate, one_over_srate;
} t_cabasa;

/****FUNCTIONS****/

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

static void cabasa_setup(t_cabasa *x) {

    x->num_objects = x->num_objectsSave = CABA_NUM_BEADS;
    x->gain = log(x->num_objects) / log(4.0) * 120.0 / (float)x->num_objects;
    x->coeffs[0] = -0.7 * 2.0 * cos(3000.0 * TWO_PI / x->srate);
    x->coeffs[1] = 0.7 * 0.7;
    x->soundDecay = CABA_SOUND_DECAY;
    x->systemDecay = CABA_SYSTEM_DECAY;
    x->shake_damp = x->shake_dampSave = 0.9;
    x->shake_max = x->shake_maxSave = 0.9;
    x->res_freq = x->res_freqSave = 2000.;
}

static float cabasa_tick(t_cabasa *x) {
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
    data = x->output[0] - x->output[1];

    return data;
}

// primary MSP funcs
static t_int *cabasa_perform(t_int *w) {
    t_cabasa *x = (t_cabasa *)(w[1]);

    float num_objects = x->num_objects;
    float shake_damp = x->shake_damp;
    float shake_max = x->shake_max;
    float res_freq = x->res_freq;

    t_float *out = (t_float *)(w[2]);
    long n = w[3];

    float lastOutput;
    // long temp2;

    if (num_objects != x->num_objectsSave) {
        if (num_objects < 1.)
            num_objects = 1.;
        x->num_objects = (long)num_objects;
        x->num_objectsSave = (long)num_objects;
        x->gain = log(num_objects) / log(4.0) * 120.0 / (float)num_objects;
    }

    if (res_freq != x->res_freqSave) {
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
        *out++ = lastOutput;
    }
    return w + 4;
}

static void cabasa_dsp(t_cabasa *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    dsp_add(cabasa_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void cabasa_float(t_cabasa *x, t_floatarg f) { x->num_objects = f; }

static void cabasa_res_freq(t_cabasa *x, t_floatarg f) { x->res_freq = f; }

static void cabasa_shake_damp(t_cabasa *x, t_floatarg f) { x->shake_damp = f; }

static void cabasa_shake_max(t_cabasa *x, t_floatarg f) { x->shake_max = f; }

void cabasa_bang(t_cabasa *x) {
    int i;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
    }
    x->input = 0.0;
}

void *cabasa_new(double initial_coeff) {
    (void)initial_coeff;
    unsigned int i;

    // user controlled vars

    t_cabasa *x = (t_cabasa *)pd_new(cabasa_class);
    if (x != NULL) {
        for (i = sizeof(t_object); i < sizeof(t_cabasa); i++) {
            ((char *)x)[i] = 0;
        }
    } else {
        perror("Not enough memory for cabasa object");
        return NULL;
    }
    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_damp"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_max"));

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    x->shakeEnergy = 0.0;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
    }
    x->input = 0.0;
    x->sndLevel = 0.0;

    cabasa_setup(x);

    srand(54);

    return (x);
}

void cabasa_tilde_setup(void) {
    cabasa_class = class_new(gensym("cabasa~"), (t_newmethod)cabasa_new, 0,
                             sizeof(t_cabasa), 0, 0);
    class_addmethod(cabasa_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(cabasa_class, (t_method)cabasa_dsp, gensym("dsp"), A_NULL);
    class_addfloat(cabasa_class, (t_method)cabasa_float);
    class_addbang(cabasa_class, (t_method)cabasa_bang);
    class_addmethod(cabasa_class, (t_method)cabasa_res_freq, gensym("res_freq"),
                    A_FLOAT, A_NULL);
    class_addmethod(cabasa_class, (t_method)cabasa_shake_damp,
                    gensym("shake_damp"), A_FLOAT, A_NULL);
    class_addmethod(cabasa_class, (t_method)cabasa_shake_max,
                    gensym("shake_max"), A_FLOAT, A_NULL);
}
