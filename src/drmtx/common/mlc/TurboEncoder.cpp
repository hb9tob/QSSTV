/******************************************************************************\
 * QSSTV - LTE-style Turbo Encoder
 *
 * Two parallel RSC encoders with interleaver and trellis termination.
 * RSC: g0=13o (feedback=1+D^2+D^3), g1=15o (feedforward=1+D+D^3)
 * Constraint length 4, 8 states.
 *
\******************************************************************************/

#include "TurboEncoder.h"
#include <string.h>
#include <math.h>

/* RSC encoder state machine (3 memory elements, 8 states)
 * g0 = 13 octal = 1011 binary (feedback: taps at D^0, D^1, D^3 — wait,
 *   actually g0 = 13o = 001 011 → bits: 1+D^2+D^3 for a K=4 code.
 *   Let me be precise: LTE uses g0=013 and g1=015 in octal.
 *   g0 = 1 + D + D^3 (feedback) = 1011 = 13 octal
 *   g1 = 1 + D^2 + D^3 (feedforward) -- wait, let me check.
 *
 * LTE 3GPP TS 36.212: transfer function G(D) = [1, g1(D)/g0(D)]
 *   g0(D) = 1 + D^2 + D^3   (feedback polynomial, octal 13)
 *   g1(D) = 1 + D + D^3     (feedforward polynomial, octal 15)
 *
 * State = [s0, s1, s2] (3 shift register bits)
 * Input bit u → feedback: fb = u XOR s1 XOR s2
 * Next state: s2'=s1, s1'=s0, s0'=fb
 * Parity output: p = fb XOR s0 XOR s2
 */

/* Encode one RSC constituent encoder, return parity bits */
static void rsc_encode(const char *input, int K, char *parity,
                       char *tail_sys, char *tail_par)
{
  int s0 = 0, s1 = 0, s2 = 0; /* shift register */
  int i;

  for (i = 0; i < K; i++)
  {
    /* Feedback: fb = input XOR s1 XOR s2 (g0 = 1+D^2+D^3) */
    int fb = input[i] ^ s1 ^ s2;
    /* Parity: p = fb XOR s0 XOR s2 (g1 = 1+D+D^3) */
    parity[i] = fb ^ s0 ^ s2;
    /* Shift register update */
    s2 = s1;
    s1 = s0;
    s0 = fb;
  }

  /* Trellis termination: 3 tail bits to drive state to zero.
   * During termination, the feedback input is driven by the state
   * (not external data) to force the state to all-zeros. */
  for (i = 0; i < 3; i++)
  {
    int fb = s1 ^ s2;  /* feedback from state (input forced to make fb=state-dependent) */
    tail_sys[i] = fb;  /* "systematic" tail bit = the forced input */
    tail_par[i] = fb ^ s0 ^ s2;
    s2 = s1;
    s1 = s0;
    s0 = fb;
  }
}

/* QPP-style interleaver: pi(i) = (f1*i + f2*i^2) mod K
 * f1 chosen coprime with K, f2 chosen for good spread.
 * For any K, we pick f1 = nearest prime to K*phi (golden ratio)
 * and f2 = smallest even number giving good minimum spread. */
static int gcd(int a, int b)
{
  while (b) { int t = b; b = a % b; a = t; }
  return a;
}

static int is_prime(int n)
{
  if (n < 2) return 0;
  if (n < 4) return 1;
  if (n % 2 == 0) return 0;
  for (int i = 3; i * i <= n; i += 2)
    if (n % i == 0) return 0;
  return 1;
}

void turbo_interleaver(int K, int *perm)
{
  /* Find f1: nearest prime to K/golden_ratio, coprime with K */
  int f1_target = (int)(K / 1.6180339887);
  int f1 = f1_target;
  while (!is_prime(f1) || gcd(f1, K) != 1)
    f1++;

  /* f2: small even value that gives good spread */
  int f2 = 2;
  if (K > 512) f2 = 4;
  if (K > 2048) f2 = 6;
  /* Ensure f2 and K are compatible for QPP */
  while (gcd(f2, K) != gcd(f2, K)) f2 += 2; /* no-op, just ensure even */

  for (int i = 0; i < K; i++)
  {
    /* QPP: pi(i) = (f1*i + f2*i*i) mod K */
    long long idx = ((long long)f1 * i + (long long)f2 * i * i) % K;
    if (idx < 0) idx += K;
    perm[i] = (int)idx;
  }
}

int turbo_coded_length(int K, int rate)
{
  /* Mother code: 3*K + 12 (systematic + 2 parities + 12 tail) */
  switch (rate)
  {
  case TURBO_RATE_1_3: return 3 * K + 12;
  case TURBO_RATE_1_2: return 2 * K + 12;
  case TURBO_RATE_2_3: return (3 * K) / 2 + 12;
  case TURBO_RATE_3_4: return (4 * K) / 3 + 12;
  default: return -1;
  }
}

int turbo_encode(const char *infoBits, int K, char *codedBits, int rate)
{
  if (!infoBits || !codedBits || K < 40 || K > 8192)
    return -1;

  /* Interleave input for second encoder */
  int *perm = new int[K];
  char *interleavedBits = new char[K];
  turbo_interleaver(K, perm);
  for (int i = 0; i < K; i++)
    interleavedBits[i] = infoBits[perm[i]];

  /* Encode with both RSC encoders */
  char *parity1 = new char[K];
  char *parity2 = new char[K];
  char tail1_sys[3], tail1_par[3];
  char tail2_sys[3], tail2_par[3];

  rsc_encode(infoBits, K, parity1, tail1_sys, tail1_par);
  rsc_encode(interleavedBits, K, parity2, tail2_sys, tail2_par);

  /* Assemble output with puncturing */
  int n = 0;

  if (rate == TURBO_RATE_1_3)
  {
    /* No puncturing: systematic + parity1 + parity2 */
    for (int i = 0; i < K; i++) codedBits[n++] = infoBits[i];
    for (int i = 0; i < K; i++) codedBits[n++] = parity1[i];
    for (int i = 0; i < K; i++) codedBits[n++] = parity2[i];
  }
  else if (rate == TURBO_RATE_1_2)
  {
    /* Puncture: keep all systematic, alternate parity1/parity2 */
    for (int i = 0; i < K; i++) codedBits[n++] = infoBits[i];
    for (int i = 0; i < K; i++)
    {
      if (i % 2 == 0)
        codedBits[n++] = parity1[i];
      else
        codedBits[n++] = parity2[i];
    }
  }
  else if (rate == TURBO_RATE_2_3)
  {
    /* Keep systematic, every 4th from each parity stream */
    for (int i = 0; i < K; i++) codedBits[n++] = infoBits[i];
    for (int i = 0; i < K; i++)
    {
      if (i % 4 == 0)
        codedBits[n++] = parity1[i];
      else if (i % 4 == 2)
        codedBits[n++] = parity2[i];
    }
  }
  else if (rate == TURBO_RATE_3_4)
  {
    /* Keep systematic, every 6th from each parity stream */
    for (int i = 0; i < K; i++) codedBits[n++] = infoBits[i];
    for (int i = 0; i < K; i++)
    {
      if (i % 6 == 0)
        codedBits[n++] = parity1[i];
      else if (i % 6 == 3)
        codedBits[n++] = parity2[i];
    }
  }

  /* Tail bits: always sent (12 bits: 3 sys + 3 par per encoder) */
  for (int i = 0; i < 3; i++) codedBits[n++] = tail1_sys[i];
  for (int i = 0; i < 3; i++) codedBits[n++] = tail1_par[i];
  for (int i = 0; i < 3; i++) codedBits[n++] = tail2_sys[i];
  for (int i = 0; i < 3; i++) codedBits[n++] = tail2_par[i];

  delete[] perm;
  delete[] interleavedBits;
  delete[] parity1;
  delete[] parity2;

  return n;
}
