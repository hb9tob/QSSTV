/******************************************************************************\
 * QSSTV - LDPC Base Matrices from IEEE 802.11n-2009
 *
 * Description:
 *   Base parity-check matrices for QC-LDPC codes.
 *   The expansion factor z is computed at runtime to match the DRM frame.
 *   n = 24 * z (codeword length), k = (24 - base_rows) * z (info bits).
 *
 *   Shift values in the base matrices are taken mod z at runtime.
 *   Entry -1 means zero sub-matrix.
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

/* Number of columns in base matrix (fixed for all 802.11n codes) */
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

/* Code rate as numerator/denominator pairs */
static const int ldpc_rate_num[LDPC_NUM_RATES] = { 1, 2, 3, 5 };
static const int ldpc_rate_den[LDPC_NUM_RATES] = { 2, 3, 4, 6 };

/*
 * Base matrices from IEEE 802.11n-2009 Tables 20-17..20-20 (z=81 reference).
 * Shift values are taken mod z at runtime for any expansion factor.
 */

/* Rate 1/2: 12 rows x 24 columns */
static const int ldpc_base_1_2[12][24] = {
    { 57, -1, -1, -1, 50, -1, 11, -1, 50, -1, 79, -1,  1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {  3, -1, 28, -1,  0, -1, -1, -1, 55,  7, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 30, -1, -1, -1, 24, 37, -1, -1, 56, 14, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1},
    { 62, 53, -1, -1, 53, -1, -1,  3, 35, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1, -1},
    { 40, -1, -1, 20, 66, -1, -1, 22, 28, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1, -1},
    {  0, -1, -1, -1,  8, -1, 42, -1, 50, -1, -1,  8, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1},
    { 69, 79, 79, -1, -1, -1, 56, -1, 52, -1, -1, -1,  0, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1},
    { 65, -1, -1, -1, 38, 57, -1, -1, 72, -1, 27, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1},
    { 64, -1, -1, -1, 14, 52, -1, -1, 30, -1, -1, 32, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1, -1},
    { -1, 45, -1, 70,  0, -1, -1, -1, 77,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1},
    {  2, 56, -1, 57, 35, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0},
    { 24, -1, 61, -1, 60, -1, -1, 27, 51, -1, -1, 16,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0}
};

/* Rate 2/3: 8 rows x 24 columns */
static const int ldpc_base_2_3[8][24] = {
    { 61, 75,  4, 63, 56, -1, -1, -1, -1, -1, -1,  8, -1,  2, 17, 25,  1,  0, -1, -1, -1, -1, -1, -1},
    { 56, 74, 77, 20, -1, -1, -1, 64, 24,  4, 67, -1,  7, -1, -1, -1, -1,  0,  0, -1, -1, -1, -1, -1},
    { 28, 21, 68, 10,  7, 14, 65, -1, -1, -1, 23, -1, -1, -1, 75, -1, -1, -1,  0,  0, -1, -1, -1, -1},
    { 48, 38, 43, 78, 76, -1, -1, -1, -1,  5, 36, -1, 15, 72, -1, -1, -1, -1, -1,  0,  0, -1, -1, -1},
    { 40,  2, 53, 25, -1, 52, 62, -1, 20, -1, -1, 44, -1, -1, -1, -1,  0, -1, -1, -1,  0,  0, -1, -1},
    { 69, 23, 64, 10, 22, -1, 21, -1, -1, -1, -1, -1, 68, 23, 29, -1, -1, -1, -1, -1, -1,  0,  0, -1},
    { 12,  0, 68, 20, 55, 61, -1, 40, -1, -1, -1, 44, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0},
    { 58,  8, 34, 64, 78, -1, -1, 11, 78, 24, -1, -1, -1, -1, -1, 58,  1, -1, -1, -1, -1, -1, -1,  0}
};

/* Rate 3/4: 6 rows x 24 columns */
static const int ldpc_base_3_4[6][24] = {
    { 48, 29, 28, 39,  9, 61, -1, -1, -1, 63, 45, 80, -1, -1, -1, 37, 32, 22,  1,  0, -1, -1, -1, -1},
    {  4, 49, 42, 48, 11, 30, -1, -1, -1, 49, 17, 41, 37, 15, -1, 54, -1, -1, -1,  0,  0, -1, -1, -1},
    { 35, 76, 78, 51, 37, 35, 21, -1, 17, 64, -1, -1, -1, 59,  7, -1, -1, 32, -1, -1,  0,  0, -1, -1},
    {  9, 65, 44,  9, 54, 56, 73, 34, 42, -1, -1, -1, 35, -1, -1, -1, 46, 39,  0, -1, -1,  0,  0, -1},
    {  3, 62,  7, 80, 68, 26, -1, 80, 55, -1, 36, -1, 26, -1,  9, -1, 72, -1, -1, -1, -1, -1,  0,  0},
    { 26, 75, 33, 21, 69, 59,  3, 38, -1, -1, -1, 35, -1, 62, 36, 26, -1, -1,  1, -1, -1, -1, -1,  0}
};

/* Rate 5/6: 4 rows x 24 columns */
static const int ldpc_base_5_6[4][24] = {
    { 13, 48, 80, 66,  4, 74,  7, 30, 76, 52, 37, 60, -1, 49, 73, 31, 74, 73, 23, -1,  1,  0, -1, -1},
    { 69, 63, 74, 56, 64, 77, 57, 65,  6, 16, 51, -1, 64, -1, 68,  9, 48, 62, 54, 27, -1,  0,  0, -1},
    { 51, 15,  0, 80, 24, 25, 42, 54, 44, 71, 71,  9, 67, 35, -1, 58, -1, 29, -1, 53,  0, -1,  0,  0},
    { 16, 29, 36, 41, 44, 56, 59, 37, 50, 24, -1, 65,  4, 65, -1, 26,  7, 14, 29, 16, -1, -1, -1,  0}
};

/* Array of pointers to base matrices for indexing by rate */
static const int* ldpc_base_matrix[LDPC_NUM_RATES] = {
    (const int*)ldpc_base_1_2,
    (const int*)ldpc_base_2_3,
    (const int*)ldpc_base_3_4,
    (const int*)ldpc_base_5_6
};

/* Helper: compute LDPC parameters for a given z and rate */
static inline int ldpc_n_from_z(int z) { return LDPC_BASE_COLS * z; }
static inline int ldpc_k_from_z(int z, int rate) { return (LDPC_BASE_COLS - ldpc_base_rows[rate]) * z; }
static inline int ldpc_nk_from_z(int z, int rate) { return ldpc_base_rows[rate] * z; }

#endif /* LDPC_TABLES_H */
