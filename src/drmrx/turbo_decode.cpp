/******************************************************************************\
 * QSSTV - LTE-style Turbo Decoder (Max-Log-MAP / BCJR)
 *
 * Iterative decoder with two SISO (Soft-In Soft-Out) MAP decoders.
 * RSC: g0=13o (feedback), g1=15o (feedforward), 8 states.
 *
\******************************************************************************/

#include "turbo_decode.h"
#include "../drmtx/common/mlc/TurboEncoder.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define NUM_STATES 8
#define CONSTRAINT_LEN 4
#define NEG_INF (-1e10f)

/* RSC trellis structure
 * State = 3 bits [s0,s1,s2], g0 feedback = input^s1^s2
 * Parity = fb^s0^s2 */

typedef struct {
  int nextState[NUM_STATES][2];  /* nextState[state][input] */
  int parityOut[NUM_STATES][2];  /* parityOut[state][input] */
  int prevState[NUM_STATES][2];  /* prevState[state][input_that_led_here] */
  int prevInput[NUM_STATES][2];  /* which input (0 or 1) led to this state */
} rsc_trellis_t;

static rsc_trellis_t trellis;
static int trellis_init = 0;

static void init_trellis(void)
{
  if (trellis_init) return;

  /* Build trellis for all 8 states and 2 inputs */
  for (int state = 0; state < NUM_STATES; state++)
  {
    int s0 = (state >> 0) & 1;
    int s1 = (state >> 1) & 1;
    int s2 = (state >> 2) & 1;

    for (int input = 0; input < 2; input++)
    {
      /* Feedback: fb = input ^ s1 ^ s2 */
      int fb = input ^ s1 ^ s2;
      /* Parity: p = fb ^ s0 ^ s2 */
      int par = fb ^ s0 ^ s2;
      /* Next state: s0'=fb, s1'=s0, s2'=s1 */
      int ns = (fb << 0) | (s0 << 1) | (s1 << 2);

      trellis.nextState[state][input] = ns;
      trellis.parityOut[state][input] = par;
    }
  }

  /* Build reverse lookup */
  memset(trellis.prevState, -1, sizeof(trellis.prevState));
  memset(trellis.prevInput, -1, sizeof(trellis.prevInput));
  for (int state = 0; state < NUM_STATES; state++)
  {
    for (int input = 0; input < 2; input++)
    {
      int ns = trellis.nextState[state][input];
      /* Store: prevState[ns][slot] = state, prevInput[ns][slot] = input */
      if (trellis.prevState[ns][0] < 0)
      {
        trellis.prevState[ns][0] = state;
        trellis.prevInput[ns][0] = input;
      }
      else
      {
        trellis.prevState[ns][1] = state;
        trellis.prevInput[ns][1] = input;
      }
    }
  }
  trellis_init = 1;
}

static inline float max2(float a, float b) { return (a > b) ? a : b; }

/*
 * SISO MAP decoder for one RSC constituent code.
 * Inputs:
 *   Lsys[K+3]  - LLR for systematic bits (channel + extrinsic)
 *   Lpar[K+3]  - LLR for parity bits (channel only)
 *   K          - info block size (not counting tail)
 * Output:
 *   Lext[K]    - Extrinsic LLR for info bits
 */
static void siso_decode(const float *Lsys, const float *Lpar, int K,
                        float *Lext)
{
  int T = K + 3; /* total trellis steps including tail */

  /* Alpha (forward state metrics) */
  float *alpha = (float *)malloc((T + 1) * NUM_STATES * sizeof(float));
  /* Beta (backward state metrics) */
  float *beta = (float *)malloc((T + 1) * NUM_STATES * sizeof(float));

  /* Initialize alpha: start in state 0 */
  for (int s = 0; s < NUM_STATES; s++)
    alpha[0 * NUM_STATES + s] = NEG_INF;
  alpha[0 * NUM_STATES + 0] = 0.0f;

  /* Forward pass */
  for (int t = 0; t < T; t++)
  {
    for (int s = 0; s < NUM_STATES; s++)
      alpha[(t + 1) * NUM_STATES + s] = NEG_INF;

    for (int s = 0; s < NUM_STATES; s++)
    {
      if (alpha[t * NUM_STATES + s] <= NEG_INF + 1.0f) continue;

      for (int u = 0; u < 2; u++)
      {
        int ns = trellis.nextState[s][u];
        int par = trellis.parityOut[s][u];

        /* Branch metric: gamma = 0.5 * (u*Lsys + par*Lpar)
         * Using bipolar: bit 0 → +1, bit 1 → -1
         * gamma = 0.5 * ((1-2u)*Lsys + (1-2par)*Lpar)
         * Simplification for max-log-MAP: */
        float gamma = 0.0f;
        if (u) gamma -= Lsys[t]; else gamma += Lsys[t];
        if (par) gamma -= Lpar[t]; else gamma += Lpar[t];
        gamma *= 0.5f;

        float newAlpha = alpha[t * NUM_STATES + s] + gamma;
        if (newAlpha > alpha[(t + 1) * NUM_STATES + ns])
          alpha[(t + 1) * NUM_STATES + ns] = newAlpha;
      }
    }
  }

  /* Initialize beta: end in state 0 (after tail) */
  for (int s = 0; s < NUM_STATES; s++)
    beta[T * NUM_STATES + s] = NEG_INF;
  beta[T * NUM_STATES + 0] = 0.0f;

  /* Backward pass */
  for (int t = T - 1; t >= 0; t--)
  {
    for (int s = 0; s < NUM_STATES; s++)
      beta[t * NUM_STATES + s] = NEG_INF;

    for (int s = 0; s < NUM_STATES; s++)
    {
      if (beta[(t + 1) * NUM_STATES + s] <= NEG_INF + 1.0f) continue;

      for (int slot = 0; slot < 2; slot++)
      {
        int ps = trellis.prevState[s][slot];
        int pu = trellis.prevInput[s][slot];
        if (ps < 0) continue;

        int par = trellis.parityOut[ps][pu];
        float gamma = 0.0f;
        if (pu) gamma -= Lsys[t]; else gamma += Lsys[t];
        if (par) gamma -= Lpar[t]; else gamma += Lpar[t];
        gamma *= 0.5f;

        float newBeta = beta[(t + 1) * NUM_STATES + s] + gamma;
        if (newBeta > beta[t * NUM_STATES + ps])
          beta[t * NUM_STATES + ps] = newBeta;
      }
    }
  }

  /* Compute extrinsic LLR for info bits (first K positions only) */
  for (int t = 0; t < K; t++)
  {
    float maxU0 = NEG_INF;
    float maxU1 = NEG_INF;

    for (int s = 0; s < NUM_STATES; s++)
    {
      if (alpha[t * NUM_STATES + s] <= NEG_INF + 1.0f) continue;

      for (int u = 0; u < 2; u++)
      {
        int ns = trellis.nextState[s][u];
        int par = trellis.parityOut[s][u];

        float gamma_par = 0.0f; /* only parity contribution */
        if (par) gamma_par -= Lpar[t]; else gamma_par += Lpar[t];
        gamma_par *= 0.5f;

        float metric = alpha[t * NUM_STATES + s] + gamma_par +
                       beta[(t + 1) * NUM_STATES + ns];

        if (u == 0 && metric > maxU0) maxU0 = metric;
        if (u == 1 && metric > maxU1) maxU1 = metric;
      }
    }
    /* Extrinsic = LLR(u) - a priori (Lsys already contains a priori) */
    Lext[t] = maxU0 - maxU1;
  }

  free(alpha);
  free(beta);
}

int turbo_decode(const float *llr, int K, int rate,
                 char *infoOut, int maxIter)
{
  init_trellis();

  if (!llr || !infoOut || K < 40 || K > 8192)
    return -1;

  /* De-puncture: extract systematic, parity1, parity2 LLRs.
   * Punctured positions get LLR = 0 (no information). */
  float *Lch_sys = (float *)calloc(K + 3, sizeof(float));  /* channel systematic */
  float *Lch_par1 = (float *)calloc(K + 3, sizeof(float)); /* channel parity 1 */
  float *Lch_par2 = (float *)calloc(K + 3, sizeof(float)); /* channel parity 2 */

  int idx = 0;

  if (rate == TURBO_RATE_1_2)
  {
    for (int i = 0; i < K; i++) Lch_sys[i] = llr[idx++];
    for (int i = 0; i < K; i++)
    {
      if (i % 2 == 0)
        Lch_par1[i] = llr[idx++];
      else
        Lch_par2[i] = llr[idx++];
    }
  }
  else if (rate == TURBO_RATE_2_3)
  {
    for (int i = 0; i < K; i++) Lch_sys[i] = llr[idx++];
    for (int i = 0; i < K; i++)
    {
      if (i % 4 == 0)
        Lch_par1[i] = llr[idx++];
      else if (i % 4 == 2)
        Lch_par2[i] = llr[idx++];
    }
  }
  else if (rate == TURBO_RATE_3_4)
  {
    for (int i = 0; i < K; i++) Lch_sys[i] = llr[idx++];
    for (int i = 0; i < K; i++)
    {
      if (i % 6 == 0)
        Lch_par1[i] = llr[idx++];
      else if (i % 6 == 3)
        Lch_par2[i] = llr[idx++];
    }
  }
  else if (rate == TURBO_RATE_5_6)
  {
    for (int i = 0; i < K; i++) Lch_sys[i] = llr[idx++];
    for (int i = 0; i < K; i++)
    {
      if (i % 10 == 0)
        Lch_par1[i] = llr[idx++];
      else if (i % 10 == 5)
        Lch_par2[i] = llr[idx++];
    }
  }

  /* Tail bits (always 12: 3 sys1 + 3 par1 + 3 sys2 + 3 par2) */
  for (int i = 0; i < 3; i++) Lch_sys[K + i] = llr[idx++];   /* tail1 sys */
  for (int i = 0; i < 3; i++) Lch_par1[K + i] = llr[idx++];  /* tail1 par */
  float tail2_sys[3], tail2_par[3];
  for (int i = 0; i < 3; i++) tail2_sys[i] = llr[idx++];      /* tail2 sys */
  for (int i = 0; i < 3; i++) tail2_par[i] = llr[idx++];      /* tail2 par */

  /* Interleaver */
  int *perm = (int *)malloc(K * sizeof(int));
  turbo_interleaver(K, perm);

  /* Build reverse interleaver */
  int *iperm = (int *)malloc(K * sizeof(int));
  for (int i = 0; i < K; i++)
    iperm[perm[i]] = i;

  /* Iterative decoding */
  float *Lext1 = (float *)calloc(K, sizeof(float));  /* extrinsic from decoder 1 */
  float *Lext2 = (float *)calloc(K, sizeof(float));  /* extrinsic from decoder 2 */
  float *Lsys1 = (float *)malloc((K + 3) * sizeof(float));
  float *Lsys2 = (float *)malloc((K + 3) * sizeof(float));
  float *Lpar2_int = (float *)malloc((K + 3) * sizeof(float));

  /* Interleave parity2 channel LLRs + tail */
  for (int i = 0; i < K; i++)
    Lpar2_int[i] = Lch_par2[perm[i]]; /* parity2 was encoded on interleaved data */
  /* Actually parity2 is already in interleaved order from the encoder.
   * We need to pass it directly to decoder 2. */
  /* Correction: parity2[i] corresponds to interleaved bit i.
   * For decoder 2 which sees interleaved systematic, parity2 is in order. */
  for (int i = 0; i < K; i++)
    Lpar2_int[i] = Lch_par2[i];
  for (int i = 0; i < 3; i++)
    Lpar2_int[K + i] = tail2_par[i];

  int iter;
  for (iter = 0; iter < maxIter; iter++)
  {
    /* Decoder 1: systematic = channel_sys + extrinsic from decoder 2 (de-interleaved) */
    for (int i = 0; i < K; i++)
      Lsys1[i] = Lch_sys[i] + Lext2[i];
    for (int i = 0; i < 3; i++)
      Lsys1[K + i] = Lch_sys[K + i]; /* tail */

    siso_decode(Lsys1, Lch_par1, K, Lext1);

    /* Subtract a priori to get pure extrinsic */
    for (int i = 0; i < K; i++)
      Lext1[i] -= Lext2[i]; /* remove the a priori we added */

    /* Decoder 2: interleaved systematic + extrinsic from decoder 1 (interleaved) */
    for (int i = 0; i < K; i++)
      Lsys2[i] = Lch_sys[perm[i]] + Lext1[perm[i]];
    for (int i = 0; i < 3; i++)
      Lsys2[K + i] = tail2_sys[i]; /* tail */

    siso_decode(Lsys2, Lpar2_int, K, Lext2);

    /* De-interleave extrinsic from decoder 2 */
    float *Lext2_deint = (float *)malloc(K * sizeof(float));
    for (int i = 0; i < K; i++)
      Lext2_deint[i] = Lext2[perm[i]]; /* back to natural order */
    /* Subtract a priori */
    for (int i = 0; i < K; i++)
      Lext2_deint[i] -= Lext1[i];
    memcpy(Lext2, Lext2_deint, K * sizeof(float));
    free(Lext2_deint);

    /* Early stopping: check if all hard decisions are stable */
    if (iter > 0)
    {
      int stable = 1;
      for (int i = 0; i < K; i++)
      {
        float Ltotal = Lch_sys[i] + Lext1[i] + Lext2[i];
        char bit = (Ltotal < 0.0f) ? 1 : 0;
        if (bit != infoOut[i]) { stable = 0; break; }
      }
      if (stable) { iter++; break; }
    }

    /* Hard decisions */
    for (int i = 0; i < K; i++)
    {
      float Ltotal = Lch_sys[i] + Lext1[i] + Lext2[i];
      infoOut[i] = (Ltotal < 0.0f) ? 1 : 0;
    }
  }

  free(Lch_sys); free(Lch_par1); free(Lch_par2);
  free(perm); free(iperm);
  free(Lext1); free(Lext2);
  free(Lsys1); free(Lsys2); free(Lpar2_int);

  return iter;
}
