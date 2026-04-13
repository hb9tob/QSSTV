/******************************************************************************\
 * QSSTV - LDPC Decoder for IEEE 802.11n codes
 *
 * Description:
 *   Min-sum belief propagation LDPC decoder.
 *   Replaces viterbi_decode() in the MSC decoding path when LDPC mode
 *   is signaled.
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
 * ldpc_decode - Decode one or more LDPC blocks from LLR values
 *
 * Parameters:
 *   llr            - Log-likelihood ratios from demapper (positive = bit is 0)
 *   n_coded_bits   - Total number of coded bits (may not be multiple of 1944)
 *   ldpc_rate      - LDPC rate index: 0=1/2, 1=2/3, 2=3/4, 3=5/6
 *   infoout        - Output buffer for decoded info bits (hard decisions)
 *   max_iter       - Maximum number of BP iterations per block
 *   max_info_bits  - Maximum info bits to write (prevents buffer overflow
 *                    when LDPC block size exceeds MLC buffer allocation)
 *   cw_hard        - Output buffer for full codeword hard decisions
 *                    (n_coded_bits entries, used to update hardpoints).
 *                    May be NULL if not needed.
 *
 * Returns:
 *   0 on success, nonzero on error
 */
int ldpc_decode(float *llr, int n_coded_bits, int ldpc_rate,
                char *infoout, int max_iter, int max_info_bits,
                char *cw_hard);

#endif /* LDPC_DECODE_H */
