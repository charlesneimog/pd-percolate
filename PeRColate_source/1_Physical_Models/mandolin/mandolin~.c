/********************************************/
/*  Commuted Mandolin Subclass of enhanced  */
/*  dual plucked-string model               */
/*  by Perry Cook, 1995-96                  */
/*   Controls:    CONTROL1 = bodySize       */
/*                CONTROL2 = pluckPosition  */
/*                CONTROL3 = loopGain       */
/*                MOD_WHEEL= deTuning       */
/*										  	*/
/*  ported to MSP by Dan Trueman, 2000	  	*/
/*                                          */
/*  ported to PD by Olaf Matthes, 2002	  	*/
/*                                          */
/*  Note: Commuted Synthesis, as with many  */
/*  other WaveGuide techniques, is covered  */
/*  by patents, granted, pending, and/or    */
/*  applied-for.  All are assigned to the   */
/*  Board of Trustees, Stanford University. */
/*  For information, contact the Office of  */
/*  Technology Licensing, Stanford U.       */
/********************************************/

#include "m_pd.h"
#include "mandimpulses.h"
#include "stk_c.h"

#define LENGTH 2048 // 44100/LOWFREQ + 1 --mando length

static t_class *mando_class;

typedef struct _mando {
    // header
    t_object x_obj;

    // user controlled vars
    float pluckAmp;      // bow pressure
    float pluckPos;      // that's right
    float stringDamping; // yep...
    float detuning;      // tone deafness
    float bodySize;      // big mandolin
    float x_fr;          // frequency

    int mic; // directional position (NBody)

    float fr_save, detuning_save;

    // signals connected? or controls...
    short pluckAmpconnected;
    short pluckPosconnected;
    short stringDampingconnected;
    short detuningconnected;
    short bodySizeconnected;
    short x_frconnected;

    // delay lines0
    DLineA delayLine;
    DLineA delayLine2;
    DLineL combDelay;

    // impulse response files
    HeaderSnd soundfile[12];

    // filters
    OneZero filter;
    OneZero filter2;

    // stuff
    long length;
    float loopGain;
    float baseLoopGain;
    float lastFreq;
    float lastLength;
    short pluck;
    long dampTime;
    int waveDone;
    float directBody;

    float srate, one_over_srate;
} t_mando;

/****FUNCTIONS****/

void pluck(t_mando *x, float amplitude,
           float position) { /* this function gets interesting here, */
    /* because pluck may be longer than     */
    /* string length, so we just reset the  */
    /* soundfile and add in the pluck in    */
    /* the tick method.                     */
    x->pluckPos = position; /* pluck position is zeroes at pos*length  */
    HeaderSnd_reset(&x->soundfile[x->mic]);
    x->pluckAmp = amplitude;
    /* Set Pick Position which puts zeroes at pos*length  */
    DLineL_setDelay(&x->combDelay, 0.5 * x->pluckPos * x->lastLength);
    x->dampTime = (long)x->lastLength; /* See tick method below */
    x->waveDone = 0;
}

void setBodySize(t_mando *x, float size) {
    int i;
    for (i = 0; i < 12; i++) {
        HeaderSnd_setRate(&x->soundfile[i], size);
    }
}

void mando_bang(t_mando *x) { x->pluck = 1; }

void setFreq(t_mando *x, float frequency) {
    if (frequency < 20.)
        frequency = 20.;
    x->lastFreq = frequency;
    x->lastLength = x->srate / x->lastFreq; /* length - delays */
    if (x->detuning != 0.) {
        DLineA_setDelay(&x->delayLine, (x->lastLength / x->detuning) - .5);
        DLineA_setDelay(&x->delayLine2, (x->lastLength * x->detuning) - .5);
    }
    x->loopGain = x->baseLoopGain + (frequency * 0.000005);
    if (x->loopGain > 1.0)
        x->loopGain = 0.99999;
}

void setDetune(t_mando *x, float detune) {
    if (detune != 0.) {
        x->detuning = detune;
        DLineA_setDelay(&x->delayLine, (x->lastLength / x->detuning) - .5);
        DLineA_setDelay(&x->delayLine2, (x->lastLength * x->detuning) - .5);
    }
}

void setBaseLoopGain(t_mando *x, float aGain) {
    x->baseLoopGain = aGain;
    x->loopGain = x->baseLoopGain + (x->lastFreq * 0.000005);
    if (x->loopGain > 1.0)
        x->loopGain = 0.99999;
}

static t_int *mando_perform(t_int *w) {
    t_mando *x = (t_mando *)(w[1]);

    float pluckAmp = x->pluckAmp;
    float pluckPos = x->pluckPos;
    float stringDamping = x->stringDamping;
    float detuning = x->detuning;
    float bodySize = x->bodySize;
    float fr = x->x_fr;
    t_float *out = (float *)(w[2]);
    long n = w[3];

    float temp, lastOutput;

    if (fr != x->fr_save) {
        setFreq(x, fr);
        x->fr_save = fr;
    }

    setBodySize(x, bodySize);
    setBaseLoopGain(x, stringDamping);

    if (detuning != x->detuning_save) {
        setDetune(x, detuning);
        x->detuning_save = detuning;
    }

    if (x->pluck) {
        pluck(x, pluckAmp, pluckPos);
        x->pluck = 0;
    }

    while (n--) {
        /* //this is busted, for some mysterious reason...
        if (!x->waveDone)      {
            x->waveDone = HeaderSnd_informTick(&x->soundfile[x->mic]);
            temp = x->soundfile[x->mic].lastOutput * pluckAmp;
            temp = temp - DLineL_tick(&x->combDelay, temp);
        }
        */

        x->waveDone = HeaderSnd_informTick(&x->soundfile[x->mic]);
        temp = x->soundfile[x->mic].lastOutput * pluckAmp;
        temp = temp - DLineL_tick(&x->combDelay, temp);

        if (x->dampTime >= 0) { /* Damping hack to help avoid */
            x->dampTime -= 1;   /* overflow on replucking     */
            lastOutput = DLineA_tick(
                &x->delayLine,                  /* Calculate 1st delay */
                OneZero_tick(&x->filter, temp + /* filterered reflection      */
                                             (x->delayLine.lastOutput *
                                              .7))); /* plus pluck excitation */
            lastOutput += DLineA_tick(
                &x->delayLine2, /* and 2nd delay              */
                OneZero_tick(&x->filter2, temp + /* just like the 1st */
                                              (x->delayLine2.lastOutput * .7)));
        } else { /*  No damping hack after 1 period */
            lastOutput = DLineA_tick(
                &x->delayLine, /* Calculate 1st delay */
                OneZero_tick(&x->filter,
                             temp + /* filterered reflection      */
                                 (x->delayLine.lastOutput *
                                  x->loopGain))); /* plus pluck excitation */
            lastOutput += DLineA_tick(
                &x->delayLine2, /* and 2nd delay              */
                OneZero_tick(&x->filter2,
                             temp + /* just like the 1st          */
                                 (x->delayLine2.lastOutput * x->loopGain)));
        }

        *out++ = lastOutput;
    }
    return w + 4;
}

static void mando_dsp(t_mando *x, t_signal **sp) {
    /* 	x->pluckAmpconnected 		= count[0];
     x->pluckPosconnected 		= count[1];
     x->stringDampingconnected 	= count[2];
     x->detuningconnected 		= count[3];
     x->bodySizeconnected 		= count[4];
     x->x_frconnected 			= count[5]; */

    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    dsp_add(mando_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void setmic(t_mando *x, t_floatarg argc) {
    x->mic = (int)(argc) % 12;
    post("mando: setting mic: %d", x->mic);
}

static void mando_float(t_mando *x, t_floatarg f) { x->pluckAmp = f; }

static void mando_pluckPos(t_mando *x, t_floatarg f) { x->pluckPos = f; }

static void mando_stringDamping(t_mando *x, t_floatarg f) {
    x->stringDamping = f;
}

static void mando_detuning(t_mando *x, t_floatarg f) { x->detuning = f; }

static void mando_bodySize(t_mando *x, t_floatarg f) { x->bodySize = f; }

static void mando_freq(t_mando *x, t_floatarg f) { x->x_fr = f; }

static void mando_free(t_mando *x) {
    int i;
    DLineA_free(&x->delayLine);
    DLineA_free(&x->delayLine2);
    DLineL_free(&x->combDelay);
    for (i = 0; i < 12; i++)
        HeaderSnd_free(&x->soundfile[i]);
}

static void *mando_new(void) {
    unsigned int i;
    char temp[128];

    t_mando *x = (t_mando *)pd_new(mando_class);
    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object) - 1; i < sizeof(t_mando); i++)
            ((char *)x)[i] = 0;
    }
    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("pluckPos"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("stringDamping"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("detuning"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("bodySize"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("freq"));

    x->length = LENGTH;
    x->baseLoopGain = 0.995;
    x->loopGain = 0.999;
    x->directBody = 1.0;
    x->mic = 0;
    x->dampTime = 0;
    x->waveDone = 1;
    x->pluckAmp = 0.3;
    x->pluckPos = 0.4;
    x->detuning = 0.995;
    x->lastFreq = 80.;
    x->lastLength = x->length * 0.5;
    x->x_fr = 440.;
    x->pluck = 0;

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    DLineA_alloc(&x->delayLine, LENGTH);
    DLineA_alloc(&x->delayLine2, LENGTH);
    DLineL_alloc(&x->combDelay, LENGTH);

    // clear stuff
    DLineA_clear(&x->delayLine);
    DLineA_clear(&x->delayLine2);
    DLineL_clear(&x->combDelay);
    OneZero_init(&x->filter);
    OneZero_init(&x->filter2);

    // impulse responses
    for (i = 0; i < 12; i++) {
        HeaderSnd_alloc(&x->soundfile[i], &mand[i][0], 721, "oneshot");
    }

    setFreq(x, x->x_fr);

    x->fr_save = x->x_fr;
    x->detuning_save = x->detuning;

    post("if you believe the mandolin is linear, i've got another one for "
         "ya...");

    return (x);
}

void mandolin_tilde_setup(void) {
    mando_class = class_new(gensym("mandolin~"), (t_newmethod)mando_new,
                            (t_method)mando_free, sizeof(t_mando), 0, 0);
    class_addmethod(mando_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(mando_class, (t_method)mando_dsp, gensym("dsp"), A_NULL);
    class_addfloat(mando_class, (t_method)mando_float);
    class_addbang(mando_class, (t_method)mando_bang);
    class_addmethod(mando_class, (t_method)mando_pluckPos, gensym("pluckPos"),
                    A_FLOAT, A_NULL);
    class_addmethod(mando_class, (t_method)mando_stringDamping,
                    gensym("stringDamping"), A_FLOAT, A_NULL);
    class_addmethod(mando_class, (t_method)mando_detuning, gensym("detuning"),
                    A_FLOAT, A_NULL);
    class_addmethod(mando_class, (t_method)mando_bodySize, gensym("bodySize"),
                    A_FLOAT, A_NULL);
    class_addmethod(mando_class, (t_method)mando_freq, gensym("freq"), A_FLOAT,
                    A_NULL);
    class_addmethod(mando_class, (t_method)setmic, gensym("setmic"), A_FLOAT,
                    0);
}
