/******************************************************************************\
 * QSSTV - LDPC Encoder for IEEE 802.11n codes
 *
 * Description:
 *   Systematic LDPC encoder using WiFi 802.11n parity-check matrices.
 *   Replaces the convolutional encoder (CConvEncoder) in the MLC pipeline
 *   when LDPC mode is selected.
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

    void Init(int ldpcRate, int numInputBits, int numOutputBits);
    int  Encode(CVector<_DECISION>& vecInputData,
                CVector<_DECISION>& vecOutputData);

private:
    void buildSparseH();
    void encodeBlock(const unsigned char *infoBits, unsigned char *codeBits);

    int m_rate;             /* LDPC rate index (0=1/2, 1=2/3, 2=3/4, 3=5/6) */
    int m_k;                /* info bits per block */
    int m_n;                /* codeword bits per block (1944) */
    int m_nk;               /* parity bits per block (n - k) */
    int m_numBlocks;        /* number of LDPC blocks */
    int m_numInputBits;     /* total info bits from MLC */
    int m_numOutputBits;    /* total coded bits expected by MLC (= iNumEncBits) */
    int m_shortenBits;      /* shortening bits on last block */
    int m_punctureBits;     /* punctured parity bits on last block */

    /* Sparse representation of H matrix: for each check row, list of
       (column_index) with nonzero entry. Used for encoding. */
    std::vector<std::vector<int>> m_rowCols;
    /* For each (row, col) pair, the cyclic shift value */
    std::vector<std::vector<int>> m_rowShifts;

    /* Temporary buffers */
    std::vector<unsigned char> m_infoBuf;
    std::vector<unsigned char> m_codeBuf;
};

#endif /* LDPC_ENCODER_H */
