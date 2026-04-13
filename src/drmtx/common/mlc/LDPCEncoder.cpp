/******************************************************************************\
 * QSSTV - LDPC Encoder for IEEE 802.11n codes
 *
 * Description:
 *   Systematic LDPC encoder. The 802.11n H matrices have a special
 *   dual-diagonal structure in the parity part that allows efficient
 *   encoding via back-substitution.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#include "LDPCEncoder.h"
#include "LDPCTables.h"
#include <cstring>
#include <cmath>

CLDPCEncoder::CLDPCEncoder()
    : m_rate(0), m_k(0), m_n(LDPC_N), m_nk(0),
      m_numBlocks(0), m_numInputBits(0), m_numOutputBits(0),
      m_shortenBits(0), m_punctureBits(0)
{
}

void CLDPCEncoder::Init(int ldpcRate, int numInputBits, int numOutputBits)
{
    m_rate = ldpcRate;
    if (m_rate < 0) m_rate = 0;
    if (m_rate >= LDPC_NUM_RATES) m_rate = LDPC_NUM_RATES - 1;

    m_k = ldpc_k[m_rate];
    m_n = LDPC_N;
    m_nk = ldpc_nk[m_rate];
    m_numInputBits = numInputBits;
    m_numOutputBits = numOutputBits;

    /* Calculate number of LDPC blocks needed */
    if (m_numInputBits <= 0)
        m_numBlocks = 0;
    else
        m_numBlocks = (m_numInputBits + m_k - 1) / m_k;

    /* Shortening: how many zero bits to pad the last info block */
    m_shortenBits = m_numBlocks * m_k - m_numInputBits;

    /* Puncturing: if total coded bits exceed what MLC expects, puncture
       parity bits from the last block(s) */
    int totalCodedBits = m_numBlocks * m_n;
    m_punctureBits = totalCodedBits - m_numOutputBits;
    if (m_punctureBits < 0) m_punctureBits = 0;

    /* Build sparse H matrix for encoding */
    buildSparseH();

    /* Allocate temp buffers */
    m_infoBuf.resize(m_k, 0);
    m_codeBuf.resize(m_n, 0);
}

void CLDPCEncoder::buildSparseH()
{
    int baseRows = ldpc_base_rows[m_rate];
    const int *baseH = ldpc_base_matrix[m_rate];

    m_rowCols.resize(baseRows);
    m_rowShifts.resize(baseRows);

    for (int r = 0; r < baseRows; r++)
    {
        m_rowCols[r].clear();
        m_rowShifts[r].clear();
        for (int c = 0; c < LDPC_BASE_COLS; c++)
        {
            int val = baseH[r * LDPC_BASE_COLS + c];
            if (val >= 0)
            {
                m_rowCols[r].push_back(c);
                m_rowShifts[r].push_back(val);
            }
        }
    }
}

/*
 * Encode a single LDPC block using the 802.11n quasi-cyclic structure.
 *
 * The H matrix has the form H = [A | B] where:
 *   - A is (n-k) x k (information part)
 *   - B is (n-k) x (n-k) (parity part) with dual-diagonal structure
 *
 * For 802.11n, B has the structure where the first row may differ,
 * but rows 2..m have identity shifted by 0 on the diagonal and
 * sub-diagonal, forming a dual-diagonal.
 *
 * Encoding: codeword = [s | p] where s = info bits, p = parity bits
 * Step 1: Compute syndrome contributions from info bits for each check row
 * Step 2: Resolve parity bits using the dual-diagonal structure of B
 */
void CLDPCEncoder::encodeBlock(const unsigned char *infoBits,
                                unsigned char *codeBits)
{
    int baseRows = ldpc_base_rows[m_rate];
    int z = LDPC_Z;
    int baseCols_info = LDPC_BASE_COLS - baseRows; /* info columns */

    /* Copy info bits to output (systematic) */
    memcpy(codeBits, infoBits, m_k);

    /* Initialize parity bits to zero */
    memset(codeBits + m_k, 0, m_nk);

    /* Step 1: For each base check row, compute the XOR contributions
       from the information columns using the cyclic shift structure */
    /* We store intermediate syndrome per sub-row of z bits */
    std::vector<unsigned char> syndrome(m_nk, 0);

    for (int br = 0; br < baseRows; br++)
    {
        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            int shift = m_rowShifts[br][idx];

            /* Only process information columns (bc < baseCols_info) */
            if (bc >= baseCols_info) continue;

            /* XOR the cyclically shifted z-bit sub-block into syndrome.
             * H[r*z+i, c*z+j] = 1 when j = (i+shift)%z, so the
             * contribution of column c at check position i is
             * infoBits[c*z + (i+shift)%z]. */
            for (int j = 0; j < z; j++)
            {
                int srcIdx = bc * z + ((j + shift) % z);
                int dstIdx = br * z + j;
                syndrome[dstIdx] ^= infoBits[srcIdx];
            }
        }
    }

    /* Step 2: Resolve parity bits using the 802.11n encoding algorithm.
     *
     * The 802.11n parity structure has a "first column" that appears in
     * 3 check rows, plus a dual-diagonal staircase for the remaining columns.
     * A naive iterative search for rows with 1 unsolved column FAILS because
     * no such row exists initially.
     *
     * Proper algorithm:
     * (a) p_0 = XOR of ALL syndrome sub-blocks (derived from the constraint
     *     that the sum of all check equations must equal zero)
     * (b) Process rows 0..m-2 in order. At each row, exactly one parity
     *     column is unsolved (the staircase guarantees this after p_0 is known).
     *     Solve it by XORing the syndrome with all known parity contributions.
     */
    unsigned char *parity = codeBits + m_k;

    /* (a) Compute p_0 = XOR of all syndrome sub-blocks */
    for (int j = 0; j < z; j++)
        parity[j] = 0;
    for (int br = 0; br < baseRows; br++)
    {
        for (int j = 0; j < z; j++)
            parity[j] ^= syndrome[br * z + j];
    }

    /* (b) Process rows 0..m-2 in order to solve remaining parity columns */
    std::vector<bool> solved(baseRows, false);
    solved[0] = true; /* p_0 is known */

    for (int br = 0; br < baseRows - 1; br++)
    {
        /* Find the single unsolved parity column in this row */
        int solveCol = -1;
        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            if (bc >= baseCols_info)
            {
                int pc = bc - baseCols_info;
                if (!solved[pc])
                {
                    solveCol = pc;
                    break;
                }
            }
        }

        if (solveCol < 0)
            continue; /* all parity in this row already solved (shouldn't happen) */

        /* Compute: new_parity = syndrome XOR all known parity contributions */
        unsigned char temp[LDPC_Z];
        memcpy(temp, &syndrome[br * z], z);

        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            int shift = m_rowShifts[br][idx];
            if (bc >= baseCols_info)
            {
                int pc = bc - baseCols_info;
                if (solved[pc])
                {
                    /* Contribution at check position j is
                     * parity[pc*z + (j+shift)%z] */
                    for (int j = 0; j < z; j++)
                    {
                        int srcJ = (j + shift) % z;
                        temp[j] ^= parity[pc * z + srcJ];
                    }
                }
            }
        }

        /* Account for the cyclic shift of the target column */
        int solveShift = 0;
        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            if (bc - baseCols_info == solveCol)
            {
                solveShift = m_rowShifts[br][idx];
                break;
            }
        }

        for (int j = 0; j < z; j++)
        {
            int srcJ = (j + solveShift) % z;
            parity[solveCol * z + srcJ] = temp[j];
        }

        solved[solveCol] = true;
    }
}

int CLDPCEncoder::Encode(CVector<_DECISION>& vecInputData,
                          CVector<_DECISION>& vecOutputData)
{
    int outputIdx = 0;

    for (int blk = 0; blk < m_numBlocks; blk++)
    {
        /* Fill info buffer: extract hard decisions from soft input */
        int infoStart = blk * m_k;
        memset(&m_infoBuf[0], 0, m_k);

        for (int i = 0; i < m_k; i++)
        {
            int srcIdx = infoStart + i;
            if (srcIdx < m_numInputBits)
            {
                /* Extract hard bit from _DECISION (may be soft or hard) */
                m_infoBuf[i] = (ExtractBit(vecInputData[srcIdx]) != 0) ? 1 : 0;
            }
            /* else: zero-padded (shortening) */
        }

        /* Encode the block */
        encodeBlock(&m_infoBuf[0], &m_codeBuf[0]);

        /* Copy encoded bits to output, respecting puncturing on last block */
        int bitsToOutput = m_n;
        if (blk == m_numBlocks - 1 && m_punctureBits > 0)
        {
            bitsToOutput = m_n - m_punctureBits;
            if (bitsToOutput < 0) bitsToOutput = 0;
        }

        for (int i = 0; i < bitsToOutput && outputIdx < m_numOutputBits; i++)
        {
            vecOutputData[outputIdx++] = m_codeBuf[i] ? 1 : 0;
        }
    }

    /* Zero-fill any remaining output bits (shouldn't normally happen) */
    while (outputIdx < m_numOutputBits)
    {
        vecOutputData[outputIdx++] = 0;
    }

    return outputIdx;
}
