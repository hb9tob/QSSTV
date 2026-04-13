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
		/* Single-frame LDPC with dynamic z: collect all levels' info bits,
		   single LDPC encode, distribute coded bits back to per-level
		   buffers for bit interleaving and QAM mapping. */
		CVector<_DECISION> bicmIn(iTotalInfoBits);
		CVector<_DECISION> bicmOut(iTotalCodedBits);
		int idx = 0;
		for (j = 0; j < iLevels; j++)
			for (i = 0; i < iM[j][0] + iM[j][1]; i++)
				bicmIn[idx++] = vecEncInBuffer[j][i];
		BICMEncoder.Encode(bicmIn, bicmOut);
		/* Distribute coded bits back to per-level buffers */
		idx = 0;
		for (j = 0; j < iLevels; j++)
			for (i = 0; i < iNumEncBits; i++)
				vecEncOutBuffer[j][i] = bicmOut[idx++];
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
		/* Single-frame LDPC with dynamic z: n = total coded bits per frame.
		   z = n / 24. One LDPC codeword per DRM frame. */
		iTotalInfoBits = 0;
		for (i = 0; i < iLevels; i++)
			iTotalInfoBits += iM[i][0] + iM[i][1];
		iTotalCodedBits = iLevels * iNumEncBits;

		/* Compute z so that n = iTotalCodedBits */
		iLDPCz = iTotalCodedBits / LDPC_BASE_COLS;
		if (iLDPCz < 1) iLDPCz = 1;

		BICMEncoder.Init(TransmParam.iLDPCRate, iLDPCz, iTotalInfoBits);
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
