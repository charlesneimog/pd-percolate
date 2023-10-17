/********************************************/
/*  Agogo SubClass of Modal4 Instrument		*/
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

#include "britestk.h"
#include "m_pd.h"
#include "stk_c.h"
#include <math.h>

static t_class *agogo_class;

typedef struct _agogo {
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
} t_agogo;

/****FUNCTIONS****/
static void Agogo_setStickHardness(t_agogo *x, float hardness) {
    x->x_sh = hardness;
    HeaderSnd_setRate(&x->modal.wave, 3. + 8. * hardness);
    x->modal.masterGain = 1.;
}

static void Agogo_setStrikePosition(t_agogo *x, float position) {
    float temp, temp2;
    temp2 = position * M_PI;
    x->x_spos = position; /*  Hack only first three modes */
    temp = sin(0.7 * temp2);
    Modal4_setFiltGain(&x->modal, 0,
                       .08 * temp); /*  1st mode function of pos.   */
    temp = sin(.1 + 5. * temp2);
    Modal4_setFiltGain(&x->modal, 1,
                       .07 * temp); /*  2nd mode function of pos.   */
    temp = sin(.2 + 7. * temp2);
    Modal4_setFiltGain(&x->modal, 2,
                       .04 * temp); /*  3rd mode function of pos.   */
}

static t_int *agogo_perform(t_int *w) {
    t_agogo *x = (t_agogo *)(w[1]);

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
        Agogo_setStickHardness(x, sh);
        x->sh_save = sh;
    }

    if (spos != x->spos_save) {
        Agogo_setStrikePosition(x, spos);
        x->spos_save = spos;
    }

    if (sa != x->sa_save) {
        Modal4_strike(&x->modal, sa);
        x->sa_save = sa;
    }

    HeaderSnd_setFreq(&x->modal.vibr, vf, x->srate);
    x->modal.vibrGain = va;

    while (n--) {

        *out++ = Modal4_tick(&x->modal);
    }
    return w + 4;
}

static void agogo_dsp(t_agogo *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    dsp_add(agogo_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

/* ----- handle data from inlets ------ */
void agogo_noteon(t_agogo *x) { Modal4_noteOn(&x->modal, x->x_fr, x->x_sa); }

void agogo_noteoff(t_agogo *x) { Modal4_noteOff(&x->modal, x->x_sa); }

static void agogo_float(t_agogo *x, t_floatarg f) { x->x_sh = f; }

static void agogo_spos(t_agogo *x, t_floatarg f) { x->x_spos = f; }

static void agogo_sa(t_agogo *x, t_floatarg f) { x->x_sa = f; }

static void agogo_vf(t_agogo *x, t_floatarg f) { x->x_vf = f; }

static void agogo_va(t_agogo *x, t_floatarg f) { x->x_va = f; }

static void agogo_freq(t_agogo *x, t_floatarg f) { x->x_fr = f; }

static void agogo_free(t_agogo *x) {
    HeaderSnd_free(&x->modal.wave);
    HeaderSnd_free(&x->modal.vibr);
}

static void *agogo_new(void) {
    unsigned int i;
    char file[128];

    t_agogo *x = (t_agogo *)pd_new(agogo_class);
    // zero out the struct, to be careful (takk to jkclayton)
    if (x != NULL) {
        for (i = sizeof(t_object); i < sizeof(t_agogo); i++)
            ((char *)x)[i] = 0;
    } else {
        return NULL;
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
    strcpy(file, RAWWAVE_PATH);
    HeaderSnd_alloc(&x->modal.wave, britestk, 2048, "oneshot");
    HeaderSnd_setRate(&x->modal.wave, 7.); /*  normal stick  */

    Modal4_setRatioAndReson(&x->modal, 0, 1.00, 0.999);
    Modal4_setRatioAndReson(&x->modal, 1, 4.08, 0.999);
    Modal4_setRatioAndReson(&x->modal, 2, 6.669, 0.999);
    Modal4_setRatioAndReson(&x->modal, 3, -3725., 0.999);
    Modal4_setFiltGain(&x->modal, 0, .06);
    Modal4_setFiltGain(&x->modal, 1, .05);
    Modal4_setFiltGain(&x->modal, 2, .03);
    Modal4_setFiltGain(&x->modal, 3, .02);
    x->modal.directGain = 0.25;
    x->multiStrike = 0;
    x->modal.masterGain = 1.;

    x->fr_save = x->x_fr;

    post("agogo...");

    return (x);
}

void agogo_tilde_setup(void) {
    agogo_class = class_new(gensym("agogo~"), (t_newmethod)agogo_new,
                            (t_method)agogo_free, sizeof(t_agogo), 0, 0);
    class_addmethod(agogo_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_dsp, gensym("dsp"), A_NULL);
    class_addfloat(agogo_class, (t_method)agogo_float);
    class_addmethod(agogo_class, (t_method)agogo_spos, gensym("spos"), A_FLOAT,
                    A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_sa, gensym("sa"), A_FLOAT,
                    A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_vf, gensym("vf"), A_FLOAT,
                    A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_va, gensym("va"), A_FLOAT,
                    A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_freq, gensym("freq"), A_FLOAT,
                    A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_noteon, gensym("noteon"),
                    A_NULL);
    class_addmethod(agogo_class, (t_method)agogo_noteoff, gensym("noteoff"),
                    A_NULL);
}
