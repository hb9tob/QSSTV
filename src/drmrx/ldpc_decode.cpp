/******************************************************************************\
 * QSSTV - LDPC Decoder for IEEE 802.11n codes
 *
 * Description:
 *   Normalized min-sum belief propagation decoder for 802.11n LDPC codes.
 *   Block size n=1944, expansion factor z=81.
 *   Supports rates 1/2, 2/3, 3/4, 5/6.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#include "ldpc_decode.h"
#include "../drmtx/common/mlc/LDPCTables.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>

/* Normalized min-sum scaling factor */
#define MS_SCALE 0.75f

/* Maximum variable node degree (for stack allocation) */
#define MAX_VN_DEGREE 20

/* Clamp LLR to prevent numerical issues */
#define LLR_CLAMP 50.0f

/*
 * Sparse representation of the expanded H matrix.
 * We store, for each check node, the list of connected variable nodes
 * and the corresponding cyclic shift.
 */
typedef struct {
    int numChecks;          /* number of check nodes = nk */
    int numVars;            /* number of variable nodes = n */
    int k;                  /* info bits per block */
    int *checkDegree;       /* degree of each check node */
    int **checkToVar;       /* checkToVar[i][j] = j-th variable node of check i */
    float **c2vMsg;         /* check-to-variable messages */
    float *varBelief;       /* current belief (LLR + sum of incoming c2v) */
    float *channelLLR;      /* saved channel LLR */
} ldpc_graph_t;

static void graph_init(ldpc_graph_t *g, int rate)
{
    int baseRows = ldpc_base_rows[rate];
    int baseCols = LDPC_BASE_COLS;
    int baseCols_info = baseCols - baseRows;
    int z = LDPC_Z;
    const int *baseH = ldpc_base_matrix[rate];

    g->numChecks = baseRows * z;
    g->numVars = LDPC_N;
    g->k = ldpc_k[rate];

    /* Count edges per check node (base row) */
    g->checkDegree = (int *)calloc(g->numChecks, sizeof(int));
    g->checkToVar = (int **)calloc(g->numChecks, sizeof(int *));
    g->c2vMsg = (float **)calloc(g->numChecks, sizeof(float *));

    /* First pass: count degrees */
    for (int br = 0; br < baseRows; br++)
    {
        int deg = 0;
        for (int bc = 0; bc < baseCols; bc++)
        {
            if (baseH[br * baseCols + bc] >= 0)
                deg++;
        }
        /* Each base check node expands to z check nodes, each with same degree */
        for (int sub = 0; sub < z; sub++)
        {
            g->checkDegree[br * z + sub] = deg;
        }
    }

    /* Second pass: fill adjacency lists */
    for (int br = 0; br < baseRows; br++)
    {
        /* Collect non-negative entries for this base row */
        int entries[LDPC_BASE_COLS];
        int shifts[LDPC_BASE_COLS];
        int deg = 0;

        for (int bc = 0; bc < baseCols; bc++)
        {
            int val = baseH[br * baseCols + bc];
            if (val >= 0)
            {
                entries[deg] = bc;
                shifts[deg] = val;
                deg++;
            }
        }

        /* Expand to z sub-check-nodes */
        for (int sub = 0; sub < z; sub++)
        {
            int ci = br * z + sub;
            g->checkToVar[ci] = (int *)malloc(deg * sizeof(int));
            g->c2vMsg[ci] = (float *)calloc(deg, sizeof(float));

            for (int d = 0; d < deg; d++)
            {
                int bc = entries[d];
                int shift = shifts[d];
                /* Variable node index: bc * z + (sub + shift) % z */
                g->checkToVar[ci][d] = bc * z + ((sub + shift) % z);
            }
        }
    }

    g->varBelief = (float *)calloc(LDPC_N, sizeof(float));
    g->channelLLR = (float *)calloc(LDPC_N, sizeof(float));
}

static void graph_free(ldpc_graph_t *g)
{
    for (int i = 0; i < g->numChecks; i++)
    {
        free(g->checkToVar[i]);
        free(g->c2vMsg[i]);
    }
    free(g->checkDegree);
    free(g->checkToVar);
    free(g->c2vMsg);
    free(g->varBelief);
    free(g->channelLLR);
}

/*
 * Run min-sum BP decoding on a single block.
 * Returns 1 if decoding succeeded (syndrome = 0), 0 otherwise.
 */
static int decode_block(ldpc_graph_t *g, float *blockLLR, char *infoOut,
                         int max_iter, int n_coded, int n_info)
{
    int n = LDPC_N;
    int z = LDPC_Z;

    /* Initialize channel LLR (clamp for numerical stability)
     *
     * Three regions in the codeword [info(k) | parity(n-k)]:
     *   0..n_coded-1       : received from channel (actual LLR)
     *   n_coded..k-1       : shortening zeros beyond received window
     *                        → set to +LLR_CLAMP (known to be 0)
     *   k..n-1             : punctured parity → set to 0 (erasure)
     *
     * Within the received window, positions n_info..k-1 are also
     * shortening zeros, but their channel LLR should already be
     * positive in a clean channel. We trust the channel value there.
     */
    for (int i = 0; i < n; i++)
    {
        float val;
        if (i < n_coded)
            val = blockLLR[i];
        else if (i < g->k)
            val = LLR_CLAMP;   /* shortening: known zero */
        else
            val = 0.0f;        /* punctured parity: unknown */
        if (val > LLR_CLAMP) val = LLR_CLAMP;
        if (val < -LLR_CLAMP) val = -LLR_CLAMP;
        g->channelLLR[i] = val;
        g->varBelief[i] = val;
    }

    /* Initialize c2v messages to zero */
    for (int ci = 0; ci < g->numChecks; ci++)
    {
        int deg = g->checkDegree[ci];
        memset(g->c2vMsg[ci], 0, deg * sizeof(float));
    }

    /* Iterative decoding */
    for (int iter = 0; iter < max_iter; iter++)
    {
        /* Check-node update (normalized min-sum) */
        for (int ci = 0; ci < g->numChecks; ci++)
        {
            int deg = g->checkDegree[ci];

            /* Compute variable-to-check messages: v2c = belief - old_c2v */
            float v2c[MAX_VN_DEGREE];
            for (int d = 0; d < deg; d++)
            {
                int vi = g->checkToVar[ci][d];
                v2c[d] = g->varBelief[vi] - g->c2vMsg[ci][d];
            }

            /* For each outgoing edge, compute min-sum message */
            for (int d = 0; d < deg; d++)
            {
                float minAbs = 1e20f;
                int signProduct = 0; /* XOR of sign bits */

                for (int d2 = 0; d2 < deg; d2++)
                {
                    if (d2 == d) continue;
                    float absVal = fabsf(v2c[d2]);
                    if (absVal < minAbs) minAbs = absVal;
                    if (v2c[d2] < 0.0f) signProduct ^= 1;
                }

                float newMsg = MS_SCALE * minAbs;
                if (signProduct) newMsg = -newMsg;

                /* Update belief: remove old c2v, add new c2v */
                int vi = g->checkToVar[ci][d];
                g->varBelief[vi] += (newMsg - g->c2vMsg[ci][d]);
                g->c2vMsg[ci][d] = newMsg;
            }
        }

        /* Check syndrome: verify all check nodes satisfied */
        int syndromeOK = 1;
        for (int ci = 0; ci < g->numChecks; ci++)
        {
            int deg = g->checkDegree[ci];
            int parity = 0;
            for (int d = 0; d < deg; d++)
            {
                int vi = g->checkToVar[ci][d];
                if (g->varBelief[vi] < 0.0f) parity ^= 1;
            }
            if (parity)
            {
                syndromeOK = 0;
                break;
            }
        }

        if (syndromeOK)
            break;
    }

    /* Extract info bits (first k bits of the codeword) */
    for (int i = 0; i < g->k; i++)
    {
        infoOut[i] = (g->varBelief[i] < 0.0f) ? 1 : 0;
    }

    return n_coded; /* return number of coded bits actually used */
}

int ldpc_decode(float *llr, int n_coded_bits, int ldpc_rate,
                char *infoout, int max_iter, int max_info_bits,
                char *cw_hard)
{
    if (!llr || !infoout)
        return ENOMEM;

    if (ldpc_rate < 0 || ldpc_rate >= LDPC_NUM_RATES)
        return EINVAL;

    int k = ldpc_k[ldpc_rate];
    int n = LDPC_N;

    /* Number of blocks */
    int numBlocks = (n_coded_bits + n - 1) / n;
    if (numBlocks <= 0) numBlocks = 1;

    /* Build graph for this rate */
    ldpc_graph_t graph;
    graph_init(&graph, ldpc_rate);

    int infoIdx = 0;

    for (int blk = 0; blk < numBlocks; blk++)
    {
        int offset = blk * n;
        int codedThisBlock = n;

        /* Last block may have fewer coded bits (punctured parity) */
        if (offset + codedThisBlock > n_coded_bits)
            codedThisBlock = n_coded_bits - offset;
        if (codedThisBlock < 0) codedThisBlock = 0;

        /* Info bits for this block (for shortening detection) */
        int infoThisBlock = max_info_bits - blk * k;
        if (infoThisBlock > k) infoThisBlock = k;
        if (infoThisBlock < 0) infoThisBlock = 0;

        /* Decode this block */
        char blockInfo[1944]; /* max k is 1620, but use n for safety */
        decode_block(&graph, llr + offset, blockInfo, max_iter,
                     codedThisBlock, infoThisBlock);

        /* Copy info bits to output, respecting buffer limit */
        for (int i = 0; i < k && infoIdx < max_info_bits; i++)
        {
            infoout[infoIdx++] = blockInfo[i];
        }

        /* Output full codeword hard decisions for hardpoints update */
        if (cw_hard)
        {
            for (int i = 0; i < codedThisBlock; i++)
            {
                cw_hard[offset + i] = (graph.varBelief[i] < 0.0f) ? 1 : 0;
            }
        }
    }

    graph_free(&graph);
    return 0;
}
