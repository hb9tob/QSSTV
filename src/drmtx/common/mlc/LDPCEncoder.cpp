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

            /* XOR the cyclically shifted z-bit sub-block into syndrome */
            for (int j = 0; j < z; j++)
            {
                int srcIdx = bc * z + j;
                int dstIdx = br * z + ((j + shift) % z);
                syndrome[dstIdx] ^= infoBits[srcIdx];
            }
        }
    }

    /* Step 2: Resolve parity bits using the parity part of H.
       The parity columns of the base matrix (columns baseCols_info..23)
       form a near-dual-diagonal structure.

       For 802.11n rate 1/2, the parity part has:
         - First column: entry in row 0 (value v0) and row with value v1
         - Remaining: dual-diagonal with shift 0

       Generic approach: process parity columns left to right.
       For each base parity column, accumulate contributions from
       previously determined parity sub-blocks, then solve. */

    /* Build parity column structure */
    /* For each base parity column (index 0..baseRows-1 mapped to
       base matrix columns baseCols_info..23), find which rows reference it
       and with what shift */
    struct ParityEntry {
        int baseRow;
        int shift;
    };
    std::vector<std::vector<ParityEntry>> parityColEntries(baseRows);

    for (int br = 0; br < baseRows; br++)
    {
        for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
        {
            int bc = m_rowCols[br][idx];
            int shift = m_rowShifts[br][idx];
            if (bc >= baseCols_info)
            {
                int pc = bc - baseCols_info;
                ParityEntry pe;
                pe.baseRow = br;
                pe.shift = shift;
                parityColEntries[pc].push_back(pe);
            }
        }
    }

    /* For the 802.11n structure, parity column pc appears in at most
       2 rows (the diagonal and possibly the first row for the special
       column). We resolve column by column.

       The dual-diagonal means each parity column pc appears in row pc
       (with some shift) and row pc-1 (or first row for special structure).

       We use iterative approach: process each parity sub-block,
       XOR-ing known contributions. */

    /* Initialize parity sub-blocks */
    unsigned char *parity = codeBits + m_k;

    /* Process first parity column: find the row where this column
       appears with only this parity unknown */
    /* For all 802.11n codes, we use a general approach:
       Process parity columns 0..baseRows-1 sequentially.
       For column pc, find a row where pc is the ONLY unsolved parity column.
       Initially all parity columns are unsolved. */

    std::vector<bool> solved(baseRows, false);

    for (int iter = 0; iter < baseRows; iter++)
    {
        /* Find a row with exactly one unsolved parity column */
        int solveRow = -1;
        int solveCol = -1;

        for (int br = 0; br < baseRows; br++)
        {
            int unsolvedCount = 0;
            int lastUnsolved = -1;
            for (size_t idx = 0; idx < m_rowCols[br].size(); idx++)
            {
                int bc = m_rowCols[br][idx];
                if (bc >= baseCols_info)
                {
                    int pc = bc - baseCols_info;
                    if (!solved[pc])
                    {
                        unsolvedCount++;
                        lastUnsolved = pc;
                    }
                }
            }
            if (unsolvedCount == 1)
            {
                solveRow = br;
                solveCol = lastUnsolved;
                break;
            }
        }

        if (solveRow < 0)
        {
            /* Fallback: couldn't find a row with 1 unknown.
               This shouldn't happen with valid 802.11n matrices.
               Set remaining parity to zero. */
            break;
        }

        /* Compute the parity sub-block for column solveCol from row solveRow.
           The syndrome for this row must be zero:
           syndrome[row] XOR contributions_from_solved_parity XOR new_parity = 0
           => new_parity = syndrome[row] XOR contributions_from_solved_parity */

        /* Start with syndrome from info bits */
        /* Already computed in syndrome[] */

        /* XOR contributions from already-solved parity columns in this row */
        unsigned char temp[LDPC_Z];
        memcpy(temp, &syndrome[solveRow * z], z);

        for (size_t idx = 0; idx < m_rowCols[solveRow].size(); idx++)
        {
            int bc = m_rowCols[solveRow][idx];
            int shift = m_rowShifts[solveRow][idx];
            if (bc >= baseCols_info)
            {
                int pc = bc - baseCols_info;
                if (solved[pc])
                {
                    /* XOR the already-solved parity sub-block (shifted) */
                    for (int j = 0; j < z; j++)
                    {
                        int srcIdx = pc * z + j;
                        int dstJ = (j + shift) % z;
                        temp[dstJ] ^= parity[srcIdx];
                    }
                }
            }
        }

        /* Now temp[] contains the value we need for the solved parity column,
           but we need to account for the shift of solveCol in solveRow.
           If the entry for solveCol in solveRow has shift s, then:
           sum = ... + P_shifted = 0, where P_shifted[j] = parity[(j+s)%z]
           So parity[(j+s)%z] = temp[j], i.e., parity[j] = temp[(j-s+z)%z] */

        int solveShift = 0;
        for (size_t idx = 0; idx < m_rowCols[solveRow].size(); idx++)
        {
            int bc = m_rowCols[solveRow][idx];
            if (bc - baseCols_info == solveCol)
            {
                solveShift = m_rowShifts[solveRow][idx];
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
