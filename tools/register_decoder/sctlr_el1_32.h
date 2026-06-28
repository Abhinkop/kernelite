
/**
 * @file sctlr_decode_32.h
 * @brief ARMv8-A SCTLR_EL1 (System Control Register) Decoder implementation.
 * @author Abhin Parekadan Jose
 * @date 2024-06-01
 */

#ifndef SCTLR_EL1_32_H
#define SCTLR_EL1_32_H

#include <stdint.h>
#include <stdio.h>

/**
 * @union sctlr_el1_32_t
 * @brief Representation of the System Control Register for Exception Level 1
 * (32-bit).
 * * This union allows accessing the SCTLR_EL1 register as a collective 32-bit
 * value or as individual bit-fields according to the ARMv8-A architecture
 * reference manual.
 */
typedef union {
	/** @brief Bit-field representation */
	struct {
		/** @brief [0] MMU enable for EL1 and EL0 */
		uint32_t M : 1;

		/** @brief [1] Alignment check enable */
		uint32_t A : 1;

		/** @brief [2] Data cache enable */
		uint32_t C : 1;

		/** @brief [3] No Trap Load Multiple and Store Multiple to
		 * Device */
		uint32_t nTLSMD : 1;

		/** @brief [4] Load Multiple and Store Multiple Atomicity and
		 * Ordering Enable */
		uint32_t LSMAOE : 1;

		/** @brief [5] CP15 barrier enable (AArch32 legacy) */
		uint32_t CP15BEN : 1;

		/** @brief [6] Writes to this bit are IGNORED. Reads return an
		 * UNKNOWN value. */
		uint32_t UNK : 1;

		/** @brief [7] IT Disable (AArch32 legacy) */
		uint32_t ITD : 1;

		/** @brief [8] SETEND Disable (AArch32 legacy) */
		uint32_t SED : 1;

		/** @brief [9] Reserved (RES0) */
		uint32_t RES0_9 : 1;

		/** @brief [10] Disable EL0 access to Predictor flushes */
		uint32_t EnRCTX : 1;

		/** @brief [11] Reserved (RES1) */
		uint32_t RES1_11 : 1;

		/** @brief [12] Instruction cache enable */
		uint32_t I : 1;

		/** @brief [13] Normal vs High exep vecs */
		uint32_t V : 1;

		/** @brief [14] Reserved (RES0) */
		uint32_t RES0_14 : 1;

		/** @brief [15] Reserved (RES0) */
		uint32_t RES0_15 : 1;

		/** @brief [16] Not trap WFI (Wait For Interrupt) */
		uint32_t nTWI : 1;

		/** @brief [17] Reserved (RES0) */
		uint32_t RES0_17 : 1;

		/** @brief [18] Not trap WFE (Wait For Event) */
		uint32_t nTWE : 1;

		/** @brief [19] Write permission implies XN (Execute Never) */
		uint32_t WXN : 1;

		/** @brief [20] Unprivileged write permission implies PL1 XN
		 * (Execute-never) */
		uint32_t UWXN : 1;

		/** @brief [21] Reserved (RES0) */
		uint32_t RES0_21 : 1;

		/** @brief [22] Reserved (RES1) */
		uint32_t RES0_22 : 1;

		/** @brief [23] Set Privileged Access Never */
		uint32_t SPAN : 1;

		/** @brief [24] Reserved (RES0) */
		uint32_t RES0_24 : 1;

		/** @brief [25] Endianness of data access at EL1 (0:LE, 1:BE) */
		uint32_t EE : 1;

		/** @brief [26] Reserved (RES0) */
		uint32_t RES0_26 : 1;

		/** @brief [27] Reserved (RES0) */
		uint32_t RES0_27 : 1;

		/** @brief [28] TEX remap enable. */
		uint32_t TRE : 1;

		/** @brief [29] Access Flag Enable. */
		uint32_t AFE : 1;

		/** @brief [30] T32 Exception Enable. */
		uint32_t TE : 1;

		/** @brief [31] Default Speculative Store Bypass Safe */
		uint32_t DSSBS : 1;
	} bits;

	/** @brief Raw 64-bit register value */
	uint32_t raw;
} sctlr_el1_32_t;

/**
 * @brief Decodes and prints the status of ALL bitfields in the 32-bit SCTLR_EL1
 * mapping.
 * @param val The 32-bit raw value of the SCTLR_EL1 register to decode.
 */
static inline void decode_sctlr_el1_32(uint64_t val)
{
	sctlr_el1_32_t sctlr = { .raw = (uint32_t)val };

	printf("SCTLR_EL1 (32-bit Mapping) Decode (0x%08X):\n", sctlr.raw);
	printf("------------------------------------------------------------------\n");
	printf("MMU Enable [M]                [0]:  %u\n", sctlr.bits.M);
	printf("Alignment Check [A]           [1]:  %u\n", sctlr.bits.A);
	printf("Data Cache [C]                [2]:  %u\n", sctlr.bits.C);
	printf("No Trap LDM/STM Device [nTLSMD][3]: %u\n", sctlr.bits.nTLSMD);
	printf("LDM/STM Atomicity En [LSMAOE] [4]:  %u\n", sctlr.bits.LSMAOE);
	printf("CP15 Barrier Enable [CP15BEN] [5]:  %u\n", sctlr.bits.CP15BEN);
	printf("Unknown/Ignored [UNK]         [6]:  %u\n", sctlr.bits.UNK);
	printf("IT Disable [ITD]              [7]:  %u\n", sctlr.bits.ITD);
	printf("SETEND Disable [SED]          [8]:  %u\n", sctlr.bits.SED);
	printf("Reserved [RES0_9]             [9]:  %u\n", sctlr.bits.RES0_9);
	printf("Disable Predictor Flush [EnRCTX][10]: %u\n", sctlr.bits.EnRCTX);
	printf("Reserved [RES1_11]            [11]: %u\n", sctlr.bits.RES1_11);
	printf("Instr Cache [I]               [12]: %u\n", sctlr.bits.I);
	printf("Vectors Bit [V]               [13]: %u (%s)\n", sctlr.bits.V,
	       sctlr.bits.V ? "High (0xFFFF0000)" : "Low (0x00000000)");
	printf("Reserved [RES0_14]            [14]: %u\n", sctlr.bits.RES0_14);
	printf("Reserved [RES0_15]            [15]: %u\n", sctlr.bits.RES0_15);
	printf("Not Trap WFI [nTWI]           [16]: %u\n", sctlr.bits.nTWI);
	printf("Reserved [RES0_17]            [17]: %u\n", sctlr.bits.RES0_17);
	printf("Not Trap WFE [nTWE]           [18]: %u\n", sctlr.bits.nTWE);
	printf("Write XN [WXN]                [19]: %u\n", sctlr.bits.WXN);
	printf("Unprivileged WXN [UWXN]       [20]: %u\n", sctlr.bits.UWXN);
	printf("Reserved [RES0_21]            [21]: %u\n", sctlr.bits.RES0_21);
	printf("Reserved [RES1_22]            [22]: %u\n", sctlr.bits.RES0_22);
	printf("Set Priv Access Never [SPAN]  [23]: %u\n", sctlr.bits.SPAN);
	printf("Reserved [RES0_24]            [24]: %u\n", sctlr.bits.RES0_24);
	printf("EL1 Endianness [EE]           [25]: %u (%s)\n", sctlr.bits.EE,
	       sctlr.bits.EE ? "Big" : "Little");
	printf("Reserved [RES0_26]            [26]: %u\n", sctlr.bits.RES0_26);
	printf("Reserved [RES0_27]            [27]: %u\n", sctlr.bits.RES0_27);
	printf("TEX Remap Enable [TRE]        [28]: %u\n", sctlr.bits.TRE);
	printf("Access Flag Enable [AFE]      [29]: %u\n", sctlr.bits.AFE);
	printf("T32 Exception Enable [TE]     [30]: %u\n", sctlr.bits.TE);
	printf("Spec Store Bypass Safe [DSSBS][31]: %u\n", sctlr.bits.DSSBS);
	printf("------------------------------------------------------------------\n");
}

#endif /* SCTLR_EL1_32_H */
