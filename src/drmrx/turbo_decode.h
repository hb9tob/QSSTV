/******************************************************************************\
 * QSSTV - LTE-style Turbo Decoder
 *
 * Description:
 *   Max-log-MAP (BCJR) iterative turbo decoder.
 *   Matches TurboEncoder: RSC g0=13o, g1=15o, constraint length 4.
 *   Per-frame decoding — no multi-frame state.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#ifndef TURBO_DECODE_H
#define TURBO_DECODE_H

/*
 * turbo_decode - Decode a turbo-coded block from LLR values
 *
 * Parameters:
 *   llr         - Input LLR values (positive = bit 0 more likely)
 *   K           - Number of info bits (block size)
 *   rate        - TURBO_RATE_1_3, _1_2, _2_3, or _3_4
 *   infoOut     - Output hard decisions (0 or 1), length K
 *   maxIter     - Maximum number of turbo iterations
 *
 * Returns:
 *   Number of iterations used (positive = converged)
 *   Negative = did not converge
 */
int turbo_decode(const float *llr, int K, int rate,
                 char *infoOut, int maxIter);

#endif /* TURBO_DECODE_H */
