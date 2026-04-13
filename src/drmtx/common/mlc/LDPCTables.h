/******************************************************************************\
 * QSSTV - LDPC Tables from IEEE 802.11n-2009
 *
 * Description:
 *   Base parity-check matrices for LDPC codes used in WiFi 802.11n
 *   Block size z=27, codeword length n=648 bits
 *   Rates: 1/2 (k=324), 2/3 (k=432), 3/4 (k=486), 5/6 (k=540)
 *
 *   Each entry is a cyclic shift value (0..z-1) or -1 (zero submatrix).
 *   The expanded H matrix is formed by replacing each entry with a z×z
 *   identity matrix cyclically shifted by the entry value, or a z×z zero
 *   matrix for -1 entries.
 *
 *   Base matrices are from IEEE 802.11n-2009 Tables 20-17..20-20 (z=81),
 *   with shift values taken mod 27 for z=27 expansion. The base matrix
 *   structure (which entries are -1 vs non-negative) is identical across
 *   all expansion factors.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#ifndef LDPC_TABLES_H
#define LDPC_TABLES_H

/* Expansion factor */
#define LDPC_Z 27

/* Codeword length */
#define LDPC_N (LDPC_BASE_COLS * LDPC_Z)  /* 648 */

/* Number of columns in base matrix */
#define LDPC_BASE_COLS 24

/* Number of rates */
#define LDPC_NUM_RATES 4

/* Rate indices */
#define LDPC_RATE_1_2  0
#define LDPC_RATE_2_3  1
#define LDPC_RATE_3_4  2
#define LDPC_RATE_5_6  3

/* Number of base rows per rate */
static const int ldpc_base_rows[LDPC_NUM_RATES] = { 12, 8, 6, 4 };

/* Information bits per rate: k = (24 - base_rows) * z */
static const int ldpc_k[LDPC_NUM_RATES] = { 324, 432, 486, 540 };

/* Parity bits per rate: n-k = base_rows * z */
static const int ldpc_nk[LDPC_NUM_RATES] = { 324, 216, 162, 108 };

/* Code rate as numerator/denominator pairs */
static const int ldpc_rate_num[LDPC_NUM_RATES] = { 1, 2, 3, 5 };
static const int ldpc_rate_den[LDPC_NUM_RATES] = { 2, 3, 4, 6 };

/*
 * Rate 1/2: base matrix 12 rows x 24 columns
 * Shift values from z=81 tables, taken mod 27
 */
static const int ldpc_base_1_2[12][24] = {
    {  3, -1, -1, -1, 23, -1, 11, -1, 23, -1, 25, -1,  1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {  3, -1,  1, -1,  0, -1, -1, -1,  1,  7, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {  3, -1, -1, -1, 24, 10, -1, -1,  2, 14, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1},
    {  8, 26, -1, -1, 26, -1, -1,  3,  8, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1},
    { 13, -1, -1, 20, 12, -1, -1, 22,  1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1},
    {  0, -1, -1, -1,  8, -1, 15, -1, 23, -1, -1,  8, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1},
    { 15, 25, 25, -1, -1, -1,  2, -1, 25, -1, -1, -1,  0, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1},
    { 11, -1, -1, -1, 11,  3, -1, -1, 18, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1},
    { 10, -1, -1, -1, 14, 25, -1, -1,  3, -1, -1,  5, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1},
    { -1, 18, -1, 16,  0, -1, -1, -1, 23,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1},
    {  2,  2, -1,  3,  8, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0},
    { 24, -1,  7, -1,  6, -1, -1,  0, 24, -1, -1, 16,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0}
};

/*
 * Rate 2/3: base matrix 8 rows x 24 columns
 */
static const int ldpc_base_2_3[8][24] = {
    {  7, 21,  4,  9,  2, -1, -1, -1, -1, -1, -1,  8, -1,  2, 17, 25,  1,  0, -1, -1, -1, -1, -1, -1},
    {  2, 20, 23, 20, -1, -1, -1, 10, 24,  4, 13, -1,  7, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1},
    {  1, 21, 14, 10,  7, 14, 11, -1, -1, -1, 23, -1, -1, -1, 21, -1, -1, -1,  0,  0, -1, -1, -1, -1},
    { 21, 11, 16, 24, 22, -1, -1, -1, -1,  5,  9, -1, 15, 18, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1},
    { 13,  2, 26, 25, -1, 25,  8, -1, 20, -1, -1, 17, -1, -1, -1, -1,  0, -1, -1, -1,  0,  0, -1, -1},
    { 15, 23, 10, 10, 22, -1, 21, -1, -1, -1, -1, -1, 14, 23,  2, -1, -1, -1, -1, -1, -1,  0,  0, -1},
    { 12,  0, 14, 20,  1,  7, -1, 13, -1, -1, -1, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0},
    {  4,  8,  7, 10, 24, -1, -1, 11, 24, 24, -1, -1, -1, -1, -1,  4,  1, -1, -1, -1, -1, -1, -1,  0}
};

/*
 * Rate 3/4: base matrix 6 rows x 24 columns
 */
static const int ldpc_base_3_4[6][24] = {
    { 21,  2,  1, 12,  9,  7, -1, -1, -1,  9, 18, 26, -1, -1, -1, 10,  5, 22,  1,  0, -1, -1, -1, -1},
    {  4, 22, 15, 21, 11,  3, -1, -1, -1, 22, 17, 14, 10, 15, -1,  0, -1, -1, -1,  0,  0, -1, -1, -1},
    {  8, 22, 24, 24, 10,  8, 21, -1, 17, 10, -1, -1, -1,  5,  7, -1, -1,  5, -1, -1,  0,  0, -1, -1},
    {  9, 11, 17,  9,  0,  2, 19,  7, 15, -1, -1, -1,  8, -1, -1, -1, 19, 12,  0, -1, -1,  0,  0, -1},
    {  3,  8,  7, 26, 14, 26, -1, 26,  1, -1,  9, -1, 26, -1,  9, -1, 18, -1, -1, -1, -1, -1,  0,  0},
    { 26, 21,  6, 21, 15,  5,  3, 11, -1, -1, -1,  8, -1,  8,  9, 26, -1, -1,  1, -1, -1, -1, -1,  0}
};

/*
 * Rate 5/6: base matrix 4 rows x 24 columns
 */
static const int ldpc_base_5_6[4][24] = {
    { 13, 21, 26, 12,  4, 20,  7,  3, 22, 25, 10,  6, -1, 22, 19,  4, 20, 19, 23, -1,  1,  0, -1, -1},
    { 15,  9, 20,  2, 10, 23,  3, 11,  6, 16, 24, -1, 10, -1, 14,  9, 21,  8,  0,  0, -1,  0,  0, -1},
    { 24, 15,  0, 26, 24, 25, 15,  0, 17, 17, 17,  9, 13,  8, -1,  4, -1,  2, -1, 26,  0, -1,  0,  0},
    { 16,  2,  9, 14, 17,  2,  5, 10, 23, 24, -1, 11,  4, 11, -1, 26,  7, 14,  2, 16, -1, -1, -1,  0}
};

/* Array of pointers to base matrices for indexing by rate */
static const int* ldpc_base_matrix[LDPC_NUM_RATES] = {
    (const int*)ldpc_base_1_2,
    (const int*)ldpc_base_2_3,
    (const int*)ldpc_base_3_4,
    (const int*)ldpc_base_5_6
};

#endif /* LDPC_TABLES_H */
