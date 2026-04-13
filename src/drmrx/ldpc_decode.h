/******************************************************************************\
 * QSSTV - LDPC Decoder for QC-LDPC codes (802.11n base matrices)
 *
 * Description:
 *   Min-sum belief propagation LDPC decoder with runtime expansion factor z.
 *   Single codeword spanning the full interleaver depth.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#ifndef LDPC_DECODE_H
#define LDPC_DECODE_H

/*
 * ldpc_decode - Decode a single LDPC codeword from LLR values
 *
 * Parameters:
 *   llr            - Log-likelihood ratios (positive = bit is 0)
 *   n_coded_bits   - Number of coded bits received (= n = 24*z)
 *   ldpc_rate      - LDPC rate index: 0=1/2, 1=2/3, 2=3/4, 3=5/6
 *   z              - Expansion factor (runtime, computed from frame params)
 *   infoout        - Output buffer for decoded info bits (hard decisions)
 *   max_iter       - Maximum number of BP iterations
 *   max_info_bits  - Maximum info bits to write (actual data, before shortening)
 *
 * Returns:
 *   >= 0 : converged at iteration N (0 = immediate)
 *   < 0  : -max_iter if not converged, or other negative on error
 */
int ldpc_decode(float *llr, int n_coded_bits, int ldpc_rate, int z,
                char *infoout, int max_iter, int max_info_bits);

#endif /* LDPC_DECODE_H */
