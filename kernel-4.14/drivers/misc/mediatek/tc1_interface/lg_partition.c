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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... filp_open */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>	/*proc */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/aio.h>
#include <linux/uaccess.h>	/*set_fs get_fs mm_segment_t */
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/unistd.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mtd/mtd.h>
#include <mt-plat/mtk_partition.h>
#include <mt-plat/mtk_boot.h>
#include "lg_partition.h"

#define PART_NAME "misc2"
#define PART_PATH "/dev/block/platform/bootdevice/by-name/misc2"
#define TC1_MOD	"TC1_PART"


int g_boot_type = BOOTDEV_SDMMC;
int g_block_size = 512;	/* eMMC(512), UFS(4096) */

static char misc2_path[64] = {0,};

static int get_misc2_path(void)
{
	struct hd_struct *part = NULL;

	g_boot_type = get_boot_type();

	if (misc2_path[0])
		return 0;

	part = get_part(PART_NAME);
	if (!part) {
		pr_info("[%s] Not find partition %s\n", TC1_MOD, PART_NAME);
		return 1;
	}

#if defined(CONFIG_MTK_EMMC_SUPPORT) || defined(CONFIG_MTK_UFS_SUPPORT)
	if (g_boot_type == BOOTDEV_SDMMC) {
		snprintf(misc2_path, sizeof(misc2_path),
					"/dev/block/mmcblk0p%d", part->partno);
	} else if (g_boot_type == BOOTDEV_UFS) {
		g_block_size = 4096;
		snprintf(misc2_path, sizeof(misc2_path),
					"/dev/block/sdc%d", part->partno);
	}
#else
#error can not copy misc2 path
#endif
	put_part(part);

	pr_info("[%s] g_boot_type[%d], block size[%d]\n",
		TC1_MOD,  g_boot_type, g_block_size);

	return 0;
}

static struct file *tc1_file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);

	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		pr_info("[%s] filp open fail! errno=%d\n", TC1_MOD, err);
		return NULL;
	}

	return filp;
}

static int tc1_file_read(struct file *file,
			unsigned int offset,
			unsigned char *data,
			unsigned int size)
{
	int ret = 0;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_read(file, data, size, &file->f_pos);
	set_fs(oldfs);

	return ret;
}

static int tc1_file_write(struct file *file,
			unsigned int offset,
			unsigned char *data,
			unsigned int size)
{
	int ret = 0;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_write(file, data, size, &file->f_pos);
	set_fs(oldfs);

	return ret;
}

static int tc1_file_seek(struct file *file, unsigned int offset)
{
	int ret = 0;
	loff_t pos;

	if (offset > LGE_FAC_MAX_OFFSET) {
		pr_info("[%s] Wrong index, seek fail!\n", TC1_MOD);
		return -1;
	}

	pos = offset * g_block_size;
	ret = vfs_setpos(file, file->f_pos + pos,
			LGE_FAC_MAX_OFFSET * g_block_size);

	return ret;
}

unsigned char _LGE_GENERIC_WRITE_FUN(unsigned char *buff,
				unsigned int offset,
				unsigned int length)
{
	int ret;
	struct file *filp;
	unsigned char *tmp;

	ret = get_misc2_path();
	if (ret == 1) {
		pr_info("[%s] get MISC2 partition info fail!\n", TC1_MOD);
		return -1;
	}

	filp = tc1_file_open(PART_PATH/*misc2_path*/, O_RDWR, 0660);
	if (filp == NULL) {
		pr_info("[%s] Open MISC2 partition fail\n", TC1_MOD);
		return -1;
	}
	pr_debug("[%s] WRITE - Open MISC2 partition OK!\n", TC1_MOD);

	tmp = kzalloc(EMMC_BLOCK_SIZE, GFP_KERNEL);
	if (!tmp) {
		filp_close(filp, NULL);
		return 0;
	}

	memcpy(tmp, buff, length);

	ret = tc1_file_seek(filp, offset);
	if (ret != offset * g_block_size) {
		pr_info("[%s] seek fail! ret[%d]\n", TC1_MOD, ret);
		filp_close(filp, NULL);
		kfree(tmp);
		return 0;
	}

	ret = tc1_file_write(filp, offset, tmp, EMMC_BLOCK_SIZE);
	if (ret != EMMC_BLOCK_SIZE) {
		pr_info("[%s] write fail!errno=%d\n", TC1_MOD, ret);
		filp_close(filp, NULL);
		kfree(tmp);
		return 0;
	}

	filp_close(filp, NULL);
	kfree(tmp);
	pr_debug("[%s] WRITE -  OK!\n", TC1_MOD);

	return 1;
}

unsigned char _LGE_GENERIC_READ_FUN(unsigned char *buff,
				unsigned int offset,
				unsigned int length)
{
	int ret;
	struct file *filp;
	unsigned char *tmp;

	ret = get_misc2_path();
	if (ret == 1) {
		pr_info("[%s] get MISC2 partition info fail!\n", TC1_MOD);
		return -1;
	}

	filp = tc1_file_open(PART_PATH/*misc2_path*/, O_RDONLY, 0440);
	if (filp == NULL) {
		pr_info("[%s] Open MISC2 partition fail\n", TC1_MOD);
		return -1;
	}
	pr_debug("[%s] READ - Open MISC2 partition OK!\n", TC1_MOD);

	tmp = kzalloc(EMMC_BLOCK_SIZE, GFP_KERNEL);
	if (!tmp) {
		filp_close(filp, NULL);
		return 0;
	}

	ret = tc1_file_seek(filp, offset);
	if (ret != offset * g_block_size) {
		pr_info("[%s] seek fail! ret[%d]\n", TC1_MOD, ret);
		filp_close(filp, NULL);
		kfree(tmp);
		return 0;
	}

	ret = tc1_file_read(filp, offset, tmp, EMMC_BLOCK_SIZE);
	if (ret != EMMC_BLOCK_SIZE) {
		pr_info("[%s] read fail!errno=%d\n", TC1_MOD, ret);
		filp_close(filp, NULL);
		kfree(tmp);
		return 0;
	}

	memcpy(buff, tmp, length);
	filp_close(filp, NULL);
	kfree(tmp);
	pr_debug("[%s] READ -  OK!\n", TC1_MOD);

	return 1;
}

unsigned char calchecksum(unsigned char *data, int leng)
{
	unsigned char csum;

	csum = 0xFF;

	for ( ; leng > 0; leng--)
		csum += *data++;

	return ~csum;
}

extern int of_scan_meid_node(char *meid, int len);

unsigned char tc1_read_meid(unsigned char *meid)
{
	bool ret;

#ifdef CONFIG_MACH_LGE
	ret = of_scan_meid_node(&meid[1], LGE_FAC_MEID_LEN);
#else
	ret = LGE_FacReadMeid(0 , meid);
#endif
	return (unsigned char) ret;
}

#define DEF_TC1_SYNC_MAKER      0x26
#define DEF_TC1_SYNC_LENG       16

unsigned char tc1_read_meid_syncform(unsigned char *meid, int leng)
{
	bool ret;
	/*
	sync format
	Maker (1 byte) : 0x26
	Meid  (14 bytes) : from LSB to MSB
	Checksum (1 byte) : using "calchecksum" function.
	*/

	if (leng != DEF_TC1_SYNC_LENG)
		return false;

	meid[0]  = DEF_TC1_SYNC_MAKER;

#ifdef CONFIG_MACH_LGE
	ret = of_scan_meid_node(&meid[1], LGE_FAC_MEID_LEN);
#else
	ret = LGE_FacReadMeid(0, &meid[1]);
#endif
	meid[DEF_TC1_SYNC_LENG-1] = calchecksum(&meid[1], LGE_FAC_MEID_LEN);

    pr_err("tc1_read_meid_syncform LGFTM_IMEI = %s\n", meid);

	return (unsigned char) ret;
}

unsigned char LGE_FacWriteWifiMacAddr(unsigned char *wifiMacAddr,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(wifiMacAddr, LGE_FAC_WIFI_MAC_ADDR_OFFSET,
				LGE_FAC_WIFI_MAC_ADDR_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteWifiMacAddr);

unsigned char LGE_FacReadWifiMacAddr(unsigned char *wifiMacAddr)
{
	return _LGE_GENERIC_READ_FUN(wifiMacAddr,
				LGE_FAC_WIFI_MAC_ADDR_OFFSET,
				LGE_FAC_WIFI_MAC_ADDR_LEN);
}
EXPORT_SYMBOL(LGE_FacReadWifiMacAddr);

unsigned char LGE_FacWriteBtAddr(unsigned char *btAddr,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(btAddr,
				LGE_FAC_BT_ADDR_OFFSET,
				LGE_FAC_BT_ADDR_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteBtAddr);

unsigned char LGE_FacReadBtAddr(unsigned char *btAddr)
{
	return _LGE_GENERIC_READ_FUN(btAddr, LGE_FAC_BT_ADDR_OFFSET,
				LGE_FAC_BT_ADDR_LEN);
}
EXPORT_SYMBOL(LGE_FacReadBtAddr);

const unsigned int imei_mapping_table[4] = { LGE_FAC_IMEI_1_OFFSET,
	LGE_FAC_IMEI_0_OFFSET,
	LGE_FAC_IMEI_2_OFFSET,
	LGE_FAC_IMEI_3_OFFSET
};

unsigned char LGE_FacWriteImei(unsigned char imei_type,
				unsigned char *imei,
				unsigned char needFlashProgram)
{
	if (imei_mapping_table[imei_type] == LGE_FAC_IMEI_ENDMARK)
		return 0;

	return _LGE_GENERIC_WRITE_FUN(imei,
				imei_mapping_table[imei_type],
				LGE_FAC_IMEI_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteImei);

unsigned char LGE_FacReadImei(unsigned char imei_type, unsigned char *imei)
{
	if (imei_mapping_table[imei_type] == LGE_FAC_IMEI_ENDMARK)
		return 0;

	return _LGE_GENERIC_READ_FUN(imei,
				imei_mapping_table[imei_type],
				LGE_FAC_IMEI_LEN);
}
EXPORT_SYMBOL(LGE_FacReadImei);

unsigned char LGE_FacWriteSimLockType(unsigned char simLockType,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(&simLockType,
				LGE_FAC_SIM_LOCK_TYPE_OFFSET,
				LGE_FAC_SIM_LOCK_TYPE_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteSimLockType);

unsigned char LGE_FacReadSimLockType(unsigned char *simLockType)
{
	return _LGE_GENERIC_READ_FUN(simLockType, LGE_FAC_SIM_LOCK_TYPE_OFFSET,
				LGE_FAC_SIM_LOCK_TYPE_LEN);
}
EXPORT_SYMBOL(LGE_FacReadSimLockType);

unsigned char LGE_FacWriteNetworkCodeListNum(unsigned short *networkCodeListNum,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN((unsigned char *)&networkCodeListNum,
				LGE_FAC_NETWORK_CODE_LIST_NUM_OFFSET,
				LGE_FAC_NETWORK_CODE_LIST_NUM_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteNetworkCodeListNum);

unsigned char LGE_FacReadNetworkCodeListNum(unsigned short *networkCodeListNum)
{
	return _LGE_GENERIC_READ_FUN((unsigned char *)networkCodeListNum,
				LGE_FAC_NETWORK_CODE_LIST_NUM_OFFSET,
				LGE_FAC_NETWORK_CODE_LIST_NUM_LEN);
}
EXPORT_SYMBOL(LGE_FacReadNetworkCodeListNum);

unsigned char LGE_FacWriteUnlockCodeVerifyFailCount(unsigned char failCount,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(&failCount,
				LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_OFFSET,
				LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteUnlockCodeVerifyFailCount);

unsigned char LGE_FacReadUnlockCodeVerifyFailCount(unsigned char *failCount)
{
	return _LGE_GENERIC_READ_FUN(failCount,
				LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_OFFSET,
				LGE_FAC_UNLOCK_CODE_VERIFY_FAIL_COUNT_LEN);
}
EXPORT_SYMBOL(LGE_FacReadUnlockCodeVerifyFailCount);

#ifndef CONFIG_MACH_LGE
unsigned char LGE_FacWriteUnlockFailCount(unsigned char simLockType,
				unsigned char failCount,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(&failCount,
				LGE_FAC_UNLOCK_FAIL_COUNT_OFFSET,
				LGE_FAC_UNLOCK_FAIL_COUNT_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteUnlockFailCount);

unsigned char LGE_FacReadUnlockFailCount(unsigned char simLockType,
				unsigned char *failCount)
{
	return _LGE_GENERIC_READ_FUN(failCount,
				LGE_FAC_UNLOCK_FAIL_COUNT_OFFSET,
				LGE_FAC_UNLOCK_FAIL_COUNT_LEN);
}
EXPORT_SYMBOL(LGE_FacReadUnlockFailCount);
#endif

unsigned char LGE_FacWriteUnlockCode(struct FactoryUnlockCode *unlockCode,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN((unsigned char *)unlockCode,
				LGE_FAC_UNLOCK_CODE_OFFSET,
				LGE_FAC_UNLOCK_CODE_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteUnlockCode);

unsigned char LGE_FacVerifyUnlockCode(unsigned char simLockType,
				unsigned char *unlockCode, unsigned char *isOk)
{
	*isOk = 1;
	return 1;
}
EXPORT_SYMBOL(LGE_FacVerifyUnlockCode);

unsigned char LGE_FacCheckUnlockCodeValidness(unsigned char *isValid)
{
	*isValid = 1;
	return 1;
}
EXPORT_SYMBOL(LGE_FacCheckUnlockCodeValidness);

unsigned char LGE_FacWriteNetworkCode(struct FactoryNetworkCode *networkCode,
				unsigned short networkCodeListNum,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN((unsigned char *)networkCode,
				      LGE_FAC_NETWORK_CODE_OFFSET,
				      LGE_FAC_NETWORK_CODE_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteNetworkCode);

unsigned char LGE_FacReadNetworkCode(struct FactoryNetworkCode *networkCode,
			    unsigned short networkCodeListNum)
{
	return _LGE_GENERIC_READ_FUN((unsigned char *)networkCode,
				LGE_FAC_NETWORK_CODE_OFFSET,
				LGE_FAC_NETWORK_CODE_LEN);
}
EXPORT_SYMBOL(LGE_FacReadNetworkCode);

unsigned char LGE_FacCheckNetworkCodeValidness(unsigned char simLockType,
				unsigned char *isValid)
{
	*isValid = 1;
	return 1;
}
EXPORT_SYMBOL(LGE_FacCheckNetworkCodeValidness);

unsigned char LGE_FacInitSimLockData(void)
{
	return 1;
}
EXPORT_SYMBOL(LGE_FacInitSimLockData);

unsigned char LGE_FacReadFusgFlag(unsigned char *fusgFlag)
{
	return _LGE_GENERIC_READ_FUN(fusgFlag,
				LGE_FAC_FUSG_FLAG_OFFSET,
				LGE_FAC_FUSG_FLAG_LEN);
}
EXPORT_SYMBOL(LGE_FacReadFusgFlag);

unsigned char LGE_FacWriteFusgFlag(unsigned char fusgFlag,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(&fusgFlag,
				LGE_FAC_FUSG_FLAG_OFFSET,
				LGE_FAC_FUSG_FLAG_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteFusgFlag);

unsigned char LGE_FacReadDataVersion(unsigned char *dataVersion)
{
	return _LGE_GENERIC_READ_FUN(dataVersion,
				LGE_FAC_DATA_VERSION_OFFSET,
				LGE_FAC_DATA_VERSION_LEN);
}
EXPORT_SYMBOL(LGE_FacReadDataVersion);

unsigned char LGE_FacWriteDataVersion(unsigned char *dataVersion,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(dataVersion,
				LGE_FAC_DATA_VERSION_OFFSET,
				LGE_FAC_DATA_VERSION_LEN);
}
EXPORT_SYMBOL(LGE_FacWriteDataVersion);

unsigned char LGE_FacReadPid(unsigned char *pid)
{
	return _LGE_GENERIC_READ_FUN(pid,
				LGE_FAC_PID_OFFSET,
				LGE_FAC_PID_LEN);
}
EXPORT_SYMBOL(LGE_FacReadPid);

unsigned char LGE_FacWritePid(unsigned char *pid,
				unsigned char needFlashProgram)
{
	return _LGE_GENERIC_WRITE_FUN(pid,
				LGE_FAC_PID_OFFSET,
				LGE_FAC_PID_LEN);
}
EXPORT_SYMBOL(LGE_FacWritePid);

void LGE_FacGetSoftwareversion(unsigned char isOriginalVersion,
				unsigned char *pVersion)
{
	int index = 0;

	for (index = 0; index < LGE_FAC_SV_LEN; index++)
		pVersion[index] = index;
}
EXPORT_SYMBOL(LGE_FacGetSoftwareversion);

/* sbp_interface */
bool LGE_FacReadNetworkCode_SBP(struct FactoryNetworkCode *networkCode, unsigned short networkCodeListNum, unsigned int length)
{
	int i = 0;
	bool result = 0;
	struct FactoryNetworkCode pNTCodeBuf01[16];
	struct FactoryNetworkCode pNTCodeBuf02[16];
	struct FactoryNetworkCode pNTCodeBuf03[8];

	if (length != LGE_FAC_MAX_NETWORK_CODE_LIST_NUM*sizeof(struct FactoryNetworkCode))
	{
		pr_err("LGE_FAC_NETWORK_CODE_SIZE_SBP length Error!\n");
		return 0;
	}

	if (( networkCode == 0) || ( networkCode == NULL ) || (networkCodeListNum > LGE_FAC_MAX_NETWORK_CODE_LIST_NUM ))
	{
		pr_err("networkCodeListNum=%d is too big!\n", networkCodeListNum);
		return 0;
	}
	
	result = _LGE_GENERIC_READ_FUN((unsigned char*)pNTCodeBuf01, LGE_FAC_NETWORK_CODE_IDX01, LGE_FAC_NETWORK_CODE_SIZE01);
	if( result != 1 )
	{
		pr_err("LGE_FAC_NETWORK_CODE_IDX01 read fail\n");
		return 0;
	}
	result = _LGE_GENERIC_READ_FUN((unsigned char*)pNTCodeBuf02, LGE_FAC_NETWORK_CODE_IDX02, LGE_FAC_NETWORK_CODE_SIZE02);
	if( result != 1 )
	{
		pr_err("LGE_FAC_NETWORK_CODE_IDX01 read fail\n");
		return 0;
	}
	result = _LGE_GENERIC_READ_FUN((unsigned char*)pNTCodeBuf03, LGE_FAC_NETWORK_CODE_IDX03, LGE_FAC_NETWORK_CODE_SIZE03);
	if( result != 1 )
	{
		pr_err("LGE_FAC_NETWORK_CODE_IDX01 read fail\n");
		return 0;
	}

	for(i = 0; i < 16; i++)
	{
		memcpy(&networkCode[i], &pNTCodeBuf01[i], sizeof(struct FactoryNetworkCode));
	}
	for(i = 0; i < 16; i++)
	{
		memcpy(&networkCode[i+16], &pNTCodeBuf02[i], sizeof(struct FactoryNetworkCode));
	}
	for(i = 0; i < (LGE_FAC_MAX_NETWORK_CODE_LIST_NUM-32); i++)
	{
		memcpy(&networkCode[i+32], &pNTCodeBuf03[i], sizeof(struct FactoryNetworkCode));
	}

	return 1;
}
EXPORT_SYMBOL(LGE_FacReadNetworkCode_SBP);

bool LGE_FacReadOneBinaryHWInfo(unsigned char *data)
{
    bool result = 0;

    result = _LGE_GENERIC_READ_FUN(data, LGE_ONE_BINARY_HWINFO_IDX, LGE_ONE_BINARY_HWINFO_SIZE);

    if( result == 1 )
    {
        pr_err("LGE_ONE_BINARY_HWINFO_IDX read OK, OneBinaryHWInfo=%s\n", data);
    }
	else
	{
		pr_err("LGE_ONE_BINARY_HWINFO_IDX read fail\n");
	}

    return result;
}
EXPORT_SYMBOL(LGE_FacReadOneBinaryHWInfo);

bool LGE_FacWriteOneBinaryHWInfo(unsigned char *data, bool needFlashProgram)
{
    bool result = 0;

    result = _LGE_GENERIC_WRITE_FUN(data, LGE_ONE_BINARY_HWINFO_IDX, LGE_ONE_BINARY_HWINFO_SIZE);

    if( result == 1 )
    {
        pr_err("LGE_ONE_BINARY_HWINFO_IDX write OK, OneBinaryHWInfo=%s\n", data);
    }
	else
	{
		pr_err("LGE_ONE_BINARY_HWINFO_IDX write fail!");
	}

    return result;
}
EXPORT_SYMBOL(LGE_FacWriteOneBinaryHWInfo);

bool LGE_FacReadSVN_SBP (unsigned char *svn)
{
	bool result = 0;

	result = _LGE_GENERIC_READ_FUN (svn, LGE_FAC_IMEI_SVN_IDX, LGE_FAC_IMEI_SVN_SIZE);

	if( result == 1 )
	{
		pr_debug("LGE_FAC_IMEI_SVN_IDX read OK, svn=%s\n", svn);
	}
	else
	{
		pr_debug("LGE_FAC_IMEI_SVN_IDX read fail\n");
		return 0;
	}

	return 1;
}
/* sbp_interface */
#ifdef ENABLE_TEST
int LGE_API_test(void)
{
	unsigned char buff[EMMC_BLOCK_SIZE];
	unsigned char temp[EMMC_BLOCK_SIZE];
	int index = 0;

#if 1
	memset(buff, 0x0, EMMC_BLOCK_SIZE);
	LGE_FacReadImei(1, buff);

	snprintf(temp, LGE_FAC_IMEI_LEN * 2 + 4,
		 "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		 buff[0], buff[1], buff[2], buff[3],
		 buff[4], buff[5], buff[6], buff[7],
		 buff[8], buff[9], buff[10], buff[11],
		 buff[12], buff[13], buff[14]);

	pr_info("[%s] Read IMEI : %s\n", TC1_MOD, temp);

	/* if IMEI is 1, 1, x, x, .... => do write test, check it by at%imei */
	if (buff[0] == 1 && buff[1] == 1) {
		for (index = 0 ; index < LGE_FAC_IMEI_LEN ; index++)
			buff[index] = (index * 3) % 10;

		snprintf(temp, LGE_FAC_IMEI_LEN * 2 + 4,
			 "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			 buff[0], buff[1], buff[2], buff[3],
			 buff[4], buff[5], buff[6], buff[7],
			 buff[8], buff[9], buff[10], buff[11],
			 buff[12], buff[13], buff[14]);

		pr_info("[%s] Write IMEI : %s\n", TC1_MOD, temp);
		LGE_FacWriteImei(1, buff, 1);
	}
#endif

	return 0;
}
EXPORT_SYMBOL(LGE_API_test);
#endif

MODULE_AUTHOR("teddy.seo@mediatek.com");
MODULE_DESCRIPTION("access partition API");
MODULE_LICENSE("GPL");
