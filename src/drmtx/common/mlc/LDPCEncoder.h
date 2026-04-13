/******************************************************************************\
 * QSSTV - LDPC Encoder for QC-LDPC codes (802.11n base matrices)
 *
 * Description:
 *   Systematic QC-LDPC encoder with runtime expansion factor z.
 *   Single codeword per interleaver period (6 frames long, 2 short).
 *   Uses PRBS padding for shortened positions (not zeros).
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#ifndef LDPC_ENCODER_H
#define LDPC_ENCODER_H

#include "../GlobalDefinitions.h"
#include "vector.h"
#include <vector>

class CLDPCEncoder
{
public:
    CLDPCEncoder();
    ~CLDPCEncoder() {}

    void Init(int ldpcRate, int z, int numInfoBits);
    int  Encode(CVector<_DECISION>& vecInputData,
                CVector<_DECISION>& vecOutputData);

    int getN() const { return m_n; }
    int getK() const { return m_k; }
    int getZ() const { return m_z; }

private:
    void buildSparseH();
    void encodeBlock(const unsigned char *infoBits, unsigned char *codeBits);
    void generatePRBS(unsigned char *buf, int len);

    int m_rate;             /* LDPC rate index (0=1/2, 1=2/3, 2=3/4, 3=5/6) */
    int m_z;                /* expansion factor (runtime) */
    int m_k;                /* info bits per block = (24 - baseRows) * z */
    int m_n;                /* codeword bits per block = 24 * z */
    int m_nk;               /* parity bits per block = baseRows * z */
    int m_numInfoBits;      /* actual info bits from MLC (may be < k) */
    int m_shortenBits;      /* k - numInfoBits: padded with PRBS */

    /* Sparse representation of H matrix */
    std::vector<std::vector<int>> m_rowCols;
    std::vector<std::vector<int>> m_rowShifts;

    /* Temporary buffers (allocated to max needed size) */
    std::vector<unsigned char> m_infoBuf;
    std::vector<unsigned char> m_codeBuf;
    std::vector<unsigned char> m_prbsBuf; /* PRBS padding for shortening */
};

#endif /* LDPC_ENCODER_H */
