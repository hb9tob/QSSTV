/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 * 
 * Adapted for ham sstv use Ties Bos - PA0MBO
 *
 *
 * Description:
 *	Multi-level-channel (de)coder (MLC)
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "MLC.h"
#include "LDPCTables.h"

#define NUM_FAC_CELLS 45


/* Implementation *************************************************************/
/******************************************************************************\
* MLC-encoder                                                                  *
\******************************************************************************/
void CMLCEncoder::ProcessDataInternal(CParameter&)
{
	int	i, j;
	int iElementCounter;

	/* Energy dispersal ----------------------------------------------------- */
	/* VSPP is treated as a separate part for energy dispersal */
	EnergyDisp.ProcessData(pvecInputData);


	/* Partitioning of input-stream ----------------------------------------- */
	iElementCounter = 0;

	if (iL[2] == 0)
	{
		/* Standard departitioning */
		/* Protection level A */
		for (j = 0; j < iLevels; j++)
		{
			/* Bits */
			for (i = 0; i < iM[j][0]; i++)
			{
				vecEncInBuffer[j][i] =
					(*pvecInputData)[iElementCounter];

				iElementCounter++;
			}
		}

		/* Protection level B */
		for (j = 0; j < iLevels; j++)
		{
			/* Bits */
			for (i = 0; i < iM[j][1]; i++)
			{
				vecEncInBuffer[j][iM[j][0] + i] =
					(*pvecInputData)[iElementCounter];

				iElementCounter++;
			}
		}
	}
	else
	{
		/* Special partitioning with hierarchical modulation. First set
		   hierarchical bits at the beginning, then append the rest */
		/* Hierarchical frame (always "iM[0][1]"). "iM[0][0]" is always "0" in
		   this case */
		for (i = 0; i < iM[0][1]; i++)
		{
			vecEncInBuffer[0][i] =
				(*pvecInputData)[iElementCounter];

			iElementCounter++;
		}


		/* Protection level A (higher protected part) */
		for (j = 1; j < iLevels; j++)
		{
			/* Bits */
			for (i = 0; i < iM[j][0]; i++)
			{
				vecEncInBuffer[j][i] =
					(*pvecInputData)[iElementCounter];

				iElementCounter++;
			}
		}

		/* Protection level B  (lower protected part) */
		for (j = 1; j < iLevels; j++)
		{
			/* Bits */
			for (i = 0; i < iM[j][1]; i++)
			{
				vecEncInBuffer[j][iM[j][0] + i] =
					(*pvecInputData)[iElementCounter];

				iElementCounter++;
			}
		}
	}


	/* Channel encoder ------------------------------------------------------ */
	if (bUseLDPC)
	{
		/* 6-frame LDPC pipeline with z=81 (n=1944):
		   - Accumulate info bits from 6 frames
		   - Encode N blocks of 1944 coded bits
		   - Prepend PRBS filler (maps to low-freq subcarriers)
		   - Output coded+filler frame by frame */

		/* Store this frame's info bits */
		int infoOfs = iLDPCFrameCount * iInfoBitsPerFrame;
		int idx = 0;
		for (j = 0; j < iLevels; j++)
			for (i = 0; i < iM[j][0] + iM[j][1]; i++)
				vecLDPCInfoAccum[infoOfs + idx++] = vecEncInBuffer[j][i];

		/* Output coded bits for this frame from last encode.
		   Layout: [PRBS filler | LDPC block 0 | LDPC block 1 | ...] */
		int codedOfs = iLDPCFrameCount * iCodedBitsPerFrame;
		idx = 0;
		for (j = 0; j < iLevels; j++)
			for (i = 0; i < iNumEncBits; i++)
				vecEncOutBuffer[j][i] = vecLDPCCodedAll[codedOfs + idx++];

		iLDPCFrameCount++;
		if (iLDPCFrameCount >= iLDPCTotalFrames)
		{
			/* All 6 frames collected — encode N LDPC blocks.
			   Output goes after the filler in vecLDPCCodedAll. */
			int ldpcN = ldpc_n_from_z(iLDPCz);
			CVector<_DECISION> blockIn(ldpc_k_from_z(iLDPCz, 0));
			CVector<_DECISION> blockOut(ldpcN);
			int ldpcK = BICMEncoder.getK();

			int codedIdx = iLDPCFillerBits; /* skip filler area */
			int infoIdx = 0;

			for (int blk = 0; blk < iLDPCNumBlocks; blk++)
			{
				/* Feed info bits to encoder for this block */
				CVector<_DECISION> blkIn(ldpcK);
				CVector<_DECISION> blkOut(ldpcN);
				for (i = 0; i < ldpcK && infoIdx < vecLDPCInfoAccum.Size(); i++)
					blkIn[i] = vecLDPCInfoAccum[infoIdx++];
				for (; i < ldpcK; i++)
					blkIn[i] = 0; /* shortening if needed */

				BICMEncoder.Encode(blkIn, blkOut);

				/* Copy coded bits after filler */
				for (i = 0; i < ldpcN && codedIdx < iTotalCodedBits; i++)
					vecLDPCCodedAll[codedIdx++] = blkOut[i];
			}

			iLDPCFrameCount = 0;
		}
	}
	else
	{
		for (j = 0; j < iLevels; j++)
			ConvEncoder[j].Encode(vecEncInBuffer[j], vecEncOutBuffer[j]);
	}


	/* Bit interleaver ------------------------------------------------------ */
	for (j = 0; j < iLevels; j++)
		if (piInterlSequ[j] != -1)
			BitInterleaver[piInterlSequ[j]].Interleave(vecEncOutBuffer[j]);


	/* QAM mapping ---------------------------------------------------------- */
	QAMMapping.Map(vecEncOutBuffer[0],
				   vecEncOutBuffer[1],
				   vecEncOutBuffer[2],
				   vecEncOutBuffer[3],
				   vecEncOutBuffer[4],
				   vecEncOutBuffer[5], pvecOutputData);
}

void CMLCEncoder::InitInternal(CParameter& TransmParam)
{
	int i;
	int	iNumInBits;

	TransmParam.Lock();
	bUseLDPC = (eChannelType == CT_MSC) && (TransmParam.iFECMode == 1);
	CalculateParam(TransmParam, eChannelType);
	TransmParam.Unlock();

	iNumInBits = iL[0] + iL[1] + iL[2];


	/* Init modules --------------------------------------------------------- */
	/* Energy dispersal */
	EnergyDisp.Init(iNumInBits, iL[2]);

	/* Encoder */
	if (bUseLDPC)
	{
		/* 6-frame LDPC with standard WiFi z=81 (n=1944 per block).
		   N LDPC blocks fit in 6 DRM frames. Remaining space is
		   PRBS filler placed at the START of the coded stream so it
		   maps to low-frequency subcarriers (poor SNR). */
		iLDPCTotalFrames = 6;
		iLDPCFrameCount = 0;
		iLDPCz = 81; /* standard WiFi 802.11n */

		/* Per-frame info and coded bits */
		iInfoBitsPerFrame = 0;
		for (i = 0; i < iLevels; i++)
			iInfoBitsPerFrame += iM[i][0] + iM[i][1];
		iCodedBitsPerFrame = iLevels * iNumEncBits;

		/* Total across 6 frames */
		iTotalCodedBits = iCodedBitsPerFrame * iLDPCTotalFrames;

		/* N blocks of n=1944 that fit */
		int ldpcN = ldpc_n_from_z(iLDPCz); /* 1944 */
		iLDPCNumBlocks = iTotalCodedBits / ldpcN;
		if (iLDPCNumBlocks < 1) iLDPCNumBlocks = 1;
		iLDPCFillerBits = iTotalCodedBits - iLDPCNumBlocks * ldpcN;

		/* Info bits = blocks * k. Adjust iM to match actual capacity. */
		int ldpcK = ldpc_k_from_z(iLDPCz, TransmParam.iLDPCRate);
		iTotalInfoBits = iLDPCNumBlocks * ldpcK;

		/* Recalculate iM per level to match actual LDPC info capacity */
		int infoPerLevel = iTotalInfoBits / (iLDPCTotalFrames * iLevels);
		int infoRemainder = iTotalInfoBits - infoPerLevel * iLDPCTotalFrames * iLevels;
		for (i = 0; i < iLevels; i++)
		{
			iM[i][0] = 0;
			iM[i][1] = infoPerLevel * iLDPCTotalFrames;
		}
		/* Distribute remainder to last level */
		iM[iLevels-1][1] += infoRemainder;

		/* Recalculate iL */
		iL[0] = 0;
		iL[1] = 0;
		for (i = 0; i < iLevels; i++)
			iL[1] += iM[i][1];
		iL[2] = 0;

		/* Update input block size to match new iM */
		iInfoBitsPerFrame = iL[1] / iLDPCTotalFrames;

		BICMEncoder.Init(TransmParam.iLDPCRate, iLDPCz,
						 iLDPCNumBlocks * ldpcK);

		/* Allocate multi-frame buffers */
		vecLDPCInfoAccum.Init(iLDPCNumBlocks * ldpcK);
		vecLDPCCodedAll.Init(iTotalCodedBits);

		/* Pre-fill coded buffer with PRBS for filler positions.
		   Filler is at the START (positions 0..iLDPCFillerBits-1)
		   so it maps to low-frequency subcarriers.
		   LDPC coded data follows at positions iLDPCFillerBits..end */
		if (iLDPCFillerBits > 0)
		{
			uint32_t reg = ~(uint32_t)0;
			for (i = 0; i < iLDPCFillerBits; i++)
			{
				unsigned char bit = ((reg >> 4) ^ (reg >> 8)) & 1;
				reg = (reg << 1) | bit;
				vecLDPCCodedAll[i] = bit;
			}
		}

		/* Recompute iNumInBits for energy dispersal */
		iNumInBits = iL[0] + iL[1] + iL[2];
		EnergyDisp.Init(iNumInBits / iLDPCTotalFrames, 0);
	}
	else
	{
		for (i = 0; i < iLevels; i++)
			ConvEncoder[i].Init(eCodingScheme, eChannelType, iN[0], iN[1],
				iM[i][0], iM[i][1], iCodeRate[i][0], iCodeRate[i][1], i);
	}

	/* Bit interleaver */
	if (eCodingScheme == CS_3_HMMIX)
	{
		BitInterleaver[0].Init(iN[0], iN[1], 13);
		BitInterleaver[1].Init(iN[0], iN[1], 21);
	}
	else
	{
		BitInterleaver[0].Init(2 * iN[0], 2 * iN[1], 13);
		BitInterleaver[1].Init(2 * iN[0], 2 * iN[1], 21);
	}

	/* QAM-mapping */
	QAMMapping.Init(iN_mux, eCodingScheme);


	/* Allocate memory for internal bit-buffers ----------------------------- */
	for (i = 0; i < iLevels; i++)
	{
		vecEncInBuffer[i].Init(iM[i][0] + iM[i][1]);
		vecEncOutBuffer[i].Init(iNumEncBits);
	}

	/* Define block-size for input and output */
	iInputBlockSize = iNumInBits;
	iOutputBlockSize = iN_mux;

}




/******************************************************************************\
* MLC base class                                                               *
\******************************************************************************/
void CMLC::CalculateParam(CParameter& Parameter, int iNewChannelType)
{
	int i;
//	int iMSCDataLenPartA;

	switch (iNewChannelType)
	{
	/* FAC ********************************************************************/
	case CT_FAC:
		eCodingScheme = CS_1_SM;
		iN_mux = NUM_FAC_CELLS;

		iNumEncBits = NUM_FAC_CELLS * 2;

		iLevels = 1;

		/* Code rates for prot.-Level A and B for each level */
		/* Protection Level A */
		iCodeRate[0][0] = 0;

		/* Protection Level B */
		iCodeRate[0][1] = iCodRateCombFDC4SM;

		/* Define interleaver sequence for all levels */
		piInterlSequ = iInterlSequ4SM;


		/* iN: Number of OFDM-cells of each protection level ---------------- */
		iN[0] = 0;
		iN[1] = iN_mux;


		/* iM: Number of bits each level ------------------------------------ */
		iM[0][0] = 0;
		iM[0][1] = NUM_FAC_BITS_PER_BLOCK;


		/* iL: Number of bits each protection level ------------------------- */
		/* Higher protected part */
		iL[0] = 0;

		/* Lower protected part */
		iL[1] = iM[0][1];

		/* Very strong protected part (VSPP) */
		iL[2] = 0;
		break;



	/* MSC ********************************************************************/
	case CT_MSC:
		eCodingScheme = Parameter.eMSCCodingScheme;
		iN_mux = Parameter.CellMappingTable.iNumUsefMSCCellsPerFrame;
                // printf("iN_mux %d \n", iN_mux);

		/* Data length for part A is the sum of all lengths of the streams */
		
//		iMSCDataLenPartA = 0; // for hamversion

/*		iMSCDataLenPartA = Parameter.Stream[0].iLenPartA +
						   Parameter.Stream[1].iLenPartA +
						   Parameter.Stream[2].iLenPartA +
						   Parameter.Stream[3].iLenPartA;  */

		switch (eCodingScheme)
		{
		case CS_1_SM:
			iLevels = 1;

			/* Code rates for prot.-Level A and B for each level */
				/* Protection Level A */
			iCodeRate[0][0] = 0;

			/* Protection Level B */
			iCodeRate[0][1] = iCodRateCombMSC4SM;

			/* Define interleaver sequence for all levels */
			piInterlSequ = iInterlSequ4SM;
			iNumEncBits = iN_mux * 2;

			/* iN: Number of OFDM-cells of each protection level ---------------- */
			iN[0] = 0;
			iN[1] = iN_mux;

			/* iM: Number of bits each level ------------------------------------ */
			iM[0][0] = 0;

			if (Parameter.iFECMode == 1)
			{
				/* LDPC mode: info bits = coded bits * code_rate
				   No tail bits needed (unlike convolutional code).
				   LDPC rate: 0=1/2, 1=2/3, 2=3/4, 3=5/6 */
				static const int rateNum[] = {1, 2, 3, 5};
				static const int rateDen[] = {2, 3, 4, 6};
				int rIdx = Parameter.iLDPCRate;
				if (rIdx < 0) rIdx = 0;
				if (rIdx > 3) rIdx = 3;
				iM[0][1] = (iNumEncBits * rateNum[rIdx]) / rateDen[rIdx];
			}
			else
			{
				/* M_p,2 = RX_p * floor((2 * N_2 - 12) / RY_p) */
				iM[0][1] = iPuncturingPatterns[iCodRateCombMSC4SM][0] *
					(int) ((_REAL) (2 * iN_mux - 12) /
					iPuncturingPatterns[iCodRateCombMSC4SM][1]);
			}

			/* iL: Number of bits each protection level ------------------------- */
			/* Higher protected part */
			iL[0] = 0;

			/* Lower protected part */
			iL[1] = iM[0][1];

			/* Very strong protected part (VSPP) */
			iL[2] = 0;
                      /*   printf("in CalcuPar CS_1_SM iN[0]= %d iN[1]= %d iM[0][0] = %d iM[0][1] = %d\n",
					 iN[0], iN[1], iM[0][0], iM[0][1] );
			 printf("in CalcuPar iL[o]= %d iL[1]= %d iL[2] = %d\n",
					 iL[0], iL[1], iL[1]);  */
			break;

		case CS_2_SM:
			iLevels = 2;

			/* Code rates for prot.-Level A and B for each level */
			for (i = 0; i < 2; i++)
			{
				/* Protection Level A */
				iCodeRate[i][0] = 0 ;  // hamversion
				/* Protection Level B */
				iCodeRate[i][1] =
					iCodRateCombMSC16SM[Parameter.MSCPrLe.iPartB][i];
			}

			/* Define interleaver sequence for all levels */
			piInterlSequ = iInterlSequ16SM;

			iNumEncBits = iN_mux * 2;

			iN[0] = 0;
			if (iN[0] > iN_mux)
				iN[0] = 0;
			iN[1] = iN_mux - iN[0];

			/* iM: Number of bits each level -------------------------------- */
			if (Parameter.iFECMode == 1)
			{
				/* LDPC mode: total info = total coded * rate, split evenly across levels */
				static const int rateNum[] = {1, 2, 3, 5};
				static const int rateDen[] = {2, 3, 4, 6};
				int rIdx = Parameter.iLDPCRate;
				if (rIdx < 0) rIdx = 0;
				if (rIdx > 3) rIdx = 3;
				int totalInfoBits = (iLevels * iNumEncBits * rateNum[rIdx]) / rateDen[rIdx];
				for (i = 0; i < 2; i++)
				{
					iM[i][0] = 0;
					iM[i][1] = totalInfoBits / iLevels;
				}
				/* Give remainder to last level */
				iM[1][1] += totalInfoBits - iLevels * (totalInfoBits / iLevels);
			}
			else
			{
				for (i = 0; i < 2; i++)
				{
					iM[i][0] = 0;
					iM[i][1] =
						iPuncturingPatterns[iCodRateCombMSC16SM[
						Parameter.MSCPrLe.iPartB][i]][0] *
						(int) ((_REAL) (2 * iN[1] - 12) /
						iPuncturingPatterns[iCodRateCombMSC16SM[
						Parameter.MSCPrLe.iPartB][i]][1]);
				}
			}

			/* iL: Number of bits each protection level --------------------- */
			/* Higher protected part */
			iL[0] = iM[0][0] + iM[1][0];

			/* Lower protected part */
			iL[1] = iM[0][1] + iM[1][1];

			/* Very strong protected part (VSPP) */
			iL[2] = 0;
                         /* printf("in CalcuPar CS_2_SM iN[0]= %d iN[1]= %d iM[0][0] = %d iM[0][1] = %d\n",
					 iN[0], iN[1], iM[0][0], iM[0][1] );
			 printf("in CalcuPar iL[o]= %d iL[1]= %d iL[2] = %d\n",
					 iL[0], iL[1], iL[1]);  */
			break;

		case CS_3_SM:
			iLevels = 3;

			/* Code rates for prot.-Level A and B for each level */
			for (i = 0; i < 3; i++)
			{
				/* Protection Level A */
				iCodeRate[i][0] = 0;
				/* Protection Level B */
				iCodeRate[i][1] =
					iCodRateCombMSC64SM[Parameter.MSCPrLe.iPartB][i];
			}

			/* Define interleaver sequence for all levels */
			piInterlSequ = iInterlSequ64SM;

			iNumEncBits = iN_mux * 2;

			iN[0] = 0;
			if (iN[0] > iN_mux)
				iN[0] = 0;
			iN[1] = iN_mux - iN[0];

			/* iM: Number of bits each level -------------------------------- */
			if (Parameter.iFECMode == 1)
			{
				/* LDPC mode: total info = total coded * rate, split evenly across levels */
				static const int rateNum[] = {1, 2, 3, 5};
				static const int rateDen[] = {2, 3, 4, 6};
				int rIdx = Parameter.iLDPCRate;
				if (rIdx < 0) rIdx = 0;
				if (rIdx > 3) rIdx = 3;
				int totalInfoBits = (iLevels * iNumEncBits * rateNum[rIdx]) / rateDen[rIdx];
				for (i = 0; i < 3; i++)
				{
					iM[i][0] = 0;
					iM[i][1] = totalInfoBits / iLevels;
				}
				/* Give remainder to last level */
				iM[2][1] += totalInfoBits - iLevels * (totalInfoBits / iLevels);
			}
			else
			{
				for (i = 0; i < 3; i++)
				{
					iM[i][0] = 0;
					iM[i][1] =
						iPuncturingPatterns[iCodRateCombMSC64SM[
						Parameter.MSCPrLe.iPartB][i]][0] *
						(int) ((_REAL) (2 * iN[1] - 12) /
						iPuncturingPatterns[iCodRateCombMSC64SM[
						Parameter.MSCPrLe.iPartB][i]][1]);
				}
			}

			/* iL: Number of bits each protection level --------------------- */
			/* Higher protected part */
			iL[0] = iM[0][0] + iM[1][0] + iM[2][0];

			/* Lower protected part */
			iL[1] = iM[0][1] + iM[1][1] + iM[2][1];

			/* Very strong protected part (VSPP) */
			iL[2] = 0;
                         /* printf("in CalcuPar CS_3_SM iN[0]= %d iN[1]= %d iM[0][0] = %d iM[0][1] = %d\n",
					 iN[0], iN[1], iM[0][0], iM[0][1] );
			 printf("in CalcuPar iL[o]= %d iL[1]= %d iL[2] = %d\n",
					 iL[0], iL[1], iL[2]); */
			break;


		default:
			break;
		}

		/* Set number of output bits for next module */
		Parameter.SetNumDecodedBitsMSC(iL[0] + iL[1] + iL[2]);

		/* Set total number of bits for hiearchical frame (needed for MSC
		   demultiplexer module) */
		Parameter.SetNumBitsHieraFrTot(iL[2]);
              //  Parameter.DataParam.iPacketLen = 1234;
		break;
	}
}
