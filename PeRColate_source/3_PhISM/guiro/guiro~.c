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

#define GUIR_SOUND_DECAY 0.95
#define GUIR_NUM_RATCHETS 128
#define GUIR_GOURD_FREQ 2500.0
#define GUIR_GOURD_RESON 0.97
#define GUIR_GOURD_FREQ2 4000.0
#define GUIR_GOURD_RESON2 0.97

static t_class *guiro_class;

typedef struct _guiro {
    // header
    t_object x_obj;

    // user controlled vars

    float guiroScrape;
    float shakeEnergy;
    float input, output[2];
    float coeffs[2];
    float input2, output2[2];
    float coeffs2[2];
    float sndLevel;
    float gain;
    float soundDecay;
    float systemDecay;

    long num_objects;
    float shake_damp;
    float shake_max;
    float res_freq, res_freq2;

    long num_objectsSave; // number of beans	//resonance
    float shake_dampSave; // damping
    float shake_maxSave;
    float res_freqSave, res_freq2Save;

    float collLikely, scrapeVel;
    float totalEnergy;
    float ratchet, ratchetDelta;
    float finalZ[3];

    float srate, one_over_srate;
} t_guiro;

/****FUNCTIONS****/
int my_random(int max) { //  Return Random Int Between 0 and max
    unsigned long temp;
    temp = (unsigned long)rand();
    temp *= (unsigned long)max;
    temp >>= 15;
    return (int)temp;
}

// noise maker
float noise_tick(void) {
    float output;
    output = (float)rand() - 16384.;
    output *= ONE_OVER_RANDLIMIT;
    return output;
}

void guiro_setup(t_guiro *x) {

    x->num_objects = x->num_objectsSave = GUIR_NUM_RATCHETS;
    x->soundDecay = GUIR_SOUND_DECAY;
    x->coeffs[0] =
        -GUIR_GOURD_RESON * 2.0 * cos(GUIR_GOURD_FREQ * TWO_PI / x->srate);
    x->coeffs[1] = GUIR_GOURD_RESON * GUIR_GOURD_RESON;
    x->coeffs2[0] =
        -GUIR_GOURD_RESON2 * 2.0 * cos(GUIR_GOURD_FREQ2 * TWO_PI / x->srate);
    x->coeffs2[1] = GUIR_GOURD_RESON2 * GUIR_GOURD_RESON2;
    x->ratchet = x->ratchetDelta = 0.;
    x->guiroScrape = 0.;
}

float guiro_tick(t_guiro *x) {
    float data;
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
    x->input2 -= x->output2[0] * x->coeffs2[0];
    x->input2 -= x->output2[1] * x->coeffs2[1];
    x->output2[1] = x->output2[0];
    x->output2[0] = x->input2;

    x->finalZ[2] = x->finalZ[1];
    x->finalZ[1] = x->finalZ[0];
    x->finalZ[0] = x->output[1] + x->output2[1];
    data = x->finalZ[0] - x->finalZ[2];

    return data;
}

// primary MSP funcs
static t_int *guiro_perform(t_int *w) {
    t_guiro *x = (t_guiro *)(w[1]);

    float num_objects = x->num_objects;
    float shake_damp = x->shake_damp;
    float shake_max = x->shake_max;
    float res_freq = x->res_freq;
    float res_freq2 = x->res_freq2;

    t_float *out = (t_float *)(w[2]);
    long n = w[3];

    t_float lastOutput, temp;
    long temp2;

    if (num_objects != x->num_objectsSave) {
        if (num_objects < 1.)
            num_objects = 1.;
        x->num_objects = (long)num_objects;
        x->num_objectsSave = (long)num_objects;
        x->gain = log(num_objects) * 30.0 / (float)num_objects;
    }

    if (res_freq != x->res_freqSave) {
        x->res_freqSave = x->res_freq = res_freq;
        x->coeffs[0] =
            -GUIR_GOURD_RESON * 2.0 * cos(res_freq * TWO_PI / x->srate);
    }

    if (shake_damp != x->shake_dampSave) {
        x->shake_dampSave = x->shake_damp = shake_damp;
        // x->systemDecay = .998 + (shake_damp * .002);
        // x->ratchetDelta = shake_damp;
        x->scrapeVel = shake_damp;
    }

    if (shake_max != x->shake_maxSave) {
        x->shake_maxSave = x->shake_max = shake_max;
        // x->shakeEnergy += shake_max * MAX_SHAKE * 0.1;
        // if (x->shakeEnergy > MAX_SHAKE) x->shakeEnergy = MAX_SHAKE;
        x->guiroScrape = shake_max;
    }

    if (res_freq2 != x->res_freq2Save) {
        x->res_freq2Save = x->res_freq2 = res_freq2;
        x->coeffs2[0] =
            -GUIR_GOURD_RESON2 * 2.0 * cos(res_freq2 * TWO_PI / x->srate);
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
    return w + 4;
}

static void guiro_dsp(t_guiro *x, t_signal **sp) {
    dsp_add(guiro_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void guiro_float(t_guiro *x, t_floatarg f) { x->num_objects = f; }

static void guiro_res_freq(t_guiro *x, t_floatarg f) { x->res_freq = f; }

static void guiro_shake_damp(t_guiro *x, t_floatarg f) { x->shake_damp = f; }

static void guiro_shake_max(t_guiro *x, t_floatarg f) { x->shake_max = f; }

static void guiro_res_freq2(t_guiro *x, t_floatarg f) { x->res_freq2 = f; }

static void guiro_bang(t_guiro *x) { x->guiroScrape = 0.; }

static void *guiro_new(void) {
    unsigned int i;
    t_guiro *x = (t_guiro *)pd_new(guiro_class);
    if (x != NULL) {
        for (i = sizeof(t_object); i < sizeof(t_guiro); i++)
            ((char *)x)[i] = 0;
    } else {
        perror("guiro~");
        return NULL;
    }
    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_damp"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("shake_max"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("res_freq2"));

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    x->guiroScrape = 0;
    x->shakeEnergy = 0.0;
    for (i = 0; i < 2; i++) {
        x->output[i] = 0.;
        x->output2[i] = 0.;
    }
    x->shake_damp = x->shake_dampSave = 0.9;
    x->shake_max = x->shake_maxSave = 0.9;
    x->totalEnergy = 0.9;
    x->input = 0.0;
    x->input2 = 0.0;
    x->sndLevel = 0.0;
    x->gain = 0.0;
    x->soundDecay = 0.0;
    x->systemDecay = 0.0;
    x->res_freq = x->res_freqSave = 2000.;
    x->res_freq2 = x->res_freq2Save = 3500.;
    x->collLikely = 0.;
    x->scrapeVel = 0.00015;
    x->ratchet = 0.0;
    x->ratchetDelta = 0.0005;
    for (i = 0; i < 3; i++) {
        x->finalZ[i] = 0.;
    }

    guiro_setup(x);

    srand(54);

    return (x);
}

void guiro_tilde_setup(void) {
    guiro_class = class_new(gensym("guiro~"), (t_newmethod)guiro_new, 0,
                            sizeof(t_guiro), 0, 0);
    class_addmethod(guiro_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(guiro_class, (t_method)guiro_dsp, gensym("dsp"), A_NULL);
    class_addfloat(guiro_class, (t_method)guiro_float);
    class_addbang(guiro_class, (t_method)guiro_bang);
    class_addmethod(guiro_class, (t_method)guiro_res_freq, gensym("res_freq"),
                    A_FLOAT, A_NULL);
    class_addmethod(guiro_class, (t_method)guiro_shake_damp,
                    gensym("shake_damp"), A_FLOAT, A_NULL);
    class_addmethod(guiro_class, (t_method)guiro_shake_max, gensym("shake_max"),
                    A_FLOAT, A_NULL);
    class_addmethod(guiro_class, (t_method)guiro_res_freq2, gensym("res_freq2"),
                    A_FLOAT, A_NULL);
}
