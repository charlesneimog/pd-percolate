/******************************************/
/*  WaveGuide Flute ala Karjalainen,      */
/*  Smith, Waryznyk, etc.                 */
/*  with polynomial Jet ala Cook          */
/*  by Perry Cook, 1995-96                */
/*										  */
/*  ported to MSP by Dan Trueman, 2000	  */
/*                                        */
/*  ported to PD by Olaf Matthes, 2002	  */
/*                                        */
/*  This is a waveguide model, and thus   */
/*  relates to various Stanford Univ.     */
/*  and possibly Yamaha and other patents.*/
/*                                        */
/*   Controls:    CONTROL1 = jetDelay     */
/*                CONTROL2 = noiseGain    */
/*                CONTROL3 = vibFreq      */
/*                MOD_WHEEL= vibAmt       */
/******************************************/

#include "m_pd.h"
#include "stk_c.h"
#include <math.h>

#define LENGTH 882 // 44100/LOWFREQ + 1 --flute length
#define JETLENGTH (LENGTH >> 1)
#define VIBLENGTH 1024

static t_class *flute_class;

typedef struct _flute {
    // header
    t_object x_obj;

    // user controlled vars
    float x_bp; // breath pressure
    float x_jd; // jet delay
    float x_ng; // noise gain
    float x_vf; // vib freq
    float x_va; // vib amount
    float x_fr; // frequency

    float jd_save;
    float fr_save;

    // delay lines
    DLineL boreDelay;
    DLineL jetDelay;

    // one pole filter
    OnePole filter;

    // DC blocker
    DCBlock killdc;

    // vibrato table
    float vibTable[VIBLENGTH];
    float vibRate;
    float vibTime;

    // stuff
    float endRefl;
    float jetRefl;
    float jetRatio;
    float lastFreq;
    float srate, one_over_srate;
} t_flute;

/****FUNCTIONS****/

#define WATCHIT 0.00001

static void setFreq(t_flute *x, float freq) {
    float temp;
    x->lastFreq = freq * 0.66666;
    if (x->lastFreq < WATCHIT)
        x->lastFreq = WATCHIT;
    temp = (x->srate / x->lastFreq) - 2.;
    DLineL_setDelay(&x->boreDelay, temp);
    DLineL_setDelay(&x->jetDelay, temp * x->jetRatio);
}

static void setJetDelay(t_flute *x, float ratio) {
    float temp;
    temp = (x->srate / x->lastFreq) - 2.;
    x->jetRatio = ratio;
    DLineL_setDelay(&x->jetDelay, (temp * ratio));
}

// vib funcs
static void setVibFreq(t_flute *x, float freq) {
    x->vibRate = VIBLENGTH * x->one_over_srate * freq;
}

static float vib_tick(t_flute *x) {
    long temp;
    float temp_time, alpha, output;

    x->vibTime += x->vibRate;
    while (x->vibTime >= (float)VIBLENGTH)
        x->vibTime -= (float)VIBLENGTH;
    while (x->vibTime < 0.)
        x->vibTime += (float)VIBLENGTH;

    temp_time = x->vibTime;

    temp = (long)temp_time;
    alpha = temp_time - (float)temp;
    output = x->vibTable[temp];
    output = output + (alpha * (x->vibTable[temp + 1] - output));
    return output;
}

static t_int *flute_perform(t_int *w) {
    t_flute *x = (t_flute *)(w[1]);

    float bp = x->x_bp;
    float jd = x->x_jd;
    float ng = x->x_ng;
    float vf = x->x_vf;
    float va = x->x_va;
    float fr = x->x_fr;

    float *out = (float *)(w[2]);
    long n = w[3];

    float jr = x->jetRefl;
    float er = x->endRefl;
    float temp, pressureDiff, randPressure;

    if (fr != x->fr_save) {
        setFreq(x, fr);
        x->fr_save = fr;
    }

    x->vibRate = VIBLENGTH * x->one_over_srate * vf;

    if (jd != x->jd_save) {
        setJetDelay(x, jd);
        x->jd_save = jd;
    }

    while (n--) {

        randPressure = ng * Noise_tick();
        randPressure += va * vib_tick(x);
        randPressure *= bp;

        temp = OnePole_tick(&x->filter, x->boreDelay.lastOutput);
        temp = DCBlock_tick(&x->killdc, temp);
        pressureDiff = bp + randPressure - (jr * temp);
        pressureDiff = DLineL_tick(&x->jetDelay, pressureDiff);
        pressureDiff = JetTabl_lookup(pressureDiff) + (er * temp);

        *out++ = DLineL_tick(&x->boreDelay, pressureDiff);
    }
    return w + 4;
}

static void flute_dsp(t_flute *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    OnePole_setPole(&x->filter, 0.7 - (0.1 * 22050. / x->srate));
    dsp_add(flute_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void flute_float(t_flute *x, t_floatarg f) { x->x_bp = f; }

static void flute_jd(t_flute *x, t_floatarg f) { x->x_jd = f; }

static void flute_ng(t_flute *x, t_floatarg f) { x->x_ng = f; }

static void flute_vf(t_flute *x, t_floatarg f) { x->x_vf = f; }

static void flute_va(t_flute *x, t_floatarg f) { x->x_va = f; }

static void flute_freq(t_flute *x, t_floatarg f) { x->x_fr = f; }

static void flute_free(t_flute *x) {
    DLineL_free(&x->boreDelay);
    DLineL_free(&x->jetDelay);
}

static void *flute_new(void) {
    unsigned int i;

    t_flute *x = (t_flute *)pd_new(flute_class);
    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object) - 1; i < sizeof(t_flute); i++)
            ((char *)x)[i] = 0;
    }
    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("jd"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ng"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("vf"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("va"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("freq"));

    x->x_bp = 0.5;
    x->x_ng = 0.15;
    x->x_vf = 0.5;
    x->x_va = 0.05;
    x->x_fr = 440.;
    x->lastFreq = 440.;

    DLineL_alloc(&x->boreDelay, LENGTH);
    DLineL_alloc(&x->jetDelay, JETLENGTH);

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    for (i = 0; i < VIBLENGTH; i++)
        x->vibTable[i] = sin(i * TWO_PI / VIBLENGTH);
    x->vibRate = 1.;
    x->vibTime = 0.;

    // clear stuff
    DLineL_clear(&x->boreDelay);
    DLineL_clear(&x->jetDelay);
    OnePole_init(&x->filter);

    // initialize things
    x->endRefl = 0.5;
    x->jetRefl = 0.5;
    x->jetRatio = 0.32;
    DLineL_setDelay(&x->boreDelay, 100.);
    DLineL_setDelay(&x->jetDelay, 49.);

    OnePole_setPole(&x->filter, 0.7 - (0.1 * 22050. / x->srate));
    OnePole_setGain(&x->filter, -1.);

    setFreq(x, x->x_fr);
    setVibFreq(x, 5.925);
    setJetDelay(x, x->jetRatio);

    x->jd_save = x->jetRatio;
    x->fr_save = x->x_fr;

    post("you can play the flute too");

    return (x);
}

void flute_tilde_setup(void) {
    flute_class = class_new(gensym("flute~"), (t_newmethod)flute_new,
                            (t_method)flute_free, sizeof(t_flute), 0, 0);
    class_addmethod(flute_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(flute_class, (t_method)flute_dsp, gensym("dsp"), A_NULL);
    class_addfloat(flute_class, (t_method)flute_float);
    // 	class_addbang(flute_class, (t_method)flute_bang);
    class_addmethod(flute_class, (t_method)flute_jd, gensym("jd"), A_FLOAT,
                    A_NULL);
    class_addmethod(flute_class, (t_method)flute_ng, gensym("ng"), A_FLOAT,
                    A_NULL);
    class_addmethod(flute_class, (t_method)flute_vf, gensym("vf"), A_FLOAT,
                    A_NULL);
    class_addmethod(flute_class, (t_method)flute_va, gensym("va"), A_FLOAT,
                    A_NULL);
    class_addmethod(flute_class, (t_method)flute_freq, gensym("freq"), A_FLOAT,
                    A_NULL);
}
