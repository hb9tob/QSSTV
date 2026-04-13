
/* 
*  changed filename to msdhardmsc.c
*  and added new interface to accomodate
*  own C-language interface instead of 
*  Matlab interface
*
*  almost fully copied  from msd_hard.c
*  by Torsten Schorr Kaiserslautern 2004
*
*  Author of changes M.Bos - PA0MBO
*  Date Feb 21st 2009
*/



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

/* #include "viterbi_decode.h" */
#include "ldpc_decode.h"
#include "../drmtx/common/mlc/LDPCTables.h"
#include "msd_hard_sdc.h"

/* LDPC mode flags (set by channeldecode.cpp from FAC) */
extern int ldpc_mode_flag;
extern int ldpc_rate_index;

/* 6-frame LDPC state */
static float bicm_llr_accum[120000]; /* LLRs accumulated across 6 frames */
static char  ldpc_decoded_info[60000]; /* decoded info bits for 6 frames */
static int   ldpc_decoded_valid = 0;   /* 1 = ldpc_decoded_info has valid data */
static int   ldpc_rx_sf_parity = 0;   /* superframe parity tracker (0/1) */
static int   ldpc_rx_last_identity = -1; /* to detect superframe boundary */

/* Reset LDPC accumulation state (called when sync is lost) */
void ldpc_rx_reset(void)
{
  ldpc_decoded_valid = 0;
  ldpc_rx_sf_parity = 0;
  ldpc_rx_last_identity = -1;
  memset(bicm_llr_accum, 0, sizeof(bicm_llr_accum));
}

#define ITER_BREAK
#define CONSIDERING_SNR

#ifdef CONSIDERING_SNR

#define ARG_INDEX_OFFSET 1
#define NARGS_RHS_STR "9"
#define NARGS_RHS 9

#else /*  */

#define ARG_INDEX_OFFSET 0
#define NARGS_RHS_STR "8"
#define NARGS_RHS 8

#endif /*  */
#define STATES 64
#define PROGNAME "msd_hard"

#define PRBS_INIT(reg) reg = 511;
#define PRBS_BIT(reg) ((reg ^ (reg >> 4)) & 0x1)
#define PRBS_SHIFT(reg) reg = (((reg ^ (reg >> 4)) & 0x1) << 8) | (reg >> 1)
int viterbi_decode(float *, int, int, signed char *, signed char *,
		   signed char *, char *, char *, int, int *, int, int,
		   char *);
int msdhardmsc(double *received_real, double *received_imag, int Lrxdata,
	       double *snr, int N1, double *L, int rowdimL, int coldimL,
	       int Lvspp, int *Deinterleaver, int *PL, int maxiter,
	       int SDCorMSC, /*@out@ */ double *SPPhard,
	       /*@out@ */ double *VSPPhard, double *iterations,
	       double *calc_variance, double *noise_signal)
{
  double *received, *first_received, *L1, *L2, L1_real[10], *L2_real,
    *L1_imag, *L2_imag;
  double *PL1, *PL2, PL1_real[10], *PL2_real, *PL1_imag, *PL2_imag,
    *output_ptr, L_dummy[3] = { 0, 0, 0 };
  float *metric_real, *metric_imag, *metric, *first_metric, closest_one,
    closest_zero, sample, *llr, dist;
  double variance;
  char *memory_ptr, *viterbi_mem, *msd_mem, *hardpoints, *hardpoints_ptr,
    *lastiter, *infoout[3];
  int m, n, N, no_of_levels, iteration, diff;
  int sample_index, rp_real[3], rp_imag[3], *rp, level, subset_point,
    no_of_bits, error, msd_mem_size, viterbi_mem_size;
  int PRBS_reg;
  int HMmix = 0, HMsym = 0;
  int i;
  static float bicm_llr[12000]; /* BICM: de-interleaved LLRs for all levels combined */
  static int msc_call_count = 0;
  if (msc_call_count++ % 50 == 0)
    fprintf(stderr, "MSC called #%d ldpc=%d\n", msc_call_count, ldpc_mode_flag);

#ifdef CONSIDERING_SNR
  double *signal_to_noise_ratio;
  float SNR;


#endif /*  */


/*  new interface to C-language */
  signal_to_noise_ratio = snr;
  no_of_levels = rowdimL;
  for (i = 0; i < rowdimL * coldimL; i++)

    {
      L1_real[i] = L[i];

      /* debugging  
         printf("L[%d] = %g , L1_real[%d] = %g\n", i, L[i], i,  L1_real[i]);  */
    }
  L2_real = L1_real + no_of_levels;
  L1_imag = L_dummy;
  L2_imag = L_dummy;
  for (i = 0; i < coldimL * rowdimL; i++)	/* pa0mbo 4 niet gebruiken later */

    {
      PL1_real[i] = (double) PL[i];

      /* printf("PL1_real[%d] = %g \n", i , PL1_real[i]);   */
    } PL2_real = PL1_real + no_of_levels;
  PL1_imag = PL2_real + no_of_levels;
  PL2_imag = PL1_imag + no_of_levels;	/* pa0mbo will not be OK has to be checked !! will do for the moment */

  /* debugging  
     printf("PL1_real[0]= %g, PL1_real[1]= %g, PL2_real[0]= %g, PL2_real[1]= %g, PL1_imag[0]= %g, PL2_imag[0]= %g\n",
     PL1_real[0], PL1_real[1], PL2_real[0], PL2_real[1],
     PL1_imag[0], PL2_imag[0]);   */
  SDCorMSC = ((0 - SDCorMSC) != 0);

  /* debugging 
     printf("SDCorMSC = %d\n", SDCorMSC);   */
  if (Lrxdata < 20)

    {
      printf("msdhardmsc: length rxdata should be >= 20\n");
      exit(1);
    }
  N = Lrxdata;
  if (N < 20)
    {
      printf("msdhardmsc: N  has to be >= 20!\n");
      exit(1);
    }
  if ((N1 < 0) || (N1 > N - 20))
    {
      printf("msdhardmsc: N1 has to be >= 0!\n");
      exit(1);
    }
  if (Lvspp < 0)
    {
      printf("msdhardmsc: Lvspp has to be >= 0!\n");
      exit(1);
    }
  if (maxiter < 0)
    {
      printf("msdhardmsc: maxiter must not be negativ.");
      exit(1);
    }
  if (HMmix && (Lvspp == 0))
    {
      printf("msdhardmsc:  HMmix requires Lvspp > 0.");
      exit(1);
    }

  /* printf("start mem alloc \n ");
     printf("N= %d, no_of_levels= %d\n", N, no_of_levels);  */

  /* memory allocation and initialization: */
  no_of_bits = 0;
  for (level = 0; level < no_of_levels; level++)
    {
      no_of_bits +=
	(int) L1_real[level] + (int) L2_real[level] + 6 +
	(int) L1_imag[level] + (int) L2_imag[level] + 6;

      /* printf(" --- level %d L1real %d L2real %d L1imga %d L2imag %d\n",
         level, (int)L1_real[level], (int)L2_real[level], (int)L1_imag[level], (int)L2_imag[level]);    */
    } msd_mem_size =
    2 * N * sizeof(float) + 2 * N * sizeof(char) + 2 * N * sizeof(char) +
    no_of_bits * sizeof(char);
  viterbi_mem_size =
    STATES * sizeof(float) + STATES * sizeof(float) +
    2 * N * STATES * sizeof(char);

  /* printf("msdhardmsc: viterbi_mem_size is %d STATES is %d\n", viterbi_mem_size, STATES);   */
  if (received_imag == NULL)
    {
      memory_ptr =
	(char *) malloc(viterbi_mem_size + msd_mem_size + N * sizeof(double) +
			2);
      received_imag =
	(double *) (memory_ptr + viterbi_mem_size + msd_mem_size);
      memset(received_imag, 0, N * sizeof(double));
    }
  else
    {
      memory_ptr = (char *) malloc(viterbi_mem_size + msd_mem_size + 2);

      /*        printf("msdhardmsc: debugging memory_ptr alloc succeeded viterbi_size = %d mds_size=%d  end addr is %x\n",
         viterbi_mem_size, msd_mem_size, memory_ptr+ viterbi_mem_size+msd_mem_size);   */
      if (memory_ptr == NULL)

	{
	  printf("msdhardmsc: cannot malloc for memory_ptr\n");
	  exit(1);
	}
    }
  if (!memory_ptr)
    {
      printf("Failed memory request!\n");
      exit(1);
    }
  viterbi_mem = memory_ptr;
  msd_mem = memory_ptr + viterbi_mem_size;
  llr = (float *) msd_mem;
  hardpoints = (char *) (msd_mem + 2 * N * sizeof(float));
  lastiter =
    (char *) (msd_mem + 2 * N * sizeof(float) + 2 * N * sizeof(char) + 2);
  infoout[0] =
    (char *) (msd_mem + 2 * N * sizeof(float) + 2 * N * sizeof(char) + 2 +
	      2 * N * sizeof(char));
  infoout[1] = 0;
  for (m = 1; m < no_of_levels; m++)
    {
      infoout[m] =
	infoout[m - 1] + (int) L1_real[m - 1] + (int) L2_real[m - 1] + 6 +
	(int) L1_imag[m - 1] + (int) L2_imag[m - 1] + 6;

      /* debugging pa0mbo 
         printf("infoout[%d] = %p \n", m, infoout[m]);  */
    } memset(hardpoints, 0, 2 * N * sizeof(char));

  /* choosing partitioning type: */
  if (no_of_levels == 3)
    {
      if ((Lvspp != 0) && HMmix)
	{			/* HMmix 64-QAM */
	  metric_real = partitioning[1];
	  metric_imag = partitioning[0];
	  rp_real[0] =
	    (N - 12) -
	    RY[(int) PL2_real[0]] * ((N - 12) / RY[(int) PL2_real[0]]);
	  rp_real[1] =
	    ((N - N1) - 12) -
	    RY[(int) PL2_real[1]] * (((N - N1) - 12) / RY[(int) PL2_real[1]]);
	  rp_real[2] =
	    ((N - N1) - 12) -
	    RY[(int) PL2_real[2]] * (((N - N1) - 12) / RY[(int) PL2_real[2]]);
	  rp_imag[0] =
	    ((N - N1) - 12) -
	    RY[(int) PL2_imag[0]] * (((N - N1) - 12) / RY[(int) PL2_imag[0]]);
	  rp_imag[1] =
	    ((N - N1) - 12) -
	    RY[(int) PL2_imag[1]] * (((N - N1) - 12) / RY[(int) PL2_imag[1]]);
	  rp_imag[2] =
	    ((N - N1) - 12) -
	    RY[(int) PL2_imag[2]] * (((N - N1) - 12) / RY[(int) PL2_imag[2]]);
	}
      else if (Lvspp != 0)
	{			/* HMsym 64-QAM */
	  HMsym = 1;
	  metric_real = partitioning[1];
	  metric_imag = partitioning[1];
	  rp_real[0] =
	    (2 * N - 12) -
	    RY[(int) PL2_real[0]] * ((2 * N - 12) / RY[(int) PL2_real[0]]);
	  rp_real[1] =
	    (2 * (N - N1) - 12) -
	    RY[(int) PL2_real[1]] * ((2 * (N - N1) - 12) /
				     RY[(int) PL2_real[1]]);
	  rp_real[2] =
	    (2 * (N - N1) - 12) -
	    RY[(int) PL2_real[2]] * ((2 * (N - N1) - 12) /
				     RY[(int) PL2_real[2]]);
	}
      else
	{			/* SM 64-QAM */
	  metric_real = partitioning[0];
	  metric_imag = partitioning[0];
	  rp_real[0] =
	    (2 * (N - N1) - 12) -
	    RY[(int) PL2_real[0]] * ((2 * (N - N1) - 12) /
				     RY[(int) PL2_real[0]]);
	  rp_real[1] =
	    (2 * (N - N1) - 12) -
	    RY[(int) PL2_real[1]] * ((2 * (N - N1) - 12) /
				     RY[(int) PL2_real[1]]);
	  rp_real[2] =
	    (2 * (N - N1) - 12) -
	    RY[(int) PL2_real[2]] * ((2 * (N - N1) - 12) /
				     RY[(int) PL2_real[2]]);
    }}
  else if (no_of_levels == 2)
    {				/* SM 16-QAM */
      rp_real[0] =
	(2 * (N - N1) - 12) -
	RY[(int) PL2_real[0]] * ((2 * (N - N1) - 12) / RY[(int) PL2_real[0]]);
      rp_real[1] =
	(2 * (N - N1) - 12) -
	RY[(int) PL2_real[1]] * ((2 * (N - N1) - 12) / RY[(int) PL2_real[1]]);
      metric_real = partitioning[2];
      metric_imag = partitioning[2];

      /* printf("SM 16 QAM\n"); */
    }
  else
    {				/* SM 4-QAM */
      rp_real[0] =
	(2 * (N - N1) - 12) -
	RY[(int) PL2_real[0]] * ((2 * (N - N1) - 12) / RY[(int) PL2_real[0]]);
      metric_real = partitioning[3];
      metric_imag = partitioning[3];
  } if (!SDCorMSC)
    {
      rp_real[0] = -12;
      rp_real[1] = -12;
      rp_real[2] = -12;
    }
  if (Lvspp != 0)
    {
      L1_real[0] = 0;
      L2_real[0] = (double) Lvspp;
    }

  /* debugging pa0mbo  
     printf("=== voor  viterbi \n");
     for (i=0; i < 2*N ; i++)
     {
     printf("hardpoints[%d] = %d \n",i,  hardpoints[i]);
     }  */

  /* Multi-Stage Decoding: */

  /* first decoding: */
  PL1 = PL1_real;
  PL2 = PL2_real;
  L1 = L1_real;
  L2 = L2_real;
  rp = rp_real;
  first_metric = metric_real;
  first_received = received_real;
  hardpoints_ptr = hardpoints;

  /* debugging  
     printf("msdhardmsc: at start first decoding\n");
     printf("PL1[0] = %g, PL2[0]= %g, L1_real[0]= %g, L2_real[0]=%g, L1[0]=%g, L2[0]= %g, rp[0]= %d\n",
     PL1[0], PL2[0], L1_real[0], L2_real[0], L1[0], L2[0], rp[0]);   */
  for (n = 0; n <= HMmix; n++)

    {
      for (level = 0; level < no_of_levels; level++)

	{
	  metric = first_metric;
	  received = first_received;
	  for (m = 0; m < 2 - HMmix; m++)

	    {			/* for real and imaginary part */
	      for (sample_index = m; sample_index < (2 - HMmix) * N;
		   sample_index += 2 - HMmix)

		{
		  sample = (float) received[sample_index >> (1 - HMmix)];	/* extract real or imaginary part respectively */
		  closest_zero =
		    fabs(sample - metric[(int) hardpoints_ptr[sample_index]]);

		  /* printf("msdhardmsc: index= %d  sample = %g metric = %g closest_zero = %g \n",
		     sample_index, sample,  metric[hardpoints_ptr[sample_index]], closest_zero);   pa0mbo */
		  for (subset_point = (0x1 << (level + 1));
		       subset_point < (0x1 << no_of_levels);
		       subset_point += (0x1 << (level + 1)))

		    {
		      dist =
			fabs(sample -
			     metric[hardpoints_ptr[sample_index] +
				    subset_point]);
		      if (dist < closest_zero)

			{
			  closest_zero = dist;
			}
		    }
		  closest_one =
		    fabs(sample -
			 metric[hardpoints_ptr[sample_index] +
				(0x1 << level)]);

		  /* printf("closest_one %g\n", closest_one);  pa0mbo */
		  for (subset_point = (0x3 << level);
		       subset_point < (0x1 << no_of_levels);
		       subset_point += (0x1 << (level + 1)))

		    {
		      dist =
			fabs(sample -
			     metric[hardpoints_ptr[sample_index] +
				    subset_point]);
		      if (dist < closest_one)

			{
			  closest_one = dist;
			}
		    }

		  /* printf("final closest_zero=%g closest_one=%g\n", closest_zero, closest_one);  pa0mbo */

#ifdef CONSIDERING_SNR
		  SNR =
		    (float) signal_to_noise_ratio[sample_index >>
						  (1 - HMmix)];
		  llr[sample_index] = (closest_zero - closest_one) * SNR;

		  /* printf("llr[%d] = %g\n", sample_index, llr[sample_index]);   */

		  /* llr[sample_index] = (closest_zero*closest_zero - closest_one*closest_one) * SNR * SNR; */
#else /*  */
		  llr[sample_index] = (closest_zero - closest_one);

		  /* llr[sample_index] = (closest_zero*closest_zero - closest_one*closest_one); */
#endif /*  */
		}		/* end loop sample_index */
	      metric = metric_imag;
	      received = received_imag;
	    }			/* end loop m */

	  /* printf(" level %d HMsym %d HMmix %d N1 %d n %d\n", level, HMsym, HMmix, N1, n);  
	     printf("msdhardmsc: (level || (!HMsym ..) = %d\n", (level || (!HMsym && (n || !HMmix)))* (2 - HMmix)*N1 ); 
	     printf("eerste viter PL1[0] %g PL2[0] %g L1[0] %g L2[0] %g L1_real[0] %g L2_real[0] %g rp[0] %d\n",
	     PL1[0], PL2[0], L1[0], L2[0], L1_real[0], L2_real[0], rp[0]);  */
	  /* for (i=0; i < 17 ; i++)
	     printf("puncturing[6][%d] = %d\n", i, puncturing[6][i]);  
	     printf("level %d rp[level] %d \n", level, rp[level]);
	     printf("tailpuncturing[... ] = %p\n", tailpuncturing[rp[level]]);
	     for (i=0; i < 13 ; i++)
	     printf("inhoud is %d ", tailpuncturing[rp[level]][i]);  */
	  if (ldpc_mode_flag)
	    {
	      /* WiFi-style BICM: store de-interleaved LLRs per level,
	         decode all together after the level loop */
	      int n_coded = (2 - HMmix) * N;
	      int *deint = Deinterleaver + n_coded * level;
	      for (sample_index = 0; sample_index < n_coded; sample_index++)
		bicm_llr[level * n_coded + sample_index] = -llr[deint[sample_index]];
	      /* Update hardpoints from raw LLR signs.
	       * MSD convention: positive llr → bit 1 (same as viterbi_decode
	       * which uses -llr for 0-metric, +llr for 1-metric) */
	      for (sample_index = 0; sample_index < n_coded; sample_index++)
		{
		  int pos = deint[sample_index];
		  int bit = (llr[pos] > 0.0f) ? 1 : 0;
		  hardpoints_ptr[pos] =
		    (hardpoints_ptr[pos] & ~(0x1 << level)) |
		    (bit << level);
		}
	      error = 0;
	    }
	  else
	    {
	      error = viterbi_decode(llr, (2 - HMmix) * N,
				     (level
				      || (!HMsym
					  && (n
					      || !HMmix))) * (2 - HMmix) * N1,
				     puncturing[(int) PL1[level]],
				     puncturing[(int) PL2[level]],
				     tailpuncturing[rp[level]],
				     infoout[level] + n * ((int) L1_real[level] +
							   (int) L2_real[level] +
							   6), hardpoints_ptr,
				     level,
				     Deinterleaver + (2 - HMmix) * N * level,
				     (int) L1[level] + (int) L2[level] + 6,
				     rp[level] + 12, viterbi_mem);
	    }

	  if (error)
	    {
	      free(memory_ptr);
	      printf("msdhardmsc: Error in Viterbi decoder");
	      return 1;
	    }
	}			/* end loop level */

      /* 6-frame LDPC pipeline: accumulate LLRs, decode at frame 5,
	 output decoded data frame-by-frame from buffer every frame. */
      if (ldpc_mode_flag)
	{
	  int n_coded = (2 - HMmix) * N;
	  int this_frame_coded = no_of_levels * n_coded;
	  /* Compute LDPC position using FAC identity + superframe parity,
	     same formula as TX: pos = sfParity * 3 + identity.
	     Toggle parity when identity wraps (identity 0 after identity 2). */
	  extern int drm_fac_identity;
	  int fac_id = drm_fac_identity; /* 0, 1, or 2 within superframe */
	  if (fac_id == 0 && ldpc_rx_last_identity == 2)
	    ldpc_rx_sf_parity ^= 1; /* new superframe → toggle parity */
	  ldpc_rx_last_identity = fac_id;
	  int ldpc_pos = (ldpc_rx_sf_parity * 3 + fac_id + 1) % 6; /* 0..5, +1 for pipeline delay */

	  fprintf(stderr, "LDPC-RX pos=%d fac=%d sfp=%d\n", ldpc_pos, fac_id, ldpc_rx_sf_parity);

	  /* Accumulate this frame's LLRs */
	  int accum_offset = ldpc_pos * this_frame_coded;
	  for (sample_index = 0; sample_index < this_frame_coded; sample_index++)
	    bicm_llr_accum[accum_offset + sample_index] = bicm_llr[sample_index];

	  /* OUTPUT first, THEN decode — same principle as TX.
	     At pos=5 the old buffer still holds the previous cycle's data,
	     so we must output before overwriting with the new decode. */
	  /* WiFi-style: MSC payload = LDPC capacity / 6 (same as TX) */
	  int ldpc_info_cap = (6 * this_frame_coded / ldpc_n_from_z(81))
			      * ldpc_k_from_z(81, ldpc_rate_index);
	  int total_info_per_frame = ldpc_info_cap / 6;

	  /* Override L1/L2 for energy dispersal (single-level: all in L2) */
	  L1_real[0] = 0;
	  L2_real[0] = total_info_per_frame;
	  for (level = 1; level < no_of_levels; level++)
	    { L1_real[level] = 0; L2_real[level] = 0; }

	  if (ldpc_decoded_valid)
	    {
	      int out_idx = ldpc_pos * total_info_per_frame;
	      for (level = 0; level < no_of_levels; level++)
		{
		  int info_bits = (int) L1_real[level] + (int) L2_real[level];
		  for (sample_index = 0; sample_index < info_bits; sample_index++)
		    infoout[level][sample_index] = ldpc_decoded_info[out_idx++];
		}
	    }
	  else
	    {
	      /* No decoded data yet (warm-up): fill with PRBS (X^9+X^5+1)
		 to avoid fortuitous convergence and energy concentration.
		 Same PRBS seed as TX run-in so energy dispersal stays coherent. */
	      uint32_t prbs_reg = ~(uint32_t)0;
	      for (level = 0; level < no_of_levels; level++)
		{
		  int info_bits = (int) L1_real[level] + (int) L2_real[level];
		  for (sample_index = 0; sample_index < info_bits; sample_index++)
		    {
		      unsigned char bit = ((prbs_reg >> 4) ^ (prbs_reg >> 8)) & 1;
		      prbs_reg = (prbs_reg << 1) | bit;
		      infoout[level][sample_index] = bit;
		    }
		}
	    }

	  /* Now decode AFTER output — new data for NEXT cycle's output */
	  if (ldpc_pos == 5)
	    {
	      int total_coded_6f = 6 * this_frame_coded;
	      int ldpc_z = 81;
	      int ldpc_n = ldpc_n_from_z(ldpc_z);
	      int ldpc_k = ldpc_k_from_z(ldpc_z, ldpc_rate_index);
	      int num_blocks = total_coded_6f / ldpc_n;
	      int filler_bits = total_coded_6f - num_blocks * ldpc_n;

	      int info_idx = 0;
	      int converged_cnt = 0;
	      for (int blk = 0; blk < num_blocks; blk++)
		{
		  float *block_llr = bicm_llr_accum + filler_bits + blk * ldpc_n;
		  char block_info[1944];
		  int rc = ldpc_decode(block_llr, ldpc_n, ldpc_rate_index, ldpc_z,
				       block_info, 50, ldpc_k);
		  if (rc >= 0) converged_cnt++;
		  for (int bi = 0; bi < ldpc_k && info_idx < 60000; bi++)
		    ldpc_decoded_info[info_idx++] = block_info[bi];
		}
	      fprintf(stderr, "LDPC-DECODE: %d/%d converged, filler=%d k=%d info=%d\n",
		      converged_cnt, num_blocks, filler_bits, ldpc_k, info_idx);
	      ldpc_decoded_valid = 1;
	    }
	}
      PL1 = PL1_imag;
      PL2 = PL2_imag;
      L1 = L1_imag;
      L2 = L2_imag;
      rp = rp_imag;
      first_metric = metric_imag;
      first_received = received_imag;
      hardpoints_ptr = hardpoints + N;
    }				/* end loop over n */
  diff = 1;
  iteration = 0;

  /* iterations: LDPC converges in a single pass (first decoding above),
     so MSD iterations are skipped — the simplified LLR formula used in
     iterations actually degrades LDPC performance. */
  while (!ldpc_mode_flag && iteration < maxiter)

    {
      PL1 = PL1_real;
      PL2 = PL2_real;
      L1 = L1_real;
      L2 = L2_real;
      rp = rp_real;
      first_metric = metric_real;
      first_received = received_real;
      hardpoints_ptr = hardpoints;

#ifdef ITER_BREAK
      memcpy(lastiter, hardpoints, 2 * N);

#endif /*  */
      for (n = 0; n <= HMmix; n++)

	{
	  for (level = 0; level < no_of_levels; level++)

	    {
	      metric = first_metric;
	      received = first_received;
	      for (m = 0; m < 2 - HMmix; m++)

		{		/* for real and imaginary part */
		  for (sample_index = m; sample_index < (2 - HMmix) * N;
		       sample_index += 2 - HMmix)

		    {
		      sample = (float) received[sample_index >> (1 - HMmix)];	/* extract real or imaginary part respectively */
		      closest_zero =
			fabs(sample -
			     metric[hardpoints_ptr[sample_index] &
				    ~(0x1 << level)]);
		      closest_one =
			fabs(sample -
			     metric[hardpoints_ptr[sample_index] |
				    (0x1 << level)]);

#ifdef CONSIDERING_SNR
		      SNR =
			(float) signal_to_noise_ratio[sample_index >>
						      (1 - HMmix)];
		      llr[sample_index] = (closest_zero - closest_one) * SNR;

		      /* llr[sample_index] = (closest_zero*closest_zero - closest_one*closest_one) * SNR * SNR; */
#else /*  */
		      llr[sample_index] = (closest_zero - closest_one);

		      /* llr[sample_index] = (closest_zero*closest_zero - closest_one*closest_one); */
#endif /*  */
		    }		/* end loop over sample_index */
		  metric = metric_imag;
		  received = received_imag;
		}		/* end loop over m */

	      /* printf("Tweede viterbi PL1[0] %g PL2[0] %g L1[0] %g L2[0] %g L1_real[0] %g L2_real[0] %g rp[0] %d\n",
	         PL1[0], PL2[0], L1[0], L2[0], L1_real[0], L2_real[0], rp[0]);   */
	      if (ldpc_mode_flag)
		{
		  /* This branch should not be reached (MSD iterations disabled
		     for LDPC), but kept for safety */
		  int n_coded = (2 - HMmix) * N;
		  int info_bits = (int) L1_real[level] + (int) L2_real[level];
		  int *deint = Deinterleaver + n_coded * level;
		  for (sample_index = 0; sample_index < info_bits; sample_index++)
		    {
		      float val = llr[deint[sample_index]];
		      infoout[level][n * info_bits + sample_index] = (val < 0.0f) ? 1 : 0;
		    }
		  for (sample_index = 0; sample_index < n_coded; sample_index++)
		    {
		      int pos = deint[sample_index];
		      int bit = (llr[pos] > 0.0f) ? 1 : 0;
		      hardpoints_ptr[pos] =
			(hardpoints_ptr[pos] & ~(0x1 << level)) |
			(bit << level);
		    }
		  error = 0;
		}
	      else
		{
		  error = viterbi_decode(llr,
					 (2 - HMmix) * N,
					 (level
					  || (!HMsym
					      && (n
						  || !HMmix))) * (2 - HMmix) * N1,
					 puncturing[(int) PL1[level]],
					 puncturing[(int) PL2[level]],
					 tailpuncturing[rp[level]],
					 infoout[level] +
					 n * ((int) L1_real[level] +
					      (int) L2_real[level] + 6),
					 hardpoints_ptr, level,
					 Deinterleaver + (2 - HMmix) * N * level,
					 (int) L1[level] + (int) L2[level] + 6,
					 rp[level] + 12, viterbi_mem);
		}
	      if (error)

		{
		  free(memory_ptr);
		  printf("msdhardmsc: Error in channel decoder");
		  return 1;
		}

#ifdef ITER_BREAK
	      if (level == 0)

		{
		  diff = 0;
      for (sample_index = 0;sample_index <(int)((2 - HMmix) * N * sizeof(char) / sizeof(int));sample_index++)
        {
		      diff +=
			(((int *) hardpoints)[sample_index] ^
			 ((int *) lastiter)[sample_index]) != 0;
		    }
		  /*diff = memcmp (lastiter,hardpoints,2 * N); */
		  if (!diff)

		    {
		      break;
		    }
		}

#endif /*  */
	    }			/* for (level = 0; level < no_of_levels; level++) */
	  PL1 = PL1_imag;
	  PL2 = PL2_imag;
	  L1 = L1_imag;
	  L2 = L2_imag;
	  rp = rp_imag;
	  first_metric = metric_imag;
	  first_received = received_imag;
	  hardpoints_ptr = hardpoints + N;
	}			/* for (n = 0; n <= HMmix; n++) */

#ifdef ITER_BREAK
      if (!diff)

	{
	  break;
	}

#endif /*  */
      iteration++;
    }				/* while (iteration < maxiter) */

  /* Energy Dispersal */
  no_of_bits = 0;
  for (level = (Lvspp != 0); level < no_of_levels; level++)
    {
      no_of_bits += (int) L1_real[level] + (int) L2_real[level];
  } for (level = 0; level < no_of_levels; level++)
    {
      no_of_bits += (int) L1_imag[level] + (int) L2_imag[level];
    } output_ptr = SPPhard;
  PRBS_INIT(PRBS_reg);
  n = 0;
  if (HMmix)
    {
      for (m = Lvspp + 6; m < Lvspp + 6 + (int) L1_imag[0]; m++)
	{
	  output_ptr[n++] = (double) (infoout[0][m] ^ PRBS_BIT(PRBS_reg));
	  PRBS_SHIFT(PRBS_reg);
    }}

  /* printf("msdhardmsc: 1e n %d\n", n); */
  for (level = (Lvspp != 0); level < no_of_levels; level++)
    {
      for (m = 0; m < (int) L1_real[level]; m++)
	{
	  output_ptr[n++] = (double) (infoout[level][m] ^ PRBS_BIT(PRBS_reg));
	  PRBS_SHIFT(PRBS_reg);
	}
      /* printf("msdhardmsc: 2a n %d\n", n); */
      for (m = (int) L1_real[level] + (int) L2_real[level] + 6;
	   m <
	   (int) L1_real[level] + (int) L2_real[level] + 6 +
	   (int) L1_imag[level]; m++)
	{
	  output_ptr[n++] = (double) (infoout[level][m] ^ PRBS_BIT(PRBS_reg));
	  PRBS_SHIFT(PRBS_reg);
    }}
  /*  printf("msdhardmsc: 2b n %d\n", n);  */
  if (HMmix)
    {
      for (m = Lvspp + 6 + (int) L1_imag[0];
	   m < Lvspp + 6 + (int) L1_imag[0] + (int) L2_imag[0]; m++)
	{
	  output_ptr[n++] = (double) (infoout[0][m] ^ PRBS_BIT(PRBS_reg));
	  PRBS_SHIFT(PRBS_reg);
    }}

  /* printf("msdhardmsc: 3e n %d\n", n); */
  for (level = (Lvspp != 0); level < no_of_levels; level++)
    {
      for (m = (int) L1_real[level];
	   m < (int) L1_real[level] + (int) L2_real[level]; m++)
	{
	  output_ptr[n++] = (double) (infoout[level][m] ^ PRBS_BIT(PRBS_reg));
	  PRBS_SHIFT(PRBS_reg);
      } for (m =
	       (int) L1_real[level] + (int) L2_real[level] + 6 +
	       (int) L1_imag[level];
	       m <
	       (int) L1_real[level] + (int) L2_real[level] + 6 +
	       (int) L1_imag[level] + (int) L2_imag[level]; m++)
	{
	  output_ptr[n++] = (double) (infoout[level][m] ^ PRBS_BIT(PRBS_reg));
	  PRBS_SHIFT(PRBS_reg);
    }}

/*         printf("msdhardmsc: 4e n %d\n", n);  */
  PRBS_INIT(PRBS_reg);
  if (Lvspp != 0)
    {
      printf
	("msdhardmsc: There is a very strongly protected part, but no variable to put it into!");
    }
  no_of_bits = Lvspp;
  output_ptr = VSPPhard;
  for (m = 0; m < Lvspp; m++)
    {
      output_ptr[m] = (double) (infoout[0][m] ^ PRBS_BIT(PRBS_reg));
      PRBS_SHIFT(PRBS_reg);
    } output_ptr = iterations;
  output_ptr[0] = (double) iteration;
  output_ptr = calc_variance;
  variance = 0.0;
  for (sample_index = 0; sample_index < N; sample_index++)
    {
      sample = (float) received_real[sample_index];	/* extract real part respectively */
      dist =
	(sample - metric_real[(int) hardpoints[(2 - HMmix) * sample_index]]);
      variance += (double) dist *(double) dist;

      sample = (float) received_imag[sample_index];	/* extract imaginary part respectively */
      dist =
	(sample -
	 metric_imag[(int)
		     hardpoints[HMmix * (N - 1) + (2 - HMmix) * sample_index +
				1]]);
      variance += (double) dist *(double) dist;
    } output_ptr[0] = variance / ((double) N);
  output_ptr = noise_signal;
  for (sample_index = 0; sample_index < N; sample_index++)
    {
      sample = (float) received_real[sample_index];	/* extract real part */
      output_ptr[sample_index * 2] =
	(sample - metric_real[(int) hardpoints[(2 - HMmix) * sample_index]]);
      sample = (float) received_imag[sample_index];	/* extract imaginary part */
      output_ptr[2 * sample_index + 1] =
	(sample -
	 metric_imag[(int)
		     hardpoints[HMmix * (N - 1) + (2 - HMmix) * sample_index +
				1]]);
    } free(memory_ptr);
  return n;
}
