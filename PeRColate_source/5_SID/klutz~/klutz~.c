// klutz~ -- reverses the order of samples in a vector.
// 		by r. luke dubois (luke@music.columbia.edu),
//			computer music center, columbia university, 2001.
//
// objects and source are provided without warranty of any kind, express or
// implied.
//

#include "m_pd.h"
#include <math.h>

static t_class *klutz_class;

// my object structure
typedef struct _klutz {
    t_object x_obj; // this is an msp object...
} t_klutz;

// go go go...
static t_int *klutz_perform(t_int *w) {
    t_klutz *x = (t_klutz *)w[1]; // get current state of my object class
    t_float *in, *out;            // variables for input and output buffer
    int n;                        // counter for vector size

    in = (t_float *)(w[2]);
    out = (t_float *)(w[3]); // my output vector

    for (n = 0; n < (int)(w[4]); n++) {
        *++in; // my vector size
    }
    while (--n) {
        *++out = *--in;
    }

    return (w + 5); // return one greater than the arguments in the dsp_add call
}

static void klutz_dsp(t_klutz *x, t_signal **sp, short *count) {
    dsp_add(klutz_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

// instantiate the object
static void *klutz_new(float flag, float a) {
    t_klutz *x = (t_klutz *)pd_new(klutz_class);
    outlet_new(&x->x_obj, gensym("signal"));
    return (x);
}

void klutz_tilde_setup(void) {
    klutz_class = class_new(gensym("klutz~"), (t_newmethod)klutz_new, 0,
                            sizeof(t_klutz), 0, A_DEFFLOAT, 0);
    class_addmethod(klutz_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(klutz_class, (t_method)klutz_dsp, gensym("dsp"), A_NULL);
    class_sethelpsymbol(klutz_class, gensym("help-klutz~.pd"));
}
