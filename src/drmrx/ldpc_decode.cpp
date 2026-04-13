/******************************************************************************\
 * QSSTV - LDPC Decoder for QC-LDPC codes (802.11n base matrices)
 *
 * Description:
 *   Normalized min-sum belief propagation decoder with runtime z.
 *   Single codeword per interleaver period.
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

typedef struct {
    int numChecks;
    int numVars;
    int k;
    int z;
    int *checkDegree;
    int **checkToVar;
    float **c2vMsg;
    float *varBelief;
    float *channelLLR;
} ldpc_graph_t;

static void graph_init(ldpc_graph_t *g, int rate, int z)
{
    int baseRows = ldpc_base_rows[rate];
    int baseCols = LDPC_BASE_COLS;
    const int *baseH = ldpc_base_matrix[rate];

    g->z = z;
    g->numChecks = baseRows * z;
    g->numVars = LDPC_BASE_COLS * z;
    g->k = (LDPC_BASE_COLS - baseRows) * z;

    g->checkDegree = (int *)calloc(g->numChecks, sizeof(int));
    g->checkToVar = (int **)calloc(g->numChecks, sizeof(int *));
    g->c2vMsg = (float **)calloc(g->numChecks, sizeof(float *));

    /* Count degrees */
    for (int br = 0; br < baseRows; br++)
    {
        int deg = 0;
        for (int bc = 0; bc < baseCols; bc++)
            if (baseH[br * baseCols + bc] >= 0) deg++;
        for (int sub = 0; sub < z; sub++)
            g->checkDegree[br * z + sub] = deg;
    }

    /* Fill adjacency lists */
    for (int br = 0; br < baseRows; br++)
    {
        int entries[LDPC_BASE_COLS];
        int shifts[LDPC_BASE_COLS];
        int deg = 0;

        for (int bc = 0; bc < baseCols; bc++)
        {
            int val = baseH[br * baseCols + bc];
            if (val >= 0)
            {
                entries[deg] = bc;
                shifts[deg] = val % z; /* mod z for runtime expansion */
                deg++;
            }
        }

        for (int sub = 0; sub < z; sub++)
        {
            int ci = br * z + sub;
            g->checkToVar[ci] = (int *)malloc(deg * sizeof(int));
            g->c2vMsg[ci] = (float *)calloc(deg, sizeof(float));

            for (int d = 0; d < deg; d++)
                g->checkToVar[ci][d] = entries[d] * z + ((sub + shifts[d]) % z);
        }
    }

    g->varBelief = (float *)calloc(g->numVars, sizeof(float));
    g->channelLLR = (float *)calloc(g->numVars, sizeof(float));
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
 * Run min-sum BP decoding on the single codeword.
 * n_coded = actual received coded bits (may be < numVars if frame doesn't fill perfectly)
 * n_info = actual data bits expected (before shortening)
 */
static int decode_block(ldpc_graph_t *g, float *blockLLR, char *infoOut,
                         int max_iter, int n_coded, int n_info)
{
    int n = g->numVars;
    int k = g->k;

    /* Initialize channel LLR:
     *   0..n_coded-1:  received from channel
     *   n_coded..k-1:  shortening (known PRBS, approximate as +LLR_CLAMP)
     *   k..n-1:        parity (all received, no puncturing)
     */
    for (int i = 0; i < n; i++)
    {
        float val;
        if (i < n_coded)
            val = blockLLR[i];
        else if (i < k)
            val = LLR_CLAMP;   /* shortening: known */
        else
            val = 0.0f;        /* should not happen with zero puncturing */
        if (val > LLR_CLAMP) val = LLR_CLAMP;
        if (val < -LLR_CLAMP) val = -LLR_CLAMP;
        g->channelLLR[i] = val;
        g->varBelief[i] = val;
    }

    /* Initialize c2v messages */
    for (int ci = 0; ci < g->numChecks; ci++)
        memset(g->c2vMsg[ci], 0, g->checkDegree[ci] * sizeof(float));

    /* Iterative decoding */
    int syndromeOK = 0;
    int iter;
    for (iter = 0; iter < max_iter; iter++)
    {
        /* Check-node update (normalized min-sum) */
        for (int ci = 0; ci < g->numChecks; ci++)
        {
            int deg = g->checkDegree[ci];
            float v2c[MAX_VN_DEGREE];
            for (int d = 0; d < deg; d++)
            {
                int vi = g->checkToVar[ci][d];
                v2c[d] = g->varBelief[vi] - g->c2vMsg[ci][d];
            }

            for (int d = 0; d < deg; d++)
            {
                float minAbs = 1e20f;
                int signProduct = 0;

                for (int d2 = 0; d2 < deg; d2++)
                {
                    if (d2 == d) continue;
                    float absVal = fabsf(v2c[d2]);
                    if (absVal < minAbs) minAbs = absVal;
                    if (v2c[d2] < 0.0f) signProduct ^= 1;
                }

                float newMsg = MS_SCALE * minAbs;
                if (signProduct) newMsg = -newMsg;

                int vi = g->checkToVar[ci][d];
                g->varBelief[vi] += (newMsg - g->c2vMsg[ci][d]);
                g->c2vMsg[ci][d] = newMsg;
            }
        }

        /* Check syndrome */
        syndromeOK = 1;
        for (int ci = 0; ci < g->numChecks; ci++)
        {
            int deg = g->checkDegree[ci];
            int parity = 0;
            for (int d = 0; d < deg; d++)
            {
                int vi = g->checkToVar[ci][d];
                if (g->varBelief[vi] < 0.0f) parity ^= 1;
            }
            if (parity) { syndromeOK = 0; break; }
        }

        if (syndromeOK)
            break;
    }

    /* Extract info bits (first k positions, but only n_info actual data) */
    int outBits = (n_info < k) ? n_info : k;
    for (int i = 0; i < outBits; i++)
        infoOut[i] = (g->varBelief[i] < 0.0f) ? 1 : 0;

    return syndromeOK ? iter : -max_iter;
}

int ldpc_decode(float *llr, int n_coded_bits, int ldpc_rate, int z,
                char *infoout, int max_iter, int max_info_bits)
{
    if (!llr || !infoout)
        return ENOMEM;

    if (ldpc_rate < 0 || ldpc_rate >= LDPC_NUM_RATES)
        return EINVAL;

    if (z <= 0)
        return EINVAL;

    /* Build graph for this rate and z */
    ldpc_graph_t graph;
    graph_init(&graph, ldpc_rate, z);

    /* Single block decode — no puncturing, no multi-block */
    int result = decode_block(&graph, llr, infoout, max_iter, n_coded_bits, max_info_bits);

    graph_free(&graph);
    return result;
}
