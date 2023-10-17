/********************************************/
/*  Marimba SubClass of Modal4 Instrument,  */
/*  by Perry R. Cook, 1995-96               */
/*										  	*/
/*  ported to MSP by Dan Trueman, 2000	  	*/
/*                                          */
/*  ported to PD by Olaf Matthes, 2002	  	*/
/*                                          */
/*   Controls:    CONTROL1 = stickHardness  */
/*                CONTROL2 = strikePosition */
/*		  		  CONTROL3 = vibFreq       	*/
/*		  		  MOD_WHEEL= vibAmt        	*/
/********************************************/

#include "m_pd.h"
#include "marmstk1.h"
#include "stk_c.h"
#include <math.h>

static t_class *marimba_class;

typedef struct _marimba {
    // header
    t_object x_obj;

    // user controlled vars
    float x_sh;   // stick hardness
    float x_spos; // stick position
    float x_sa;   // amplitude
    float x_vf;   // vib freq
    float x_va;   // vib amount
    float x_fr;   // frequency

    float fr_save, sh_save, spos_save, sa_save;

    Modal4 modal;

    // signals connected? or controls...
    short x_shconnected;
    short x_sposconnected;
    short x_saconnected;
    short x_vfconnected;
    short x_vaconnected;
    short x_frconnected;

    // stuff
    int multiStrike;
    float srate, one_over_srate;
} t_marimba;

/****FUNCTIONS****/
static void Marimba_setStickHardness(t_marimba *x, float hardness) {
    x->x_sh = hardness;
    HeaderSnd_setRate(&x->modal.wave, (0.25 * pow(4.0, x->x_sh)));
    x->modal.masterGain = 0.1 + (1.8 * x->x_sh);
}

static void Marimba_setStrikePosition(t_marimba *x, float position) {
    float temp, temp2;
    temp2 = position * M_PI;
    x->x_spos = position; /*  Hack only first three modes */
    temp = sin(temp2);
    Modal4_setFiltGain(&x->modal, 0,
                       .12 * temp); /*  1st mode function of pos.   */
    temp = sin(0.05 + (3.9 * temp2));
    Modal4_setFiltGain(&x->modal, 1,
                       -.03 * temp); /*  2nd mode function of pos.   */
    temp = sin(-0.05 + (11. * temp2));
    Modal4_setFiltGain(&x->modal, 2,
                       .11 * temp); /*  3rd mode function of pos.   */
}

static void Marimba_strike(t_marimba *x, float amplitude) {
    int temp;
    temp = rand() >> 10;
    if (temp < 2) {
        x->multiStrike = 1;
    } else if (temp < 1) {
        x->multiStrike = 2;
    } else
        x->multiStrike = 0;
    Modal4_strike(&x->modal, amplitude);
}

static t_int *marimba_perform(t_int *w) {
    t_marimba *x = (t_marimba *)(w[1]);

    float sh = x->x_sh;
    float spos = x->x_spos;
    float sa = x->x_sa;
    float vf = x->x_vf;
    float va = x->x_va;
    float fr = x->x_fr;

    t_float *out = (t_float *)(w[2]);
    long n = w[3];

    if (fr != x->fr_save) {
        Modal4_setFreq(&x->modal, fr);
        x->fr_save = fr;
    }

    if (sh != x->sh_save) {
        Marimba_setStickHardness(x, sh);
        x->sh_save = sh;
    }

    if (spos != x->spos_save) {
        Marimba_setStrikePosition(x, spos);
        x->spos_save = spos;
    }

    if (sa != x->sa_save) {
        Marimba_strike(x, sa);
        x->sa_save = sa;
    }

    HeaderSnd_setFreq(&x->modal.vibr, vf, x->srate);
    x->modal.vibrGain = va;

    while (n--) {

        if (x->multiStrike > 0) {
            if (x->modal.wave.finished) {
                HeaderSnd_reset(&x->modal.wave);
                x->multiStrike -= 1;
            }
        }

        *out++ = Modal4_tick(&x->modal);
    }
    return w + 4;
}

static void marimba_dsp(t_marimba *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    dsp_add(marimba_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

/* ----- handle data from inlets ------ */
void marimba_noteon(t_marimba *x) {
    Modal4_noteOn(&x->modal, x->x_fr, x->x_sa);
}

void marimba_noteoff(t_marimba *x) { Modal4_noteOff(&x->modal, x->x_sa); }

static void marimba_float(t_marimba *x, t_floatarg f) { x->x_sh = f; }

static void marimba_spos(t_marimba *x, t_floatarg f) { x->x_spos = f; }

static void marimba_sa(t_marimba *x, t_floatarg f) { x->x_sa = f; }

static void marimba_vf(t_marimba *x, t_floatarg f) { x->x_vf = f; }

static void marimba_va(t_marimba *x, t_floatarg f) { x->x_va = f; }

static void marimba_freq(t_marimba *x, t_floatarg f) { x->x_fr = f; }

static void marimba_free(t_marimba *x) {
    HeaderSnd_free(&x->modal.wave);
    HeaderSnd_free(&x->modal.vibr);
}

static void *marimba_new(void) {
    unsigned int i;
    char file[128];

    t_marimba *x = (t_marimba *)pd_new(marimba_class);
    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object); i < sizeof(t_marimba); i++)
            ((char *)x)[i] = 0;
    }
    outlet_new(&x->x_obj, gensym("signal"));

    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("spos"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("sa"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("vf"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("va"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("freq"));

    x->srate = sys_getsr();
    x->one_over_srate = 1. / x->srate;

    Modal4_init(&x->modal, x->srate);
    // strcpy(file, RAWWAVE_PATH);
    HeaderSnd_alloc(&x->modal.wave, marmstk1, 256, "oneshot");
    HeaderSnd_setRate(&x->modal.wave, .5); /*  normal stick  */

    Modal4_setRatioAndReson(&x->modal, 0, 1.00, 0.9996);
    Modal4_setRatioAndReson(&x->modal, 1, 3.99, 0.9994);
    Modal4_setRatioAndReson(&x->modal, 2, 10.65, 0.9994);
    Modal4_setRatioAndReson(&x->modal, 3, 2443.0, 0.999);
    Modal4_setFiltGain(&x->modal, 0, .04);
    Modal4_setFiltGain(&x->modal, 1, .01);
    Modal4_setFiltGain(&x->modal, 2, .01);
    Modal4_setFiltGain(&x->modal, 3, .008);
    x->modal.directGain = 0.1;
    x->multiStrike = 0;

    x->fr_save = x->x_fr;

    post("marimba...");

    return (x);
}

void marimba_tilde_setup(void) {
    marimba_class = class_new(gensym("marimba~"), (t_newmethod)marimba_new,
                              (t_method)marimba_free, sizeof(t_marimba), 0, 0);
    class_addmethod(marimba_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_dsp, gensym("dsp"),
                    A_NULL);
    class_addfloat(marimba_class, (t_method)marimba_float);
    class_addmethod(marimba_class, (t_method)marimba_spos, gensym("spos"),
                    A_FLOAT, A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_sa, gensym("sa"), A_FLOAT,
                    A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_vf, gensym("vf"), A_FLOAT,
                    A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_va, gensym("va"), A_FLOAT,
                    A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_freq, gensym("freq"),
                    A_FLOAT, A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_noteon, gensym("noteon"),
                    A_NULL);
    class_addmethod(marimba_class, (t_method)marimba_noteoff, gensym("noteoff"),
                    A_NULL);
}
