/******************************************************************************\
 * QSSTV - LDPC Encoder for QC-LDPC codes (802.11n base matrices)
 *
 * Description:
 *   Systematic QC-LDPC encoder with runtime expansion factor z.
 *   Single codeword spanning the full interleaver depth.
 *   Shortened positions are filled with PRBS (not zeros) to avoid
 *   correlated phase transitions in OFDM.
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

CLDPCEncoder::CLDPCEncoder()
    : m_rate(0), m_z(0), m_k(0), m_n(0), m_nk(0),
      m_numInfoBits(0), m_shortenBits(0)
{
}

void CLDPCEncoder::generatePRBS(unsigned char *buf, int len)
{
    /* Same PRBS as DRM energy dispersal: P(X) = X^9 + X^5 + 1 */
    uint32_t reg = ~(uint32_t)0; /* all ones */
    for (int i = 0; i < len; i++)
    {
        unsigned char bit = ((reg >> 4) ^ (reg >> 8)) & 1;
        reg = (reg << 1) | bit;
        buf[i] = bit;
    }
}

void CLDPCEncoder::Init(int ldpcRate, int z, int numInfoBits)
{
    m_rate = ldpcRate;
    if (m_rate < 0) m_rate = 0;
    if (m_rate >= LDPC_NUM_RATES) m_rate = LDPC_NUM_RATES - 1;

    m_z = z;
    m_n = ldpc_n_from_z(z);
    m_k = ldpc_k_from_z(z, m_rate);
    m_nk = ldpc_nk_from_z(z, m_rate);
    m_numInfoBits = numInfoBits;

    /* Shortening: actual info < k → pad with PRBS */
    m_shortenBits = m_k - m_numInfoBits;
    if (m_shortenBits < 0) m_shortenBits = 0;

    /* Build sparse H matrix for this z */
    buildSparseH();

    /* Allocate buffers */
    m_infoBuf.resize(m_k, 0);
    m_codeBuf.resize(m_n, 0);

    /* Pre-generate PRBS for shortening padding */
    if (m_shortenBits > 0)
    {
        m_prbsBuf.resize(m_shortenBits);
        generatePRBS(&m_prbsBuf[0], m_shortenBits);
    }
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
                /* Take shift mod z for arbitrary expansion factor */
                m_rowShifts[r].push_back(val % m_z);
            }
        }
    }
}

/*
 * Encode a single LDPC block using the 802.11n quasi-cyclic structure.
 * Codeword = [info | parity], parity resolved via dual-diagonal back-sub.
 */
void CLDPCEncoder::encodeBlock(const unsigned char *infoBits,
                                unsigned char *codeBits)
{
    int baseRows = ldpc_base_rows[m_rate];
    int z = m_z;
    int baseCols_info = LDPC_BASE_COLS - baseRows;

    /* Copy info bits to output (systematic) */
    memcpy(codeBits, infoBits, m_k);

    /* Initialize parity bits to zero */
    memset(codeBits + m_k, 0, m_nk);

    /* Step 1: Compute syndrome from information columns */
    std::vector<unsigned char> syndrome(m_nk, 0);

    for (int br = 0; br < baseRows; br++)
    {
        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            int shift = m_rowShifts[br][idx];

            if (bc >= baseCols_info) continue;

            for (int j = 0; j < z; j++)
            {
                int srcIdx = bc * z + ((j + shift) % z);
                int dstIdx = br * z + j;
                syndrome[dstIdx] ^= infoBits[srcIdx];
            }
        }
    }

    /* Step 2: Resolve parity via dual-diagonal structure */
    unsigned char *parity = codeBits + m_k;

    /* (a) p_0 = XOR of all syndrome sub-blocks */
    for (int j = 0; j < z; j++)
        parity[j] = 0;
    for (int br = 0; br < baseRows; br++)
        for (int j = 0; j < z; j++)
            parity[j] ^= syndrome[br * z + j];

    /* (b) Resolve remaining parity columns in order */
    std::vector<bool> solved(baseRows, false);
    solved[0] = true;

    for (int br = 0; br < baseRows - 1; br++)
    {
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

        if (solveCol < 0) continue;

        /* Compute: parity = syndrome XOR known parity contributions */
        std::vector<unsigned char> temp(z);
        memcpy(&temp[0], &syndrome[br * z], z);

        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            int shift = m_rowShifts[br][idx];
            if (bc >= baseCols_info)
            {
                int pc = bc - baseCols_info;
                if (solved[pc])
                {
                    for (int j = 0; j < z; j++)
                        temp[j] ^= parity[pc * z + ((j + shift) % z)];
                }
            }
        }

        /* Apply cyclic shift of target column */
        int solveShift = 0;
        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            if (m_rowCols[br][idx] - baseCols_info == solveCol)
            {
                solveShift = m_rowShifts[br][idx];
                break;
            }
        }

        for (int j = 0; j < z; j++)
            parity[solveCol * z + ((j + solveShift) % z)] = temp[j];

        solved[solveCol] = true;
    }
}

int CLDPCEncoder::Encode(CVector<_DECISION>& vecInputData,
                          CVector<_DECISION>& vecOutputData)
{
    /* Fill info buffer: actual data + PRBS padding for shortening */
    for (int i = 0; i < m_k; i++)
    {
        if (i < m_numInfoBits)
            m_infoBuf[i] = (ExtractBit(vecInputData[i]) != 0) ? 1 : 0;
        else
            m_infoBuf[i] = m_prbsBuf[i - m_numInfoBits]; /* PRBS padding */
    }

    /* Encode single block */
    encodeBlock(&m_infoBuf[0], &m_codeBuf[0]);

    /* Output: all n coded bits (no puncturing!) */
    int outputBits = m_n;
    if (outputBits > vecOutputData.Size())
        outputBits = vecOutputData.Size();

    for (int i = 0; i < outputBits; i++)
        vecOutputData[i] = m_codeBuf[i] ? 1 : 0;

    return outputBits;
}
