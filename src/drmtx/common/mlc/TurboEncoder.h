/******************************************************************************\
 * QSSTV - LTE-style Turbo Encoder
 *
 * Description:
 *   Rate 1/3 turbo encoder with two RSC constituent encoders and QPP-style
 *   interleaver. Puncturing support for rates 1/2, 2/3, 3/4.
 *   Per-frame encoding — no multi-frame accumulation.
 *
 *   RSC generators (octal): g0=13 (feedback), g1=15 (feedforward)
 *   Constraint length K=4 (3 memory elements), same as LTE.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#ifndef TURBO_ENCODER_H
#define TURBO_ENCODER_H

/* Turbo code rates */
#define TURBO_RATE_1_3  0
#define TURBO_RATE_1_2  1
#define TURBO_RATE_2_3  2
#define TURBO_RATE_3_4  3

/*
 * turbo_encode - Encode a block of info bits with turbo code
 *
 * Parameters:
 *   infoBits    - Input info bits (0 or 1), length K
 *   K           - Number of info bits
 *   codedBits   - Output coded bits, length depends on rate
 *   rate        - TURBO_RATE_1_3, _1_2, _2_3, or _3_4
 *
 * Returns:
 *   Number of coded bits written, or -1 on error
 *
 * Output layout (rate 1/3, no puncturing):
 *   [systematic(K) | parity1(K) | parity2(K) | tail(12)]
 *   Total = 3*K + 12
 *
 * For other rates, puncturing reduces the parity bits.
 */
int turbo_encode(const char *infoBits, int K, char *codedBits, int rate);

/*
 * turbo_interleaver - Get the interleaver permutation for block size K
 *
 * Parameters:
 *   K       - Block size
 *   perm    - Output permutation array of size K
 *             perm[i] = interleaved position of bit i
 */
void turbo_interleaver(int K, int *perm);

/*
 * turbo_coded_length - Number of coded bits for given K and rate
 */
int turbo_coded_length(int K, int rate);

#endif /* TURBO_ENCODER_H */
