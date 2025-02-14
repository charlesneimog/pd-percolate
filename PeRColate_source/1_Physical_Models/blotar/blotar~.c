/******************************************/
/*  WaveGuide Flute ala Karjalainen,      */
/*  Smith, Waryznyk, etc.                 */
/*  with polynomial Jet ala Cook          */
/*  by Perry Cook, 1995-96                */
/*										  */
/*  ported to MSP by Dan Trueman, 2000	  */
/*  and modified to become the nearly	  */
/*  righteous Blotar.				 	  */
/*										  */
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
#include "mandimpulses.h"
#include "stk_c.h"

#ifdef NT
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)
#endif

#ifdef PD
#include "m_pd.h"
#endif

#define LENGTH 2048          // 44100/LOWFREQ + 1 --blotar length
#define JETLENGTH LENGTH * 2 // larger, for big rooms
#define VIBLENGTH 1024

/* --------------------------------- MSP -------------------------- */
#ifdef MSP
void *blotar_class;

typedef struct _blotar {
  // header
  t_pxobject x_obj;

  // user controlled vars
  float pluckAmp;    // pluck amplitude
  float pluckPos;    // that's right
  float bodySize;    // big blotarlin
  float x_fr;        // frequency
  float x_bp;        // breath pressure
  float x_jd;        // jet frequency
  float x_ng;        // noise gain
  float x_vf;        // vib freq
  float x_va;        // vib amount
  float x_jr;        // pre-feedback gain (jet reflection coeff)
  float x_er;        // pre-distortion gain (end reflection coeff)
  float filterRatio; // OneZero vs. OnePole filter ratio

  int mic; // directional position (NBody)

  float fr_save;
  float jd_save;

  // signals connected? or controls...
  short pluckAmpconnected;
  short pluckPosconnected;
  short bodySizeconnected;
  short x_frconnected;
  short x_bpconnected;
  short x_jdconnected;
  short x_ngconnected;
  short x_vfconnected;
  short x_vaconnected;
  short x_jrconnected;
  short x_erconnected;
  short filterRatioconnected;

  // dleay lines, flute
  DLineL boreDelay;
  DLineL jetDelay;
  DLineL combDelay;

  // DC blocker
  DCBlock killdc;

  // impulse response files
  HeaderSnd soundfile[12];

  // filters
  OnePole flute_filter;
  OneZero lowpass; // lowpass filter

  // vibrato table
  float vibTable[VIBLENGTH];
  float vibRate;
  float vibTime;

  // stuff
  float endRefl;
  float jetRefl;

  // stuff
  long length;
  float lastFreq;
  float lastLength;
  short pluck;
  long dampTime;
  int waveDone;
  float directBody;

  float srate, one_over_srate;
} t_blotar;

/****PROTOTYPES****/

// setup funcs
void *blotar_new(double val);
void blotar_free(t_blotar *x);
void blotar_dsp(t_blotar *x, t_signal **sp, short *count);
void blotar_float(t_blotar *x, double f);
void blotar_bang(t_blotar *x);
void blotar_clear(t_blotar *x);
t_int *blotar_perform(t_int *w);
void blotar_assist(t_blotar *x, void *b, long m, long a, char *s);

// blotar functions
void setFreq(t_blotar *x, float frequency);
void pluck(t_blotar *x, float amplitude, float position);
void setBodySize(t_blotar *x, float size);
void setmic(t_blotar *x, Symbol *s, short argc, Atom *argv);
void setJetDelay(t_blotar *x, float ratio);

// vib funcs
void setVibFreq(t_blotar *x, float freq);
float vib_tick(t_blotar *x);

/****FUNCTIONS****/

void pluck(t_blotar *x, float amplitude,
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

void setBodySize(t_blotar *x, float size) {
  int i;
  for (i = 0; i < 12; i++) {
    HeaderSnd_setRate(&x->soundfile[i], size);
  }
}

void blotar_bang(t_blotar *x) { x->pluck = 1; }

#define WATCHIT 0.00001
void setFreq(t_blotar *x, float frequency) {
  float temp;
  if (frequency < 20.)
    frequency = 20.;
  x->lastFreq = frequency;
  x->lastLength = x->srate / x->lastFreq; /* length - delays */
  /*
  if (x->detuning != 0.) {
        DLineA_setDelay(&x->delayLine, (x->lastLength / x->detuning) - .5);
        DLineA_setDelay(&x->delayLine2, (x->lastLength * x->detuning) - .5);
  }
  x->loopGain = x->baseLoopGain + (frequency * 0.000005);
  if (x->loopGain>1.0) x->loopGain = 0.99999;
  */
  x->lastFreq = frequency * 0.66666;
  if (x->lastFreq < WATCHIT)
    x->lastFreq = WATCHIT;
  temp = (x->srate / x->lastFreq) - 2.;
  DLineL_setDelay(&x->boreDelay, temp);
}

void setJetDelay(t_blotar *x, float frequency) {
  float temp;
  if (frequency < WATCHIT)
    frequency = WATCHIT;
  temp = (x->srate / frequency) - 2.;

  // control jet length directly, not as function of bore length
  DLineL_setDelay(&x->jetDelay, temp);
}

// vib funcs
void setVibFreq(t_blotar *x, float freq) {
  x->vibRate = VIBLENGTH * x->one_over_srate * freq;
}

float vib_tick(t_blotar *x) {
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

// primary MSP funcs
void main(void) {
  setup((struct messlist **)&blotar_class, (method)blotar_new,
        (method)blotar_free, (short)sizeof(t_blotar), 0L, A_DEFFLOAT, 0);
  addmess((method)blotar_dsp, "dsp", A_CANT, 0);
  addmess((method)blotar_assist, "assist", A_CANT, 0);
  addmess((method)setmic, "mic", A_GIMME, 0);
  addmess((method)blotar_clear, "clear", A_GIMME, 0);
  addfloat((method)blotar_float);
  addbang((method)blotar_bang);
  dsp_initclass();
  rescopy('STR#', 9982);
}

void setmic(t_blotar *x, Symbol *s, short argc, Atom *argv) {
  short i;
  int temp;
  for (i = 0; i < argc; i++) {
    switch (argv[i].a_type) {
    case A_LONG:
      temp = (int)argv[i].a_w.w_long;
      x->mic = temp % 12;
      post("blotar: setting mic: %d", x->mic);
      break;
    case A_FLOAT:
      temp = (int)argv[i].a_w.w_long;
      x->mic = temp % 12;
      post("blotar: setting mic: %d", x->mic);
      break;
    }
  }
}

void blotar_assist(t_blotar *x, void *b, long m, long a, char *s) {
  assist_string(9982, m, a, 1, 13, s);
}

void blotar_float(t_blotar *x, double f) {
  if (x->x_obj.z_in == 0) {
    x->pluckAmp = f;
  } else if (x->x_obj.z_in == 1) {
    x->pluckPos = f;
  } else if (x->x_obj.z_in == 2) {
    x->bodySize = f;
  } else if (x->x_obj.z_in == 3) {
    x->x_bp = f;
  } else if (x->x_obj.z_in == 4) {
    x->x_jd = f;
  } else if (x->x_obj.z_in == 5) {
    x->x_ng = f;
  } else if (x->x_obj.z_in == 6) {
    x->x_vf = f;
  } else if (x->x_obj.z_in == 7) {
    x->x_va = f;
  } else if (x->x_obj.z_in == 8) {
    x->x_fr = f;
  } else if (x->x_obj.z_in == 9) {
    x->x_jr = f;
  } else if (x->x_obj.z_in == 10) {
    x->x_er = f;
  } else if (x->x_obj.z_in == 11) {
    x->filterRatio = f;
  }
}

void blotar_clear(t_blotar *x) {
  DLineL_clear(&x->boreDelay);
  DLineL_clear(&x->jetDelay);
  DLineL_clear(&x->combDelay);
}

void *blotar_new(double initial_coeff) {
  int i;
  char temp[128];

  t_blotar *x = (t_blotar *)newobject(blotar_class);
  // zero out the struct, to be careful (takk to jkclayton)
  if (x) {
    for (i = sizeof(t_pxobject); i < sizeof(t_blotar); i++)
      ((char *)x)[i] = 0;
  }

  dsp_setup((t_pxobject *)x, 12);
  outlet_new((t_object *)x, "signal");

  x->length = LENGTH;
  /*
  x->baseLoopGain = 0.995;
  x->loopGain = 0.999;
  */
  x->directBody = 1.0;
  x->mic = 0;
  x->dampTime = 0;
  x->waveDone = 1;
  x->pluckAmp = 0.3;
  x->pluckPos = 0.4;
  x->lastFreq = 80.;
  x->lastLength = x->length * 0.5;
  x->x_fr = 440.;
  x->pluck = 0;
  x->filterRatio = 1.;

  x->srate = sys_getsr();
  x->one_over_srate = 1. / x->srate;

  DLineL_alloc(&x->boreDelay, LENGTH);
  DLineL_alloc(&x->jetDelay,
               JETLENGTH); // longer here, for long feedback loops, big rooms
  DLineL_alloc(&x->combDelay, LENGTH);

  for (i = 0; i < VIBLENGTH; i++)
    x->vibTable[i] = sin(i * TWO_PI / VIBLENGTH);
  x->vibRate = 1.;
  x->vibTime = 0.;

  // clear stuff flute
  DLineL_clear(&x->boreDelay);
  DLineL_clear(&x->jetDelay);
  OnePole_init(&x->flute_filter);
  OneZero_init(&x->lowpass);
  DLineL_clear(&x->combDelay);

  // impulse responses
  for (i = 0; i < 12; i++) {
    HeaderSnd_alloc(&x->soundfile[i], &blotar[i][0], 721, "oneshot");
  }

  setFreq(x, x->x_fr);
  DLineL_setDelay(&x->jetDelay, 49.);

  OnePole_setPole(&x->flute_filter, 0.7 - (0.1 * 22050. / x->srate));
  OnePole_setGain(&x->flute_filter, -1.);

  x->fr_save = x->x_fr;
  x->jd_save = 49.;

  post("dooooooooooode, air guitar!");

  return (x);
}

void blotar_free(t_blotar *x) {
  int i;
  DLineL_free(&x->combDelay);
  DLineL_free(&x->boreDelay);
  DLineL_free(&x->jetDelay);
  for (i = 0; i < 12; i++)
    HeaderSnd_free(&x->soundfile[i]);
  dsp_free((t_pxobject *)x);
}

void blotar_dsp(t_blotar *x, t_signal **sp, short *count) {
  x->pluckAmpconnected = count[0];
  x->pluckPosconnected = count[1];
  x->bodySizeconnected = count[2];
  x->x_bpconnected = count[3];
  x->x_jdconnected = count[4];
  x->x_ngconnected = count[5];
  x->x_vfconnected = count[6];
  x->x_vaconnected = count[7];
  x->x_frconnected = count[8];
  x->x_jrconnected = count[9];
  x->x_erconnected = count[10];
  x->filterRatioconnected = count[11];

  x->srate = sp[0]->s_sr;
  x->one_over_srate = 1. / x->srate;

  OnePole_setPole(&x->flute_filter, 0.7 - (0.1 * 22050. / x->srate));

  dsp_add(blotar_perform, 15, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
          sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec,
          sp[8]->s_vec, sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
          sp[12]->s_vec, sp[0]->s_n);
}

t_int *blotar_perform(t_int *w) {
  t_blotar *x = (t_blotar *)(w[1]);

  float pluckAmp = x->pluckAmpconnected ? *(float *)(w[2]) : x->pluckAmp;
  float pluckPos = x->pluckPosconnected ? *(float *)(w[3]) : x->pluckPos;
  float bodySize = x->bodySizeconnected ? *(float *)(w[4]) : x->bodySize;
  float bp = x->x_bpconnected ? *(float *)(w[5]) : x->x_bp;
  float jd = x->x_jdconnected ? *(float *)(w[6]) : x->x_jd;
  float ng = x->x_ngconnected ? *(float *)(w[7]) : x->x_ng;
  float vf = x->x_vfconnected ? *(float *)(w[8]) : x->x_vf;
  float va = x->x_vaconnected ? *(float *)(w[9]) : x->x_va;
  float fr = x->x_frconnected ? *(float *)(w[10]) : x->x_fr;
  float jr = x->x_jrconnected ? *(float *)(w[11]) : x->x_jr;
  float er = x->x_erconnected ? *(float *)(w[12]) : x->x_er;
  float filterRatio =
      x->filterRatioconnected ? *(float *)(w[13]) : x->filterRatio;

  float *out = (float *)(w[14]);
  long n = w[15];

  float temp, tempsave, lastOutput, pressureDiff, randPressure, filterRatioInv;

  if (fr != x->fr_save) {
    setFreq(x, fr);
    x->fr_save = fr;
  }

  // room feedback length, or jet delay length
  if (jd != x->jd_save) {
    setJetDelay(x, jd);
    x->jd_save = jd;
  }

  filterRatioInv = 1. - filterRatio;
  setBodySize(x, bodySize);
  x->vibRate = VIBLENGTH * x->one_over_srate * vf;

  if (x->pluck) {
    pluck(x, pluckAmp, pluckPos);
    x->pluck = 0;
  }

  while (n--) {

    randPressure = ng * Noise_tick();
    randPressure += va * vib_tick(x);
    randPressure *= bp;

    temp = 0.;
    // if (!x->waveDone)      {
    x->waveDone = HeaderSnd_informTick(
        &x->soundfile[x->mic]); /* as long as it goes . . .   */
    temp = x->soundfile[x->mic].lastOutput *
           pluckAmp; /* scaled pluck excitation    */
    temp = temp - DLineL_tick(&x->combDelay, temp); /* with comb filtering */
    //}

    // balance OnePole (flute) with LowPass (Karplus Strong); total wacko hack,
    // but sounds cool
    tempsave = temp;
    temp = OnePole_tick(&x->flute_filter, (x->boreDelay.lastOutput + temp));
    temp = filterRatio * temp +
           filterRatioInv *
               OneZero_tick(&x->lowpass, (x->boreDelay.lastOutput + tempsave));

    temp = DCBlock_tick(&x->killdc, temp);
    pressureDiff = bp + randPressure - (jr * temp);
    pressureDiff = DLineL_tick(&x->jetDelay, pressureDiff);
    pressureDiff =
        JetTabl_lookup(pressureDiff + (er * temp)); // becomes "tube" distortion

    *out++ = DLineL_tick(&x->boreDelay, pressureDiff);
  }
  return w + 16;
}
#endif /* MSP */

/* -------------------------------------- Pure Data
 * ------------------------------ */
#ifdef PD
static t_class *blotar_class;

typedef struct _blotar {
  // header
  t_object x_obj;

  // user controlled vars
  float pluckAmp;    // pluck amplitude
  float pluckPos;    // that's right
  float bodySize;    // big blotarlin
  float x_fr;        // frequency
  float x_bp;        // breath pressure
  float x_jd;        // jet frequency
  float x_ng;        // noise gain
  float x_vf;        // vib freq
  float x_va;        // vib amount
  float x_jr;        // pre-feedback gain (jet reflection coeff)
  float x_er;        // pre-distortion gain (end reflection coeff)
  float filterRatio; // OneZero vs. OnePole filter ratio

  int mic; // directional position (NBody)

  float fr_save;
  float jd_save;

  // dleay lines, flute
  DLineL boreDelay;
  DLineL jetDelay;
  DLineL combDelay;

  // DC blocker
  DCBlock killdc;

  // impulse response files
  HeaderSnd soundfile[12];

  // filters
  OnePole flute_filter;
  OneZero lowpass; // lowpass filter

  // vibrato table
  float vibTable[VIBLENGTH];
  float vibRate;
  float vibTime;

  // stuff
  float endRefl;
  float jetRefl;

  // stuff
  long length;
  float lastFreq;
  float lastLength;
  short pluck;
  long dampTime;
  int waveDone;
  float directBody;

  float srate, one_over_srate;
} t_blotar;

/****FUNCTIONS****/

static void pluck(t_blotar *x, float amplitude,
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

static void setBodySize(t_blotar *x, float size) {
  int i;
  for (i = 0; i < 12; i++) {
    HeaderSnd_setRate(&x->soundfile[i], size);
  }
}

static void blotar_bang(t_blotar *x) { x->pluck = 1; }

#define WATCHIT 0.00001
static void setFreq(t_blotar *x, float frequency) {
  float temp;
  if (frequency < 20.)
    frequency = 20.;
  x->lastFreq = frequency;
  x->lastLength = x->srate / x->lastFreq; /* length - delays */
  /*
  if (x->detuning != 0.) {
        DLineA_setDelay(&x->delayLine, (x->lastLength / x->detuning) - .5);
        DLineA_setDelay(&x->delayLine2, (x->lastLength * x->detuning) - .5);
  }
  x->loopGain = x->baseLoopGain + (frequency * 0.000005);
  if (x->loopGain>1.0) x->loopGain = 0.99999;
  */
  x->lastFreq = frequency * 0.66666;
  if (x->lastFreq < WATCHIT)
    x->lastFreq = WATCHIT;
  temp = (x->srate / x->lastFreq) - 2.;
  DLineL_setDelay(&x->boreDelay, temp);
}

static void setJetDelay(t_blotar *x, float frequency) {
  float temp;
  if (frequency < WATCHIT)
    frequency = WATCHIT;
  temp = (x->srate / frequency) - 2.;

  // control jet length directly, not as function of bore length
  DLineL_setDelay(&x->jetDelay, temp);
}

// vib funcs
static void setVibFreq(t_blotar *x, float freq) {
  x->vibRate = VIBLENGTH * x->one_over_srate * freq;
}

static float vib_tick(t_blotar *x) {
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

static t_int *blotar_perform(t_int *w) {
  t_blotar *x = (t_blotar *)(w[1]);

  float pluckAmp = x->pluckAmp;
  float pluckPos = x->pluckPos;
  float bodySize = x->bodySize;
  float bp = x->x_bp;
  float jd = x->x_jd;
  float ng = x->x_ng;
  float vf = x->x_vf;
  float va = x->x_va;
  float fr = x->x_fr;
  float jr = x->x_jr;
  float er = x->x_er;
  float filterRatio = x->filterRatio;

  t_float *out = (t_float *)(w[2]);
  long n = w[3];

  float temp, tempsave, lastOutput, pressureDiff, randPressure, filterRatioInv;

  if (fr != x->fr_save) {
    setFreq(x, fr);
    x->fr_save = fr;
  }

  // room feedback length, or jet delay length
  if (jd != x->jd_save) {
    setJetDelay(x, jd);
    x->jd_save = jd;
  }

  filterRatioInv = 1. - filterRatio;
  setBodySize(x, bodySize);
  x->vibRate = VIBLENGTH * x->one_over_srate * vf;

  if (x->pluck) {
    pluck(x, pluckAmp, pluckPos);
    x->pluck = 0;
  }

  while (n--) {

    randPressure = ng * Noise_tick();
    randPressure += va * vib_tick(x);
    randPressure *= bp;

    temp = 0.;
    // if (!x->waveDone)      {
    x->waveDone = HeaderSnd_informTick(
        &x->soundfile[x->mic]); /* as long as it goes . . .   */
    temp = x->soundfile[x->mic].lastOutput *
           pluckAmp; /* scaled pluck excitation    */
    temp = temp - DLineL_tick(&x->combDelay, temp); /* with comb filtering */
    //}

    // balance OnePole (flute) with LowPass (Karplus Strong); total wacko hack,
    // but sounds cool
    tempsave = temp;
    temp = OnePole_tick(&x->flute_filter, (x->boreDelay.lastOutput + temp));
    temp = filterRatio * temp +
           filterRatioInv *
               OneZero_tick(&x->lowpass, (x->boreDelay.lastOutput + tempsave));

    temp = DCBlock_tick(&x->killdc, temp);
    pressureDiff = bp + randPressure - (jr * temp);
    pressureDiff = DLineL_tick(&x->jetDelay, pressureDiff);
    pressureDiff =
        JetTabl_lookup(pressureDiff + (er * temp)); // becomes "tube" distortion

    *out++ = DLineL_tick(&x->boreDelay, pressureDiff);
  }
  return w + 4;
}

static void blotar_dsp(t_blotar *x, t_signal **sp) {
  x->srate = sp[0]->s_sr;
  x->one_over_srate = 1. / x->srate;

  OnePole_setPole(&x->flute_filter, 0.7 - (0.1 * 22050. / x->srate));

  dsp_add(blotar_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);
}

static void setmic(t_blotar *x, t_floatarg argc) {
  x->mic = (int)(argc) % 12;
  post("blotar: setting mic: %d", x->mic);
}

static void blotar_float(t_blotar *x, t_floatarg f) { x->pluckAmp = f; }

static void blotar_pluckPos(t_blotar *x, t_floatarg f) { x->pluckPos = f; }

static void blotar_bodySize(t_blotar *x, t_floatarg f) { x->bodySize = f; }

static void blotar_bp(t_blotar *x, t_floatarg f) { x->x_bp = f; }

static void blotar_jd(t_blotar *x, t_floatarg f) { x->x_jd = f; }

static void blotar_ng(t_blotar *x, t_floatarg f) { x->x_ng = f; }

static void blotar_vf(t_blotar *x, t_floatarg f) { x->x_vf = f; }

static void blotar_va(t_blotar *x, t_floatarg f) { x->x_va = f; }

static void blotar_freq(t_blotar *x, t_floatarg f) { x->x_fr = f; }

static void blotar_er(t_blotar *x, t_floatarg f) { x->x_er = f; }

static void blotar_jr(t_blotar *x, t_floatarg f) { x->x_jr = f; }

static void blotar_filterRatio(t_blotar *x, t_floatarg f) {
  x->filterRatio = f;
}

static void blotar_clear(t_blotar *x) {
  DLineL_clear(&x->boreDelay);
  DLineL_clear(&x->jetDelay);
  DLineL_clear(&x->combDelay);
}

static void blotar_free(t_blotar *x) {
  int i;
  DLineL_free(&x->combDelay);
  DLineL_free(&x->boreDelay);
  DLineL_free(&x->jetDelay);
  for (i = 0; i < 12; i++)
    HeaderSnd_free(&x->soundfile[i]);
}

static void *blotar_new(void) {
  unsigned int i;
  char temp[128];

  t_blotar *x = (t_blotar *)pd_new(blotar_class);
  // zero out the struct, to be careful (takk to jkclayton)
  if (x) {
    for (i = sizeof(t_object); i < sizeof(t_blotar); i++)
      ((char *)x)[i] = 0;
  }

  outlet_new(&x->x_obj, gensym("signal"));

  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("pluckPos"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("bodySize"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("bp"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("jd"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ng"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("vf"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("va"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("freq"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("jr"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("er"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("filterRatio"));

  x->length = LENGTH;
  /*
  x->baseLoopGain = 0.995;
  x->loopGain = 0.999;
  */
  x->directBody = 1.0;
  x->mic = 0;
  x->dampTime = 0;
  x->waveDone = 1;
  x->pluckAmp = 0.3;
  x->pluckPos = 0.4;
  x->lastFreq = 80.;
  x->lastLength = x->length * 0.5;
  x->x_fr = 440.;
  x->pluck = 0;
  x->filterRatio = 1.;

  x->srate = sys_getsr();
  x->one_over_srate = 1. / x->srate;

  DLineL_alloc(&x->boreDelay, LENGTH);
  DLineL_alloc(&x->jetDelay,
               JETLENGTH); // longer here, for long feedback loops, big rooms
  DLineL_alloc(&x->combDelay, LENGTH);

  for (i = 0; i < VIBLENGTH; i++)
    x->vibTable[i] = sin(i * TWO_PI / VIBLENGTH);
  x->vibRate = 1.;
  x->vibTime = 0.;

  // clear stuff flute
  DLineL_clear(&x->boreDelay);
  DLineL_clear(&x->jetDelay);
  OnePole_init(&x->flute_filter);
  OneZero_init(&x->lowpass);
  DLineL_clear(&x->combDelay);

  // impulse responses
  for (i = 0; i < 12; i++) {
    HeaderSnd_alloc(&x->soundfile[i], &mand[i][0], 721, "oneshot");
  }

  setFreq(x, x->x_fr);
  DLineL_setDelay(&x->jetDelay, 49.);

  OnePole_setPole(&x->flute_filter, 0.7 - (0.1 * 22050. / x->srate));
  OnePole_setGain(&x->flute_filter, -1.);

  x->fr_save = x->x_fr;
  x->jd_save = 49.;

  post("dooooooooooode, air guitar!");

  return (x);
}

void blotar_tilde_setup(void) {
  blotar_class = class_new(gensym("blotar~"), (t_newmethod)blotar_new,
                           (t_method)blotar_free, sizeof(t_blotar), 0, 0);
  class_addmethod(blotar_class, nullfn, gensym("signal"), A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_dsp, gensym("dsp"), A_NULL);
  class_addfloat(blotar_class, (t_method)blotar_float);
  class_addbang(blotar_class, (t_method)blotar_bang);
  class_addmethod(blotar_class, (t_method)blotar_pluckPos, gensym("pluckPos"),
                  A_FLOAT, A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_bodySize, gensym("bodySize"),
                  A_FLOAT, A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_bp, gensym("bp"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_jd, gensym("jd"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_ng, gensym("ng"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_vf, gensym("vf"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_va, gensym("va"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_freq, gensym("freq"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_jr, gensym("jr"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_er, gensym("er"), A_FLOAT,
                  A_NULL);
  class_addmethod(blotar_class, (t_method)blotar_filterRatio,
                  gensym("filterRatio"), A_FLOAT, A_NULL);
  class_addmethod(blotar_class, (t_method)setmic, gensym("setmic"), A_FLOAT,
                  A_NULL);
}
#endif /* PD */
