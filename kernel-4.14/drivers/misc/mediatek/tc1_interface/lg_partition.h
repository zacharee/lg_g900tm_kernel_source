/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LG_PARTITION_H__
#define __LG_PARTITION_H__

/* ENABLE_TEST should be disabled in release source */
/* it is just for the MTK internal testing of the new platform */
#if 0
#define ENABLE_TEST
#endif

/* DATA layout*/
#ifdef CONFIG_MACH_LGE
#define LGE_FAC_DATA_VERSION_OFFSET                     (2)
#define LGE_FAC_PID_OFFSET                              (4)
#define LGE_FAC_BT_ADDR_OFFSET                          (6)
#define LGE_FAC_IMEI_MASTER_OFFSET                      (8)
#define LGE_FAC_IMEI_NOT_MASTER_OFFSET                  (9)
#define LGE_FAC_SOFTWARE_VERSION_OFFSET                 (44)
#define LGE_FAC_IMEI_TRIPLE_OFFSET                      (10)
#define LGE_FAC_IMEI_QUADRUPLE_OFFSET                   (11)
#define LGE_FAC_NETWORK_CODE_LIST_NUM_OFFSET            (14)
#define LGE_FAC_SIM_LOCK_TYPE_OFFSET                    (16)
#define LGE_FAC_FUSG_FLAG_OFFSET                        (18)
#define LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_OFFSET    (26)
#define LGE_FAC_WIFI_MAC_ADDR_OFFSET                    (28)
#define LGE_FAC_UNLOCK_CODE_OFFSET                      (30)
#define LGE_FAC_NETWORK_CODE_OFFSET                     (46)

#define LGE_FAC_IMEI_ENDMARK    (0xFFFFFFFF)

#define LGE_FAC_IMEI_0_OFFSET	LGE_FAC_IMEI_MASTER_OFFSET
#define LGE_FAC_IMEI_1_OFFSET	LGE_FAC_IMEI_NOT_MASTER_OFFSET
#define LGE_FAC_IMEI_2_OFFSET	LGE_FAC_IMEI_TRIPLE_OFFSET
#define LGE_FAC_IMEI_3_OFFSET	LGE_FAC_IMEI_QUADRUPLE_OFFSET

#define LGE_FAC_MEID_0_OFFSET		LGE_FAC_IMEI_TRIPLE_OFFSET
#define LGE_FAC_MEID_1_OFFSET		LGE_FAC_IMEI_QUADRUPLE_OFFSET

#define LGE_FAC_MAX_OFFSET  (200)
#else
#define LGE_FAC_WIFI_MAC_ADDR_OFFSET                    (1)
#define LGE_FAC_BT_ADDR_OFFSET                          (2)
#define LGE_FAC_IMEI_MASTER_OFFSET                      (3)
#define LGE_FAC_IMEI_NOT_MASTER_OFFSET                  (4)
#define LGE_FAC_SIM_LOCK_TYPE_OFFSET                    (5)
#define LGE_FAC_NETWORK_CODE_LIST_NUM_OFFSET            (6)
#define LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_OFFSET    (7)
#define LGE_FAC_UNLOCK_FAIL_COUNT_OFFSET                (8)
#define LGE_FAC_UNLOCK_CODE_OFFSET                      (9)
#define LGE_FAC_VERIFY_UNLOCK_CODE_OFFSET               (10)
#define LGE_FAC_UNLOCK_CODE_VALIDNESS_OFFSET            (11)
#define LGE_FAC_NETWORK_CODE_OFFSET                     (12)
#define LGE_FAC_NETWORK_CODE_VALIDNESS_OFFSET           (13)
#define LGE_FAC_INIT_SIM_LOCK_DATA_OFFSET               (14)
#define LGE_FAC_FUSG_FLAG_OFFSET                        (15)
#define LGE_FAC_DATA_VERSION_OFFSET                     (16)
#define LGE_FAC_PID_OFFSET                              (17)
#define LGE_FAC_SOFTWARE_VERSION_OFFSET                 (18)
#define LGE_FAC_IMEI_TRIPLE_OFFSET                      (19)
#define LGE_FAC_IMEI_QUADRUPLE_OFFSET                   (20)

#define LGE_FAC_IMEI_ENDMARK    (0xFFFFFFFF)

#define LGE_FAC_IMEI_0_OFFSET   LGE_FAC_IMEI_MASTER_OFFSET
#define LGE_FAC_IMEI_1_OFFSET   LGE_FAC_IMEI_NOT_MASTER_OFFSET
#define LGE_FAC_IMEI_2_OFFSET   LGE_FAC_IMEI_TRIPLE_OFFSET
#define LGE_FAC_IMEI_3_OFFSET   LGE_FAC_IMEI_QUADRUPLE_OFFSET

#define LGE_FAC_MAX_OFFSET  (200)
#endif

/*data length*/
#define LGE_FAC_PID_PART_1_LEN 22
#define LGE_FAC_PID_PART_2_LEN 10
/* decimal(22) + ASCII(10) */
#define LGE_FAC_PID_LEN (LGE_FAC_PID_PART_1_LEN + LGE_FAC_PID_PART_2_LEN)
#define LGE_FAC_DATA_VERSION_LEN 4
#define LGE_FAC_FUSG_FLAG	1
#define LGE_FAC_UNLOCK_FAIL_COUNT_LEN	1
#define LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_LEN	1
#define LGE_FAC_VERIFY_UNLOCK_CODE_LEN	1
#define LGE_FAC_NETWORK_CODE_LIST_NUM_LEN	2
#define LGE_FAC_SIM_LOCK_TYPE_LEN	1
#define LGE_FAC_BT_ADDR_LEN (0x6)	/* hexadecimal */
#define LGE_FAC_IMEI_LEN (15)	/* decimal */
#define LGE_FAC_MEID_LEN (14)
#define LGE_FAC_WIFI_MAC_ADDR_LEN (6)
#define LGE_FAC_SUFFIX_STR_LEN (15)
#define LGE_FAC_NC_MCC_LEN 3
#define LGE_FAC_NC_MNC_LEN 3
#define LGE_FAC_NC_GID1_LEN 8
#define LGE_FAC_NC_GID2_LEN 8
#define LGE_FAC_NC_SUBSET_LEN 2
#define LGE_FAC_NETWORK_CODE_LEN (LGE_FAC_NC_MCC_LEN + LGE_FAC_NC_MNC_LEN + \
	LGE_FAC_NC_GID1_LEN + LGE_FAC_NC_GID2_LEN + LGE_FAC_NC_SUBSET_LEN)
#define LGE_FAC_SV_LEN	60
#define LGE_FAC_FUSG_FLAG_LEN 1
/* This number may be increased in the future */
#define LGE_FAC_MAX_NETWORK_CODE_LIST_NUM (40)
#define LGE_FAC_UNLOCK_CODE_LEN (16)
#define LGE_FAC_SLTYPE_VALID_MASK 0x1F
#define LGE_FAC_SLTYPE_MASK_NETWORK 0x01
#define LGE_FAC_SLTYPE_MASK_SERVICE_PROVIDER 0x02
#define LGE_FAC_SLTYPE_MASK_NETWORK_SUBSET 0x04
#define LGE_FAC_SLTYPE_MASK_COOPERATE 0x08
#define LGE_FAC_SLTYPE_MASK_LOCK_TO_SIM 0x10
#define LGE_FAC_SLTYPE_MASK_HARDLOCK 0x20
#define LGE_FAC_SLTYPE_MASK_RESERVED_1 0x40	/* T.B.D */
#define LGE_FAC_SLTYPE_MASK_RESERVED_2 0x80	/* T.B.D */
#define LGE_FAC_MAX_UNLOCK_CODE_VERIFY_FAIL_COUNT 3

/* sbp_interface */
#define LGE_ONE_BINARY_HWINFO_IDX 100
#define LGE_ONE_BINARY_HWINFO_SIZE 512

#define LGE_FAC_FIXED_SOFTWARE_VER_IDX 101 /* SWFV-A Index */
#define LGE_FAC_FIXED_SOFTWARE_VER_SIZE 50 /* SWFV-A Size */

#define LGE_FAC_IMEI_SVN_IDX 102 /* SBP SVN Index */
#define LGE_FAC_IMEI_SVN_SIZE 10 /* SBP SVN Size */

#define LGE_FAC_MAX_NTCODE_COUNT_SBP 16

#define LGE_FAC_NETWORK_CODE_IDX01          46 /* facNetworkCode */
#define LGE_FAC_NETWORK_CODE_SIZE01         sizeof(struct FactoryNetworkCode)*16

#define LGE_FAC_NETWORK_CODE_IDX02          47 /* facNetworkCode */
#define LGE_FAC_NETWORK_CODE_SIZE02         sizeof(struct FactoryNetworkCode)*16

#define LGE_FAC_NETWORK_CODE_IDX03          48    /* facNetworkCode */
#define LGE_FAC_NETWORK_CODE_SIZE03         sizeof(struct FactoryNetworkCode)*8

#define LGE_FAC_NETWORK_CODE_SIZE_SBP       sizeof(struct FactoryNetworkCode)*LGE_FAC_MAX_NTCODE_COUNT_SBP
/* sbp_interface */

#define EMMC_BLOCK_SIZE	512

struct FactoryNetworkCode {
	/* Ex) { 2, 4, 5 } */
	unsigned char Mcc[LGE_FAC_NC_MCC_LEN];
	/* Ex) { 4, 3, 0xF } */
	unsigned char Mnc[LGE_FAC_NC_MNC_LEN];
	/* Ex) { 0xB, 2, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF } */
	unsigned char Gid1[LGE_FAC_NC_GID1_LEN];
	/* Ex) { 8, 0xA, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF } */
	unsigned char Gid2[LGE_FAC_NC_GID2_LEN];
	/* Ex) { 6, 2 } */
	unsigned char Subset[LGE_FAC_NC_SUBSET_LEN];
	unsigned char dummy[8];
};

struct FactoryUnlockCode {
	unsigned char network[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char serviceProvider[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char networkSubset[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char cooperate[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char lockToSim[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char hardlock[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char reserved_1[LGE_FAC_UNLOCK_CODE_LEN];
	unsigned char reserved_2[LGE_FAC_UNLOCK_CODE_LEN];
};

unsigned char LGE_FacWriteWifiMacAddr(unsigned char *wifiMacAddr,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadWifiMacAddr(unsigned char *wifiMacAddr);
unsigned char LGE_FacWriteBtAddr(unsigned char *btAddr,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadBtAddr(unsigned char *btAddr);
unsigned char LGE_FacWriteImei(unsigned char imei_type,
	unsigned char *imei,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadImei(unsigned char imei_type,
	unsigned char *imei);
unsigned char LGE_FacWriteSimLockType(unsigned char simLockType,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadSimLockType(unsigned char *simLockType);
unsigned char LGE_FacWriteUnlockCodeVerifyFailCount(unsigned char failCount,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadUnlockCodeVerifyFailCount(unsigned char *failCount);
#ifndef CONFIG_MACH_LGE
unsigned char LGE_FacWriteUnlockFailCount(unsigned char simLockType,
	unsigned char failCount,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadUnlockFailCount(unsigned char simLockType,
	unsigned char *failCount);
#endif
unsigned char LGE_FacWriteUnlockCode(struct FactoryUnlockCode *unlockCode,
	unsigned char needFlashProgram);
unsigned char LGE_FacVerifyUnlockCode(unsigned char simLockType,
	unsigned char *unlockCode,
	unsigned char *isOk);
unsigned char LGE_FacCheckUnlockCodeValidness(unsigned char *isValid);
unsigned char LGE_FacWriteNetworkCode(struct FactoryNetworkCode *networkCode,
	unsigned short networkCodeListNum,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadNetworkCode(struct FactoryNetworkCode *networkCode,
	unsigned short networkCodeListNum);
unsigned char LGE_FacWriteNetworkCodeListNum(unsigned short *networkCodeListNum,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadNetworkCodeListNum(unsigned short *networkCodeListNum);
unsigned char LGE_FacCheckNetworkCodeValidness(unsigned char simLockType,
	unsigned char *isValid);
unsigned char LGE_FacInitSimLockData(void);
unsigned char LGE_FacReadFusgFlag(unsigned char *fusgFlag);
unsigned char LGE_FacWriteFusgFlag(unsigned char fusgFlag,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadDataVersion(unsigned char *dataVersion);
unsigned char LGE_FacWriteDataVersion(unsigned char *dataVersion,
	unsigned char needFlashProgram);
unsigned char LGE_FacReadPid(unsigned char *pid);
unsigned char LGE_FacWritePid(unsigned char *pid,
	unsigned char needFlashProgram);
void LGE_FacGetSoftwareversion(unsigned char isOriginalVersion,
	unsigned char *pVersion);
#ifdef ENABLE_TEST
int LGE_API_test(void);
#endif

/* sbp_interface */
bool LGE_FacReadNetworkCode_SBP (struct FactoryNetworkCode *networkCode, unsigned short  networkCodeListNum, unsigned int length);
bool LGE_FacReadOneBinaryHWInfo (unsigned char *data);
bool LGE_FacWriteOneBinaryHWInfo (unsigned char *data, bool needFlashProgram);
bool LGE_FacReadSVN_SBP (unsigned char *svn);
/* sbp_interface */

#endif
