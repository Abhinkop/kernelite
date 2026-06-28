/**
 * @file sctlr_decode.h
 * @brief ARMv8-A SCTLR_EL1 (System Control Register) Decoder implementation.
 * @author Abhin Parekadan Jose
 * @date 2024-06-01
 */

#ifndef SCTLR_EL1_64_H
#define SCTLR_EL1_64_H

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Representation of the System Control Register for Exception Level 1.
 * * This union allows accessing the SCTLR_EL1 register as a collective 64-bit
 * value or as individual bit-fields according to the ARMv8-A architecture
 * reference manual.
 */
typedef union {
	/** @brief Bit-field representation */
	struct {
		/** @brief [0] MMU enable for EL1 and EL0 */
		uint64_t M : 1;

		/** @brief [1] Alignment check enable */
		uint64_t A : 1;

		/** @brief [2] Data cache enable */
		uint64_t C : 1;

		/** @brief [3] Stack Alignment check enable for EL1 */
		uint64_t SA : 1;

		/** @brief [4] Stack Alignment check enable for EL0 */
		uint64_t SA0 : 1;

		/** @brief [5] CP15 barrier enable (AArch32 legacy) */
		uint64_t CP15BEN : 1;

		/** @brief [6] Non-aligned access (Alignment fault check) */
		uint64_t nAA : 1;

		/** @brief [7] IT Disable (AArch32 legacy) */
		uint64_t ITD : 1;

		/** @brief [8] SETEND Disable (AArch32 legacy) */
		uint64_t SED : 1;

		/** @brief [9] User Mask Access (EL0 access to interrupt masks)
		 */
		uint64_t UMA : 1;

		/** @brief [10] Prevents speculative execution instruction
		 * access */
		uint64_t EnRCTX : 1;

		/** @brief [11] Exception Exit is Synchronous */
		uint64_t EOS : 1;

		/** @brief [12] Instruction cache enable */
		uint64_t I : 1;

		/** @brief [13] Controls enabling of pointer authentication */
		uint64_t EnDB : 1;

		/** @brief [14] DC ZVA instruction access at EL0 */
		uint64_t DZE : 1;

		/** @brief [15] CTR_EL0 access at EL0 */
		uint64_t UCT : 1;

		/** @brief [16] Not trap WFI (Wait For Interrupt) */
		uint64_t nTWI : 1;

		/** @brief [17] Reserved (RES0) */
		uint64_t RES0_1 : 1;

		/** @brief [18] Not trap WFE (Wait For Event) */
		uint64_t nTWE : 1;

		/** @brief [19] Write permission implies XN (Execute Never) */
		uint64_t WXN : 1;

		/** @brief [20] Trap SCXTNUM access */
		uint64_t TSCXT : 1;

		/** @brief [21] Implicit Error Synchronization barrier */
		uint64_t IESB : 1;

		/** @brief [22] Exception Entry is Synchronous */
		uint64_t EIS : 1;

		/** @brief [23] Set Privileged Access Never */
		uint64_t SPAN : 1;

		/** @brief [24] Endianness of data access at EL0 (0:LE, 1:BE) */
		uint64_t E0E : 1;

		/** @brief [25] Endianness of data access at EL1 (0:LE, 1:BE) */
		uint64_t EE : 1;

		/** @brief [26] Traps EL0 cache maintenance instructions */
		uint64_t UCI : 1;

		/** @brief [27] Pointer Authentication Enable (Key A) */
		uint64_t EnDA : 1;

		/** @brief [28] No Trap Load Multiple and Store Multiple to
		 * Device */
		uint64_t nTLSMD : 1;

		/** @brief [29] Load Multiple and Store Multiple Atomicity and
		 * Ordering Enable. */
		uint64_t LSMAOE : 1;

		/** @brief [30] Pointer Authentication Enable (Key IB) */
		uint64_t EnIB : 1;

		/** @brief [31] Pointer Authentication Enable (Key IA) */
		uint64_t EnIA : 1;

		/** @brief [32:34] Reserved (RES0) */
		uint64_t RES0_2 : 3;

		/** @brief [35] PAC Branch Type compatibility at EL0 */
		uint64_t BT0 : 1;

		/** @brief [36] PAC Branch Type compatibility at EL1 */
		uint64_t BT1 : 1;

		/** @brief [37] synchronized on entry to EL1 */
		uint64_t ITFSB : 1;

		/** @brief [38:39] Tag Check Fault in EL0. */
		uint64_t TCF0 : 2;

		/** @brief [40:41] Tag Check Fault in EL1. */
		uint64_t TCF : 2;

		/** @brief [42] Allocation Tag Access in EL0 */
		uint64_t ATA0 : 1;

		/** @brief [43] Allocation Tag Access in EL1 */
		uint64_t ATA : 1;

		/** @brief [44] Default Speculative Store Bypass Safe */
		uint64_t DSSBS : 1;

		/** @brief [45] TWE Delay Enable. */
		uint64_t TWEDEn : 1;

		/** @brief [46:49] TWE Delay.  */
		uint64_t TWEDEL : 4;

		/** @brief [50:53] Reserved (RES0) */
		uint64_t RES0_3 : 4;

		/** @brief [54]  traps execution of an ST64BV instruction at EL0
		 * to EL1. */
		uint64_t EnASR : 1;

		/** @brief [55]  traps execution of an ST64BV0 instruction at
		 * EL0 to EL1. */
		uint64_t EnAS0 : 1;

		/** @brief [56]  traps execution of an LD64B or ST64B
		 * instruction at EL0 to EL1. */
		uint64_t EnALS : 1;

		/** @brief [57]  Enhanced Privileged Access Never. */
		uint64_t EPAN : 1;

		/** @brief [58:63] Reserved (RES0) */
		uint64_t RES0_4 : 28;
	} bits;

	/** @brief Raw 64-bit register value */
	uint64_t raw;
} sctlr_el1_t_64;

/**
 * @brief Decodes and prints the status of ALL bitfields in the SCTLR_EL1
 * register.
 * * @param val The 64-bit raw value of the SCTLR_EL1 register to decode.
 */
static inline void decode_sctlr_el1_64(uint64_t val)
{
	sctlr_el1_t_64 sctlr = { .raw = val };

	printf("SCTLR_EL1 Decode (0x%016lX):\n", sctlr.raw);
	printf("------------------------------------------------------------------\n");
	printf("MMU Enable [M]                [0]:  %u\n", sctlr.bits.M);
	printf("Alignment Check [A]           [1]:  %u\n", sctlr.bits.A);
	printf("Data Cache [C]                [2]:  %u\n", sctlr.bits.C);
	printf("Stack Align EL1 [SA]          [3]:  %u\n", sctlr.bits.SA);
	printf("Stack Align EL0 [SA0]         [4]:  %u\n", sctlr.bits.SA0);
	printf("CP15 Barrier Enable [CP15BEN] [5]:  %u\n", sctlr.bits.CP15BEN);
	printf("Non-aligned Access [nAA]      [6]:  %u\n", sctlr.bits.nAA);
	printf("IT Disable [ITD]              [7]:  %u\n", sctlr.bits.ITD);
	printf("SETEND Disable [SED]          [8]:  %u\n", sctlr.bits.SED);
	printf("User Mask Access [UMA]        [9]:  %u\n", sctlr.bits.UMA);
	printf("Enforce RCTX [EnRCTX]         [10]: %u\n", sctlr.bits.EnRCTX);
	printf("Exception Exit Sync [EOS]     [11]: %u\n", sctlr.bits.EOS);
	printf("Instr Cache [I]               [12]: %u\n", sctlr.bits.I);
	printf("PAuth EnDB [EnDB]             [13]: %u\n", sctlr.bits.EnDB);
	printf("DC ZVA at EL0 [DZE]           [14]: %u\n", sctlr.bits.DZE);
	printf("CTR_EL0 Access [UCT]          [15]: %u\n", sctlr.bits.UCT);
	printf("Not Trap WFI [nTWI]           [16]: %u\n", sctlr.bits.nTWI);
	printf("Not Trap WFE [nTWE]           [18]: %u\n", sctlr.bits.nTWE);
	printf("Write XN [WXN]                [19]: %u\n", sctlr.bits.WXN);
	printf("Trap SCXTNUM [TSCXT]          [20]: %u\n", sctlr.bits.TSCXT);
	printf("Implicit Error Sync [IESB]    [21]: %u\n", sctlr.bits.IESB);
	printf("Exception Entry Sync [EIS]    [22]: %u\n", sctlr.bits.EIS);
	printf("Set Priv Access Never [SPAN]  [23]: %u\n", sctlr.bits.SPAN);
	printf("EL0 Endianness [E0E]          [24]: %u (%s)\n", sctlr.bits.E0E,
	       sctlr.bits.E0E ? "BE" : "LE");
	printf("EL1 Endianness [EE]           [25]: %u (%s)\n", sctlr.bits.EE,
	       sctlr.bits.EE ? "BE" : "LE");
	printf("Trap EL0 Cache Ops [UCI]      [26]: %u\n", sctlr.bits.UCI);
	printf("PAuth EnDA [EnDA]             [27]: %u\n", sctlr.bits.EnDA);
	printf("No Trap LDM/STM Device [nTLSMD][28]: %u\n", sctlr.bits.nTLSMD);
	printf("LDM/STM Atomicity En [LSMAOE] [29]: %u\n", sctlr.bits.LSMAOE);
	printf("PAuth EnIB [EnIB]             [30]: %u\n", sctlr.bits.EnIB);
	printf("PAuth EnIA [EnIA]             [31]: %u\n", sctlr.bits.EnIA);
	printf("PAC Branch Type EL0 [BT0]     [35]: %u\n", sctlr.bits.BT0);
	printf("PAC Branch Type EL1 [BT1]     [36]: %u\n", sctlr.bits.BT1);
	printf("Instr Tag Flood Barrier [ITFSB][37]: %u\n", sctlr.bits.ITFSB);
	printf("Tag Check Fault EL0 [TCF0]    [38:39]: 0x%X\n",
	       sctlr.bits.TCF0);
	printf("Tag Check Fault EL1 [TCF]     [40:41]: 0x%X\n", sctlr.bits.TCF);
	printf("Alloc Tag Access EL0 [ATA0]   [42]: %u\n", sctlr.bits.ATA0);
	printf("Alloc Tag Access EL1 [ATA]    [43]: %u\n", sctlr.bits.ATA);
	printf("Spec Store Bypass Safe [DSSBS][44]: %u\n", sctlr.bits.DSSBS);
	printf("TWE Delay Enable [TWEDEn]     [45]: %u\n", sctlr.bits.TWEDEn);
	printf("TWE Delay Value [TWEDEL]      [46:49]: 0x%X\n",
	       sctlr.bits.TWEDEL);
	printf("Trap ST64BV EL0 [EnASR]       [54]: %u\n", sctlr.bits.EnASR);
	printf("Trap ST64BV0 EL0 [EnAS0]      [55]: %u\n", sctlr.bits.EnAS0);
	printf("Trap LD64B/ST64B EL0 [EnALS]  [56]: %u\n", sctlr.bits.EnALS);
	printf("Enhanced PAN [EPAN]           [57]: %u\n", sctlr.bits.EPAN);
	printf("------------------------------------------------------------------\n");
}

#endif /* SCTLR_EL1_64_H */
