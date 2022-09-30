/* touch_ili7807q.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: BSP-TOUCH@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <asm/uaccess.h>

#include <touch_core.h>
#include <touch_hwif.h>
#include "touch_ili7807q.h"
#include "touch_ili7807q_fw.h"

#define K			(1024)
#define M			(K * K)
#define UPDATE_PASS		0
#define UPDATE_FAIL		-1
#define TIMEOUT_SECTOR		500
#define TIMEOUT_PAGE		3500
#define TIMEOUT_PROGRAM		10

/* Firmware upgrade */
#define DDI_BK_ST_ADDR			0x1E000
#define DDI_BK_END_ADDR			0x1FFFF
#define DDI_BK_SIZE				1*K
#define RSV_BK_ST_ADDR				0x1E000
#define RSV_BK_END_ADDR				0x1E3FF
#define FW_VER_ADDR				0xFFE0
#define FW_BLOCK_INFO_NUM			17
#define INFO_HEX_ST_ADDR			0x4F
#define INFO_MP_HEX_ADDR                  0x1F
#define HW_CRC_MAX_LEN 				0x1FFFF

/* Firmware upgrade */
#define DLM_START_ADDRESS			0x20610   //TODOcompile error

#define DUMP_FLASH_PATH				"/sdcard/flash_dump"
#define DUMP_IRAM_PATH				"/sdcard/iram_dump"
struct bin_fw_info info_from_bin;
static struct touch_fw_data {
	u8 block_number;
	u32 start_addr;
	u32 end_addr;
	u32 new_fw_cb;
	u32 bin_fw_ver;
	u32 bin_fw_build_ver;
	bool is80k;
	int hex_tag;
} tfd;

static struct flash_block_info {
	char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
} fbi[FW_BLOCK_INFO_NUM];


static u8 *pfw = NULL;

u32 ic_fw_ver;
u8 ic_fw_build_ver;
u8 bin_fw_build_ver;
u32 fw_end_addr;
u32 fw_start_addr;

int g_update_percentage = 0;

static int ili7807q_iram_read(struct device *dev, u8 *buf, u32 start, int len)
{
	int i, limit = 4*K;
	int addr = 0, end = len - 1;
//	u8 cmd[4] = {0};

	TOUCH_TRACE();

	if (!buf) {
		TOUCH_E("buf is null\n");
		return -ENOMEM;
	}

	for (i = 0, addr = start; addr < end; i += limit, addr += limit) {
		if ((addr + limit) > len)
			limit = end % limit;

		if (ili7807q_ice_reg_read(dev, addr, buf + i, limit) < 0) {
			TOUCH_E("Failed to Read iram data\n");
			return -ENODEV;
		}

		g_update_percentage = (i * 100) / end;
		TOUCH_D(FW_UPGRADE, "Reading iram data .... %d%c", g_update_percentage, '%');
	}
	return 0;
}

static int ili7807q_int_info_ctrl(struct device *dev, bool keep_status)
{
	int ret = 0;
	u8 data[2] = {0};

	data[0] = CONTROL_INT_INFO;
	data[1] = keep_status ? 1 : 0; // 0x1 : get info by int, 0x0 : get info direct

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write int info ctrl cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ? "int" : "no int");

	return ret;
}

void ili7807q_dump_iram_data(struct device *dev, u32 start, u32 end)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u8 *fw_buf = NULL;
    int ret =0;
	int len = 0;

	TOUCH_TRACE();

	g_update_percentage = FW_STAT_INIT;

	ret = ili7807q_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Failed to enter ICE mode, ret = %d\n", ret);
		goto out;
	}

	len = end - start + 1;

	if (len > MAX_HEX_FILE_SIZE) {
		TOUCH_E("len is larger than buffer, abort\n");
		ret = -EINVAL;
		goto out;
	}

	fw_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL);
	if (ERR_ALLOC_MEM(fw_buf)) {
		TOUCH_E("Failed to allocate update_buf\n");
		ret = -ENOMEM;
		goto out;
	}

    memset(fw_buf,0xFF,MAX_HEX_FILE_SIZE);

	ret = ili7807q_iram_read(dev, fw_buf, start, len);
	if (ret < 0)
		goto out;

	f = filp_open(DUMP_IRAM_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		TOUCH_E("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(f, fw_buf, len, &pos);
	set_fs(old_fs);
	filp_close(f, NULL);
	TOUCH_I("Save iram data to %s\n", DUMP_IRAM_PATH);

out:
	ret = ili7807q_ice_mode_disable(dev);
	TOUCH_I("Dump IRAM %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	g_update_percentage = (ret < 0) ? FW_UPDATE_FAIL : FW_UPDATE_PASS;
	ipio_kfree((void **)&fw_buf);
}

static u32 HexToDec(char *phex, s32 len)
{
	u32 ret = 0, temp = 0, i;
	s32 shift = (len - 1) * 4;

	for (i = 0; i < len; shift -= 4, i++) {
		if ((phex[i] >= '0') && (phex[i] <= '9'))
			temp = phex[i] - '0';
		else if ((phex[i] >= 'a') && (phex[i] <= 'f'))
			temp = (phex[i] - 'a') + 10;
		else if ((phex[i] >= 'A') && (phex[i] <= 'F'))
			temp = (phex[i] - 'A') + 10;
		else
			return -1;

		ret |= (temp << shift);
	}
	return ret;
}

static int CalculateCRC32(u32 start_addr, u32 len, u8 *pfw)
{
	int i = 0, j = 0;
	int crc_poly = 0x04C11DB7;
	int tmp_crc = 0xFFFFFFFF;

	for (i = start_addr; i < start_addr + len; i++) {
		tmp_crc ^= (pfw[i] << 24);

		for (j = 0; j < 8; j++) {
			if ((tmp_crc & 0x80000000) != 0)
				tmp_crc = tmp_crc << 1 ^ crc_poly;
			else
				tmp_crc = tmp_crc << 1;
		}
	}
	return tmp_crc;
}

static int ilitek_tddi_fw_iram_read(struct device *dev,u8 *buf, u32 start, int len)
{
	int limit = SPI_BUF_SIZE;
	int addr = 0, loop = 0, tmp_len = len, cnt = 0;

	TOUCH_TRACE();

	if (!buf) {
		TOUCH_E("buf is null\n");
		return -ENOMEM;
	}

	if (len % limit)
		loop = (len / limit) + 1;
	else
		loop = len / limit;

	for (cnt = 0, addr = start; cnt < loop; cnt++, addr += limit) {
		if (tmp_len > limit)
			tmp_len = limit;


		if (ili7807q_ice_reg_read(dev, addr, buf + cnt * limit, tmp_len) < 0)
			TOUCH_E("Read iram busy error\n");

		tmp_len = len - cnt * limit;
		g_update_percentage = ((len - tmp_len) * 100) / len;
		TOUCH_E("Reading iram data .... %d%c", g_update_percentage, '%');
	}
	return 0;
}
static int calc_hw_dma_crc(struct device *dev,u32 start_addr, u32 block_size)
{
	int count = 50;
	u32 busy = 0;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	TOUCH_TRACE();

	if (d->dma_reset) {
		TOUCH_D(FW_UPGRADE,"operate dma reset in reg after tp reset\n");
		if (ili7807q_ice_reg_write(dev,0x40040, 0x00800000, 4) < 0){
			TOUCH_E("Failed to open DMA reset\n");
			busy = -EFW_CRC;
			goto out;
		}
		if (ili7807q_ice_reg_write(dev,0x40040, 0x00000000, 4) < 0){			
			TOUCH_E("Failed to close DMA reset\n");
			busy = -EFW_CRC;
			goto out;
		}
	}
	/* dma1 src1 address */
	if (ili7807q_ice_reg_write(dev,0x072104, start_addr, 4) < 0){
		TOUCH_E("Write dma1 src1 address failed\n");
		busy = -EFW_CRC;
		goto out;
	}
	/* dma1 src1 format */
	if (ili7807q_ice_reg_write(dev,0x072108, 0x80000001, 4) < 0){
		TOUCH_E("Write dma1 src1 format failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* dma1 dest address */
	if (ili7807q_ice_reg_write(dev,0x072114, 0x0002725C, 4) < 0){
		TOUCH_E("Write dma1 src1 format failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* dma1 dest format */
	if (ili7807q_ice_reg_write(dev,0x072118, 0x80000000, 4) < 0){
		TOUCH_E("Write dma1 dest format failed\n");
    	busy = -EFW_CRC;
	    goto out;	
	}
	/* Block size*/
	if (ili7807q_ice_reg_write(dev,0x07211C, block_size, 4) < 0){
		TOUCH_E("Write block size (%d) failed\n", block_size);
    	busy = -EFW_CRC;
	    goto out;
	}
	/* crc off */
	if (ili7807q_ice_reg_write(dev,0x041016, 0x00, 1) < 0){
		TOUCH_E("Write crc of failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* dma crc */
	if (ili7807q_ice_reg_write(dev,0x041017, 0x03, 1) < 0){
		TOUCH_E("Write dma 1 crc failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* crc on */
	if (ili7807q_ice_reg_write(dev,0x041016, 0x01, 1) < 0){
		TOUCH_E("Write crc on failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* Dma1 stop */
	if (ili7807q_ice_reg_write(dev,0x072100, 0x02000000, 4) < 0){
		TOUCH_E("Write dma1 stop failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* clr int */
	if (ili7807q_ice_reg_write(dev,0x048006, 0x2, 1) < 0){
		TOUCH_E("Write clr int failed\n");
    	busy = -EFW_CRC;
	    goto out;
	}
	/* Dma1 start */
	if (ili7807q_ice_reg_write(dev,0x072100, 0x01000000, 4) < 0){
		TOUCH_E("Write dma1 start failed\n");
    	busy = -EFW_CRC;
	    goto out;	
	}

	/* Polling BIT0 */
	while (count > 0) {//check
		touch_msleep(1);
		if (ili7807q_ice_reg_read(dev,0x048006, &busy, sizeof(u8)) < 0)
			TOUCH_E("Read busy error\n");
		TOUCH_E("busy = %x\n", busy);
		if ((busy & 0x02) == 2)
			break;
		count--;
	}

	if (count <= 0) {
		TOUCH_E("BIT0 is busy\n");
		return -1;
	}

	if (ili7807q_ice_reg_read(dev,0x04101C, &busy, sizeof(u32)) < 0) {
		TOUCH_E("Read dma crc error\n");
		return -1;
	}
out:	
	return busy;
}
static int ilitek_tddi_fw_iram_program(struct device *dev,u32 start, u8 *w_buf, u32 w_len, u32 split_len)
{
	int i = 0, j = 0, addr = 0;
	u32 end = start + w_len;
	bool fix_4_alignment = false;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	TOUCH_TRACE();

	if (split_len != 0) {
        if ((split_len&3)>0)
		    TOUCH_E("Since split_len must be four-aligned, it must be a multiple of four");

		for (addr = start, i = 0; addr < end; addr += split_len, i += split_len) {
			if ((addr + split_len) > end) {
				split_len = end - addr;
				if ((split_len&3)!= 0)
					fix_4_alignment = true;
			}

			d->update_buf[0] = SPI_WRITE;
			d->update_buf[1] = 0x25;
			d->update_buf[2] = (char)((addr & 0x000000FF));
			d->update_buf[3] = (char)((addr & 0x0000FF00) >> 8);
			d->update_buf[4] = (char)((addr & 0x00FF0000) >> 16);

			for (j = 0; j < split_len; j++)
				d->update_buf[5 + j] = w_buf[i + j];

			if (fix_4_alignment) {
				TOUCH_D(FW_UPGRADE,"org split_len = 0x%X\n", split_len);
				TOUCH_D(FW_UPGRADE,"idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 4, d->update_buf[5 + split_len - 4]);
				TOUCH_D(FW_UPGRADE,"idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 3, d->update_buf[5 + split_len - 3]);
				TOUCH_D(FW_UPGRADE,"idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 2, d->update_buf[5 + split_len - 2]);
				TOUCH_D(FW_UPGRADE,"idev->update_buf[5 + 0x%X] = 0x%X\n", split_len - 1, d->update_buf[5 + split_len - 1]);
				for (j = 0; j < (4 - (split_len&3)); j++) {
					d->update_buf[5 + j + split_len] = 0xFF;
					TOUCH_D(FW_UPGRADE,"idev->update_buf[5 + 0x%X] = 0x%X\n",j + split_len, d->update_buf[5 + j + split_len]);
				}

				TOUCH_D(FW_UPGRADE,"split_len %% 4 = %d\n", split_len&3);
				split_len = split_len + (4 - (split_len % 4));
				TOUCH_D(FW_UPGRADE,"fix split_len = 0x%X\n", split_len);
			}
			if (ili7807q_host_download_write(dev,d->update_buf,split_len+5)) {
				TOUCH_I("Failed to write data via SPI in host download (%x)\n", split_len + 5);
				return -EIO;
			}
			g_update_percentage = (i * 100) / w_len;

			if (g_update_percentage > FW_UPDATING)
				g_update_percentage = FW_UPDATING;

		}
	} else {
        memset(d->update_buf,0xFF,MAX_HEX_FILE_SIZE);

		d->update_buf[0] = SPI_WRITE;
		d->update_buf[1] = 0x25;
		d->update_buf[2] = (char)((start & 0x000000FF));
		d->update_buf[3] = (char)((start & 0x0000FF00) >> 8);
		d->update_buf[4] = (char)((start & 0x00FF0000) >> 16);

		memcpy(&d->update_buf[5], w_buf, w_len);
		if ((w_len&3) != 0) {
			TOUCH_I("org w_len = %d\n", w_len);
			w_len = w_len + (4 - (w_len % 4));
			TOUCH_I("w_len = %d w_len %% 4 = %d\n", w_len, (w_len&3));
		}
		/* It must be supported by platforms that have the ability to transfer all data at once. */
		if (ili7807q_host_download_write(dev,d->update_buf,w_len+5) < 0) {
			TOUCH_E("Failed to write data via SPI in host download (%x)\n", w_len + 5);
			return -EIO;
		}
	}
	return 0;
}
static int ili7807q_iram_upgrade(struct device *dev, u8 *pfw)
{
	int i, ret = UPDATE_PASS;
	u32 mode, crc, dma, iram_crc = 0;
	u8 *fw_ptr = NULL, crc_temp[4], crc_len = 4;
	bool iram_crc_err = false;

	struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_TRACE();


	if (d->actual_fw_mode != FIRMWARE_GESTURE_MODE) {
		ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
	}
	ret = ili7807q_ice_mode_enable(dev, MCU_STOP);
	if(ret < 0) {
		return ret;
	}
	/* Point to pfw with different addresses for getting its block data. */
	fw_ptr = pfw;
	if (d->actual_fw_mode == FIRMWARE_TEST_MODE) {

		mode = MP;
	} else if (d->actual_fw_mode == FIRMWARE_GESTURE_MODE) {
		mode = GESTURE;
		crc_len = 0;
	} else {
		mode = AP;
	}
	/* Program data to iram acorrding to each block */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].mode == mode && fbi[i].len != 0) {
			TOUCH_D(FW_UPGRADE,"Download %s code from hex 0x%x to IRAM 0x%x, len = 0x%x\n",
					fbi[i].name, fbi[i].start, fbi[i].mem_start, fbi[i].len);

			if (ilitek_tddi_fw_iram_program(dev,fbi[i].mem_start, (fw_ptr + fbi[i].start), fbi[i].len, 0/*SPI_UPGRADE_LEN*/) < 0){
				TOUCH_E("IRAM program failed\n");
				ret = -EFW_PROGRAM;
			    goto out;
			}

			crc = CalculateCRC32(fbi[i].start, fbi[i].len - crc_len, fw_ptr);
			dma = calc_hw_dma_crc(dev,fbi[i].mem_start, fbi[i].len - crc_len);

			if (mode != GESTURE) {
				ilitek_tddi_fw_iram_read(dev,crc_temp, (fbi[i].mem_start + fbi[i].len - crc_len), sizeof(crc_temp));
				iram_crc = crc_temp[0] << 24 | crc_temp[1] << 16 |crc_temp[2] << 8 | crc_temp[3];
				if (iram_crc != dma)
					iram_crc_err = true;
			}

			TOUCH_D(FW_UPGRADE,"%s CRC is %s hex(%x) : dma(%x) : iram(%x), calculation len is 0x%x\n",
				fbi[i].name,((crc != dma)||(iram_crc_err)) ? "Invalid !" : "Correct !", crc, dma, iram_crc, fbi[i].len - crc_len);

			if ((crc != dma)|| iram_crc_err) {
				TOUCH_E("CRC Error! print iram data with first 16 bytes\n");
				ili7807q_dump_iram_data(dev,0x0, 0xF);
				//return -EFW_CRC;
				ret = -EFW_CRC;
				goto out;
			}
		}
	}
	if (d->actual_fw_mode != FIRMWARE_GESTURE_MODE) {
		if (ili7807q_reset_ctrl(dev, SW_RESET) < 0) {
			TOUCH_E("TP Code reset failed during iram programming\n");
			ret = -EFW_REST;
			//return ret;
			goto out;
		}
	}
out:
	if (ili7807q_ice_mode_disable(dev) <0) {
		TOUCH_E("Disable ice mode failed after code reset\n");
		ret = -EFW_REST;
		return ret;
	}
	touch_msleep(50);/*original value 100 */
	ili7807q_int_info_ctrl(dev,true);
	return ret;
}

static int calc_file_crc(u8 *pfw)
{
	int i;
	u32 ex_addr, data_crc, file_crc;

	TOUCH_TRACE();

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;
		ex_addr = fbi[i].end;

		data_crc = CalculateCRC32(fbi[i].start, fbi[i].len - 4, pfw);
		TOUCH_I("fbi[%d].start = 0x%x, end = 0x%x, len = 0x%x\n", i, fbi[i].start, fbi[i].end, fbi[i].len);
		file_crc = pfw[ex_addr - 3] << 24 | pfw[ex_addr - 2] << 16 | pfw[ex_addr - 1] << 8 | pfw[ex_addr];
		TOUCH_I("data crc = %x, file crc = %x\n", data_crc, file_crc);
		if (data_crc != file_crc) {
			TOUCH_E("Content of fw file is broken. (%d, %x, %x)\n",
				i, data_crc, file_crc);
			return -1;
		}
	}

	TOUCH_I("Content of fw file is correct\n");
	return 0;
}

static int fw_file_convert(u8 *phex, int size, u8 *pfw)
{
	int block = 0;
	u32 i = 0, j = 0, k = 0, num = 0;
	u32 len = 0, addr = 0, type = 0;
	u32 start_addr = 0x0, end_addr = 0x0, ex_addr = 0;
	u32 offset;

	TOUCH_TRACE();

	memset(fbi, 0x0, sizeof(fbi));

	/* Parsing HEX file */
	for (; i < size;) {
		len = HexToDec(&phex[i + 1], 2);
		addr = HexToDec(&phex[i + 3], 4);
		type = HexToDec(&phex[i + 7], 2);

		if (type == 0x04) {
			ex_addr = HexToDec(&phex[i + 9], 4);
		} else if (type == 0x02) {
			ex_addr = HexToDec(&phex[i + 9], 4);
			ex_addr = ex_addr >> 12;
		} else if (type == BLOCK_TAG_AF) {
			/* insert block info extracted from hex */
			tfd.hex_tag = type;
			if (tfd.hex_tag == BLOCK_TAG_AF)
				num = HexToDec(&phex[i + 9 + 6 + 6], 2);
			else
				num = 0xFF;

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				TOUCH_E("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].start = HexToDec(&phex[i + 9], 6);
			fbi[num].end = HexToDec(&phex[i + 9 + 6], 6);
			fbi[num].fix_mem_start = INT_MAX;
			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			TOUCH_I("Block[%d]: start_addr = %x, end = %x", num, fbi[num].start, fbi[num].end);

			block++;
		} else if (type == BLOCK_TAG_B0 && tfd.hex_tag == BLOCK_TAG_AF) {
			num = HexToDec(&phex[i + 9 + 6], 2);

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				TOUCH_E("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].fix_mem_start = HexToDec(&phex[i + 9], 6);
			TOUCH_I("Tag 0xB0: change Block[%d] to addr = 0x%x\n", num, fbi[num].fix_mem_start);
		}

		addr = addr + (ex_addr << 16);

		if (phex[i + 1 + 2 + 4 + 2 + (len * 2) + 2] == 0x0D)
			offset = 2;
		else
			offset = 1;

		if (addr > MAX_HEX_FILE_SIZE) {
			TOUCH_E("Invalid hex format %d\n", addr);
			return -1;
		}

		if (type == 0x00) {
			end_addr = addr + len;
			if (addr < start_addr)
				start_addr = addr;
			/* fill data */
			for (j = 0, k = 0; j < (len * 2); j += 2, k++)
				pfw[addr + k] = HexToDec(&phex[i + 9 + j], 2);
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}

	if (calc_file_crc(pfw) < 0)
		return -1;

	tfd.start_addr = start_addr;
	tfd.end_addr = end_addr;
	tfd.block_number = block;
	return 0;
}


static void update_block_info(u8 *pfw)
{
	u32 ges_area_section = 0, ges_info_addr = 0, ges_fw_start = 0, ges_fw_end = 0;
	u32 ap_end = 0, ap_len = 0;
	u32 fw_info_addr = 0, fw_mp_ver_addr = 0;

	if (tfd.hex_tag != BLOCK_TAG_AF) {
		TOUCH_E("HEX TAG is invalid (0x%X)\n", tfd.hex_tag);
		return ;//-EINVAL;
	}
	TOUCH_TRACE();
	fbi[AP].mem_start = (fbi[AP].fix_mem_start != INT_MAX) ? fbi[AP].fix_mem_start : 0;
	fbi[DATA].mem_start = (fbi[DATA].fix_mem_start != INT_MAX) ? fbi[DATA].fix_mem_start : DLM_START_ADDRESS;
	fbi[TUNING].mem_start = (fbi[TUNING].fix_mem_start != INT_MAX) ? fbi[TUNING].fix_mem_start :  fbi[DATA].mem_start + fbi[DATA].len;
	fbi[MP].mem_start = (fbi[MP].fix_mem_start != INT_MAX) ? fbi[MP].fix_mem_start : 0;
	fbi[GESTURE].mem_start = (fbi[GESTURE].fix_mem_start != INT_MAX) ? fbi[GESTURE].fix_mem_start :	0;
	fbi[TAG].mem_start = (fbi[TAG].fix_mem_start != INT_MAX) ? fbi[TAG].fix_mem_start : 0;
	fbi[PARA_BACKUP].mem_start = (fbi[PARA_BACKUP].fix_mem_start != INT_MAX) ? fbi[PARA_BACKUP].fix_mem_start : 0;
	fbi[DDI].mem_start = (fbi[DDI].fix_mem_start != INT_MAX) ? fbi[DDI].fix_mem_start : 0;

	/* Parsing gesture info form AP code */
	ges_info_addr = (fbi[AP].end + 1 - 60);
	ges_area_section = (pfw[ges_info_addr + 3] << 24) + (pfw[ges_info_addr + 2] << 16) + (pfw[ges_info_addr + 1] << 8) + pfw[ges_info_addr];
	fbi[GESTURE].mem_start = (pfw[ges_info_addr + 7] << 24) + (pfw[ges_info_addr + 6] << 16) + (pfw[ges_info_addr + 5] << 8) + pfw[ges_info_addr + 4];
	ap_end = (pfw[ges_info_addr + 11] << 24) + (pfw[ges_info_addr + 10] << 16) + (pfw[ges_info_addr + 9] << 8) + pfw[ges_info_addr + 8];

	if (ap_end != fbi[GESTURE].mem_start)
		ap_len = ap_end - fbi[GESTURE].mem_start + 1;

	ges_fw_start = (pfw[ges_info_addr + 15] << 24) + (pfw[ges_info_addr + 14] << 16) + (pfw[ges_info_addr + 13] << 8) + pfw[ges_info_addr + 12];
	ges_fw_end = (pfw[ges_info_addr + 19] << 24) + (pfw[ges_info_addr + 18] << 16) + (pfw[ges_info_addr + 17] << 8) + pfw[ges_info_addr + 16];

	if (ges_fw_end != ges_fw_start)
		fbi[GESTURE].len = ges_fw_end - ges_fw_start;

	/* update gesture address */
	fbi[GESTURE].start = ges_fw_start;

	TOUCH_I("==== Gesture loader info ====\n");
	TOUCH_I("gesture move to ap addr => start = 0x%x, ap_end = 0x%x, ap_len = 0x%x\n", fbi[GESTURE].mem_start, ap_end, ap_len);
	TOUCH_I("gesture hex addr => start = 0x%x, gesture_end = 0x%x, gesture_len = 0x%x\n", ges_fw_start, ges_fw_end, fbi[GESTURE].len);
	TOUCH_I("=============================\n");

        fbi[AP].name = "AP";
        fbi[DATA].name = "DATA";
        fbi[TUNING].name = "TUNING";
        fbi[MP].name = "MP";
        fbi[GESTURE].name = "GESTURE";
        fbi[TAG].name = "TAG";
        fbi[PARA_BACKUP].name = "PARA_BACKUP";
        fbi[DDI].name = "DDI";

        /* upgrade mode define */
        fbi[DATA].mode = fbi[AP].mode = fbi[TUNING].mode = AP;
        fbi[MP].mode = MP;
        fbi[GESTURE].mode = GESTURE;

	if (fbi[AP].end > (64*K))
		tfd.is80k = true;

	/* Copy fw info  */
	fw_info_addr = fbi[AP].end - INFO_HEX_ST_ADDR;
	TOUCH_I("Parsing hex info start addr = 0x%x\n", fw_info_addr);
	ipio_memcpy(info_from_bin.fw_info, (pfw + fw_info_addr), sizeof(info_from_bin.fw_info), sizeof(info_from_bin.fw_info));

	/* copy fw mp ver */
	fw_mp_ver_addr = fbi[MP].end - INFO_MP_HEX_ADDR;
	TOUCH_I("Parsing hex mp ver addr = 0x%x\n", fw_mp_ver_addr);
	ipio_memcpy(info_from_bin.fw_mp_info, pfw + fw_mp_ver_addr, sizeof(info_from_bin.fw_mp_info), sizeof(info_from_bin.fw_mp_info));

	/* copy fw core ver */
	info_from_bin.core_ver= (info_from_bin.fw_info[68] << 24) | (info_from_bin.fw_info[69] << 16) |
			(info_from_bin.fw_info[70] << 8) | info_from_bin.fw_info[71];
	TOUCH_I("New FW Core version = %x\n", info_from_bin.core_ver);

	/* Get hex fw vers */
	tfd.new_fw_cb = (info_from_bin.fw_info[48] << 24) | (info_from_bin.fw_info[49] << 16) |
			(info_from_bin.fw_info[50] << 8) | info_from_bin.fw_info[51];

	/* Calculate update address */
	TOUCH_I("New FW ver = 0x%x\n", tfd.new_fw_cb);
	TOUCH_I("star_addr = 0x%06X, end_addr = 0x%06X, Block Num = %d\n", tfd.start_addr, tfd.end_addr, tfd.block_number);

	return;
}

static int open_fw_file(struct device *dev, char* fwpath, u8 *pfw)
{
	int ret =0, fsize = 0;
	const struct firmware *fw = NULL;
	u8 *temp = NULL;

	TOUCH_TRACE();

//	mm_segment_t old_fs;
//	loff_t pos = 0;

	if (request_firmware(&fw, fwpath, dev) < 0) {
		TOUCH_E("Request firmware failed, try again\n");
		if (request_firmware(&fw, fwpath, dev) < 0) {
			TOUCH_E("Request firmware failed after retry\n");
			ret = -1;
			return ret;
		}
	}

	fsize = fw->size;
	TOUCH_I("fsize = %d\n", fsize);
	if (fsize <= 0) {
		TOUCH_E("The size of file is zero\n");
		ret = -1;
		goto out;
	}

	temp = vmalloc(fsize);
	if (ERR_ALLOC_MEM(temp)) {
		TOUCH_E("Failed to allocate tp_fw by vmalloc, try again\n");
		temp = vmalloc(fsize);
		if (ERR_ALLOC_MEM(temp)) {
			TOUCH_E("Failed to allocate tp_fw after retry\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	/* Copy fw data got from request_firmware to global */
	memcpy(temp, fw->data, fsize);

	/* Convert hex and copy data from temp to pfw */
	if (fw_file_convert(temp, fsize, pfw) < 0) {
		TOUCH_E("Convert hex file failed\n");
		ret = -1;
		goto out;
	}

	update_block_info(pfw);

out:
	release_firmware(fw);
	ipio_vfree((void **)&(temp));
	return ret;
}

int ili7807q_fw_upgrade(struct device *dev, char* fwpath)
{
	int ret = 0, retry = 3;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	
	TOUCH_TRACE();

//	pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
//	if (!d->boot || d->force_fw_update || ERR_ALLOC_MEM(pfw)) {//check

		if (ERR_ALLOC_MEM(pfw)) {
			ipio_vfree((void **)&pfw);
			pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
			if (ERR_ALLOC_MEM(pfw)) {
				TOUCH_E("Failed to allocate pfw memory, %ld\n", PTR_ERR(pfw));
				ipio_vfree((void **)&pfw);
				ret = -ENOMEM;
				goto out;
			}
		}

		memset(pfw, 0xff, MAX_HEX_FILE_SIZE * sizeof(u8));

		if (open_fw_file(dev, fwpath, pfw) < 0) {
			TOUCH_E("Open FW file fail\n");
			ipio_vfree((void **)&pfw);
			ret = -ENOMEM;
			goto out;
		}
//	}//check
	d->hex_info = &info_from_bin;
	do {
		ret = ili7807q_iram_upgrade(dev, pfw);
		if (ret == UPDATE_PASS)
			break;

		TOUCH_E("Upgrade failed, do retry!\n");
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		TOUCH_E("Upgrade failed, earse iram!\n");
		ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
		goto out;
	}

out:
	//ipio_vfree((void **)&pfw);
	return ret;
}

#if defined(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
static int open_app_fw_file(struct device *dev, u8 *pfw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	const struct firmware *fw = NULL;
	struct firmware temp_fw;
	int fsize = 0;
	u8 *temp = NULL;
	int ret = 0;

	TOUCH_TRACE();

	if ((ts->app_fw_upgrade.offset == 0) || (ts->app_fw_upgrade.data == NULL)) {
		TOUCH_I("app_fw_upgrade.offset = %d, app_fw_upgrade.data = %p\n",
				(int)ts->app_fw_upgrade.offset, ts->app_fw_upgrade.data);
		return -EPERM;
	}

	memset(&temp_fw, 0, sizeof(temp_fw));
	temp_fw.size = ts->app_fw_upgrade.offset;
	temp_fw.data = ts->app_fw_upgrade.data;

	fw = &temp_fw;

	fsize = fw->size;
	TOUCH_I("fsize = %d\n", fsize);
	if (fsize <= 0) {
		TOUCH_E("The size of file is zero\n");
		ret = -1;
		goto out;
	}

	temp = vmalloc(fsize);
	if (ERR_ALLOC_MEM(temp)) {
		TOUCH_E("Failed to allocate tp_fw by vmalloc, try again\n");
		temp = vmalloc(fsize);
		if (ERR_ALLOC_MEM(temp)) {
			TOUCH_E("Failed to allocate tp_fw after retry\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	/* Copy fw data got from request_firmware to global */
	memcpy(temp, fw->data, fsize);

	/* Convert hex and copy data from temp to pfw */
	if (fw_file_convert(temp, fsize, pfw) < 0) {
		TOUCH_E("Convert hex file failed\n");
		ret = -1;
		goto out;
	}

	update_block_info(pfw);

out:
	ipio_vfree((void **)&(temp));
	return ret;
}

int ili7807q_app_fw_upgrade(struct device *dev)
{
	int ret = 0, retry = 3;

	TOUCH_TRACE();

	pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
	if (ERR_ALLOC_MEM(pfw)) {
		TOUCH_E("Failed to allocate pfw memory, %ld\n", PTR_ERR(pfw));
		//ipio_vfree((void **)&pfw); check
		ret = -ENOMEM;
		goto out;
	}

	memset(pfw, 0xff, MAX_HEX_FILE_SIZE * sizeof(u8));

	if (open_app_fw_file(dev, pfw) < 0) {
		TOUCH_E("Open FW file fail\n");
		goto out;
	}

	do {
		ret = ili7807q_iram_upgrade(dev, pfw);
		if (ret == UPDATE_PASS)
			break;

		TOUCH_E("Upgrade failed, do retry!\n");
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		TOUCH_E("Upgrade failed, earse iram!\n");
		ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
	}

out:
	ipio_vfree((void **)&pfw);
	return ret;
}
#endif
#if 0
int ili7807q_read_flash_info(struct device *dev)
{
	int i, ret = 0;
	u8 buf[4] = {0};
	u8 cmd = 0x9F;
	u32 tmp = 0;
	u16 flash_id = 0, mid  = 0;

	TOUCH_TRACE();

	flashtab = devm_kzalloc(dev, sizeof(flash_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(flashtab)) {
		TOUCH_E("Failed to allocate flashtab memory, %ld\n", PTR_ERR(flashtab));
		ret = -ENOMEM;
		return ret;
	}

	ret = ili7807q_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Failed to enter ICE mode, ret = %d\n", ret);
		goto out;
	}
	ili7807q_ice_reg_write(dev, FLASH_BASED_ADDR, 0x0, 1);
	ili7807q_ice_reg_write(dev, FLASH1_ADDR, 0x66aa55, 3);

	ili7807q_ice_reg_write(dev, FLASH2_ADDR, cmd, 1);
	for (i = 0; i < ARRAY_SIZE(buf); i++) {
		ili7807q_ice_reg_write(dev, FLASH2_ADDR, 0xFF, 1);
		ili7807q_ice_reg_read(dev, FLASH4_ADDR, &tmp, sizeof(u8));

		buf[i] = tmp;
	}

	ili7807q_ice_reg_write(dev, FLASH_BASED_ADDR, 0x1, 1);

	mid  = buf[0];
	flash_id = buf[1] << 8 | buf[2];

	for (i = 0; i < ARRAY_SIZE(flash_t); i++) {
		if (mid  == flash_t[i].mid && flash_id == flash_t[i].dev_id) {
			flashtab->mid = flash_t[i].mid;
			flashtab->dev_id = flash_t[i].dev_id;
			flashtab->program_page = flash_t[i].program_page;
			flashtab->sector = flash_t[i].sector;
			break;
		}
	}

	if (i >= ARRAY_SIZE(flash_t)) {
		TOUCH_I("Not found flash id in tab, use default\n");
		flashtab->mid = flash_t[0].mid;
		flashtab->dev_id = flash_t[0].dev_id;
		flashtab->program_page = flash_t[0].program_page;
		flashtab->sector = flash_t[0].sector;
	}

	TOUCH_I("Flash MID = %x, Flash DEV_ID = %x\n", flashtab->mid , flashtab->dev_id);
	TOUCH_I("Flash program page = %d\n", flashtab->program_page);
	TOUCH_I("Flash sector = %d\n", flashtab->sector);

	ili7807q_flash_protect(dev, false);

out:
	ret = ili7807q_ice_mode_disable(dev);
	return ret;
}
#endif
