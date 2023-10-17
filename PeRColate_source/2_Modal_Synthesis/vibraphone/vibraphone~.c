/********************************************/
/*  Vibraphone SubClass of Modal4 Instrument*/
/*  by Perry R. Cook, 1995-96               */
/*										  	*/
/*  ported to MSP by Dan Trueman, 2000	  	*/
/*                                          */
/*  ported to PD by Olaf Matthes, 2000	  	*/
/*                                          */
/*   Controls:    CONTROL1 = stickHardness  */
/*                CONTROL2 = strikePosition */
/*		  		  CONTROL3 = vibFreq       	*/
/*		  		  MOD_WHEEL= vibAmt        	*/
/********************************************/

#include "marmstk1.h"
#include "stk_c.h"
#include <math.h>

#include "m_pd.h"

static t_class *vibraphone_class;

typedef struct _vibraphone {
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

    // stuff
    int multiStrike;
    float srate, one_over_srate;
} t_vibraphone;

/****FUNCTIONS****/
static void Vibraphone_setStickHardness(t_vibraphone *x, float hardness) {
    x->x_sh = hardness;
    HeaderSnd_setRate(&x->modal.wave, 2. + 22.66 * hardness);
    x->modal.masterGain = 0.2 + (1.6 * hardness);
}

static void Vibraphone_setStrikePosition(t_vibraphone *x, float position) {
    float temp, temp2;
    temp2 = position * M_PI;
    x->x_spos = position; /*  Hack only first three modes */
    temp = sin(temp2);
    Modal4_setFiltGain(&x->modal, 0,
                       .025 * temp); /*  1st mode function of pos.   */
    temp = sin(0.1 + (2.01 * temp2));
    Modal4_setFiltGain(&x->modal, 1,
                       .015 * temp); /*  2nd mode function of pos.   */
    temp = sin(3.95 * temp2);
    Modal4_setFiltGain(&x->modal, 2,
                       .015 * temp); /*  3rd mode function of pos.   */
}

static t_int *vibraphone_perform(t_int *w) {
    t_vibraphone *x = (t_vibraphone *)(w[1]);

    float sh = x->x_sh;
    float spos = x->x_spos;
    float sa = x->x_sa;
    float vf = x->x_vf;
    float va = x->x_va;
    float fr = x->x_fr;

    float *out = (float *)(w[2]);
    long n = w[3];

    if (fr != x->fr_save) {
        Modal4_setFreq(&x->modal, fr);
        x->fr_save = fr;
    }

    if (sh != x->sh_save) {
        Vibraphone_setStickHardness(x, sh);
        x->sh_save = sh;
    }

    if (spos != x->spos_save) {
        Vibraphone_setStrikePosition(x, spos);
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

static void vibraphone_dsp(t_vibraphone *x, t_signal **sp) {
    x->srate = sp[0]->s_sr;
    x->one_over_srate = 1. / x->srate;
    dsp_add(vibraphone_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

/* ----- handle data from inlets ------ */
void vibraphone_noteon(t_vibraphone *x) {
    Modal4_noteOn(&x->modal, x->x_fr, x->x_sa);
}

void vibraphone_noteoff(t_vibraphone *x) { Modal4_noteOff(&x->modal, x->x_sa); }

static void vibraphone_float(t_vibraphone *x, t_floatarg f) { x->x_sh = f; }

static void vibraphone_spos(t_vibraphone *x, t_floatarg f) { x->x_spos = f; }

static void vibraphone_sa(t_vibraphone *x, t_floatarg f) { x->x_sa = f; }

static void vibraphone_vf(t_vibraphone *x, t_floatarg f) { x->x_vf = f; }

static void vibraphone_va(t_vibraphone *x, t_floatarg f) { x->x_va = f; }

static void vibraphone_freq(t_vibraphone *x, t_floatarg f) { x->x_fr = f; }

static void *vibraphone_new(void) {
    unsigned int i;
    char file[128];

    t_vibraphone *x = (t_vibraphone *)pd_new(vibraphone_class);
    // zero out the struct, to be careful (takk to jkclayton)
    if (x) {
        for (i = sizeof(t_object); i < sizeof(t_vibraphone); i++)
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
    strcpy(file, RAWWAVE_PATH);
    HeaderSnd_alloc(&x->modal.wave, marmstk1, 256, "oneshot");
    HeaderSnd_setRate(&x->modal.wave, 13.33); /*  normal stick  */

    Modal4_setRatioAndReson(&x->modal, 0, 1.00, 0.99995);
    Modal4_setRatioAndReson(&x->modal, 1, 2.01, 0.99991);
    Modal4_setRatioAndReson(&x->modal, 2, 3.9, 0.99992);
    Modal4_setRatioAndReson(&x->modal, 3, 14.37, 0.99990);
    Modal4_setFiltGain(&x->modal, 0, .025);
    Modal4_setFiltGain(&x->modal, 1, .015);
    Modal4_setFiltGain(&x->modal, 2, .015);
    Modal4_setFiltGain(&x->modal, 3, .015);
    x->modal.directGain = 0.0;
    x->multiStrike = 0;
    x->modal.masterGain = 1.;

    x->fr_save = x->x_fr;

    post("vibraphone...");

    return (x);
}

static void vibraphone_free(t_vibraphone *x) {
    HeaderSnd_free(&x->modal.wave);
    HeaderSnd_free(&x->modal.vibr);
}

void vibraphone_tilde_setup(void) {
    vibraphone_class =
        class_new(gensym("vibraphone~"), (t_newmethod)vibraphone_new,
                  (t_method)vibraphone_free, sizeof(t_vibraphone), 0, 0);
    class_addmethod(vibraphone_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_dsp, gensym("dsp"),
                    A_NULL);
    class_addfloat(vibraphone_class, (t_method)vibraphone_float);
    class_addmethod(vibraphone_class, (t_method)vibraphone_spos, gensym("spos"),
                    A_FLOAT, A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_sa, gensym("sa"),
                    A_FLOAT, A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_vf, gensym("vf"),
                    A_FLOAT, A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_va, gensym("va"),
                    A_FLOAT, A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_freq, gensym("freq"),
                    A_FLOAT, A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_noteon,
                    gensym("noteon"), A_NULL);
    class_addmethod(vibraphone_class, (t_method)vibraphone_noteoff,
                    gensym("noteoff"), A_NULL);
}
