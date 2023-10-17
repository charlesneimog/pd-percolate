// chase~ -- uses a sync signal to determine who gets out which outlet.
// ported to PureData by Olaf Matthes, 2002
#include "m_pd.h"
#include <math.h>

#define MAXFRAMES 32
static t_class *chase_class;

typedef struct _chase {
    t_object x_obj;
} t_chase;

static t_int *chase_perform(t_int *w) {
    // t_chase *x = (t_chase *)(w[1]);

    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *sync = (t_float *)(w[4]);
    t_float *out1 = (t_float *)(w[5]);
    t_float *out2 = (t_float *)(w[6]);
    int n = (int)(w[7]);
    t_float dist1, dist2;

    while (--n) {
        if (*++sync > *++in1) {
            dist1 = fabs(*sync - *in1);
        } else {
            dist1 = fabs(*in1 - *sync);
        }
        if (*sync > *++in2) {
            dist2 = fabs(*sync - *in1);
        } else {
            dist2 = fabs(*in2 - *sync);
        }
        if (dist1 > dist2) {
            *++out1 = dist1;
            *++out2 = dist2;
        } else {
            *++out1 = dist2;
            *++out2 = dist1;
        }
    }
    return (w + 8);
}

static void chase_dsp(t_chase *x, t_signal **sp) {
    dsp_add(chase_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec, sp[4]->s_vec, sp[0]->s_n);
}

static void *chase_new(void) {
    t_chase *x = (t_chase *)pd_new(chase_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"),
              gensym("signal")); // signal 2
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"),
              gensym("signal"));             // sync
    outlet_new(&x->x_obj, gensym("signal")); // near output
    outlet_new(&x->x_obj, gensym("signal")); // far output
    post("chase chase chase...");
    return (x);
}

void chase_tilde_setup(void) {
    chase_class = class_new(gensym("chase~"), (t_newmethod)chase_new, 0,
                            sizeof(t_chase), 0, 0);
    class_addmethod(chase_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(chase_class, (t_method)chase_dsp, gensym("dsp"), A_NULL);
    class_sethelpsymbol(chase_class, gensym("help-chase~.pd"));
}
