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
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
#include <soc/qcom/lge/board_lge.h>
#endif
#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
#include <soc/mediatek/lge/board_lge.h>
#endif
#include <touch_hwif.h>
#include <touch_core.h>

#include "touch_ili7807q.h"
#include "touch_ili7807q_mp.h"
#include "touch_ili7807q_fw.h"

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
#include <linux/msm_lcd_power_mode.h>
#endif
#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
//#include <linux/version.h>
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0)
#include <linux/lcd_power_mode.h>
//#else
//Todo
//#endif
#endif

#if 1//driver code modify not to modify core code
#define SWIPE_R SWIPE_R2
#define SWIPE_L SWIPE_L2
#endif

#ifndef LCD_POWER_H
typedef enum{
	DEEP_SLEEP_ENTER =0,
    DEEP_SLEEP_EXIT,
	DSV_TOGGLE,
	DSV_ALWAYS_ON,
}DISP_SET_POWER_NODE;
#endif

int ili7807q_sense_ctrl(struct device *dev, bool start);

static void project_param_set(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);

	/* incell touch driver have no authority to control vdd&vio power pin */
	d->p_param.touch_power_control_en = FUNC_OFF;

	/* TODO */
	d->p_param.touch_maker_id_control_en = FUNC_OFF;

	/* Use DSV Toggle mode */
	d->p_param.dsv_toggle = FUNC_OFF;

	/* Use Power control when deep sleep */
	d->p_param.deep_sleep_power_control = FUNC_ON;

	/* TODO */
	d->p_param.dump_packet = FUNC_ON;
}
// Definitions for Debugging Failure Reason in LPWG
enum {
	FAIL_REASON_OFF = 0,
	FAIL_REAL_TYPE,
	FAIL_BUFFER_TYPE,
};
static const char *fail_reason_type_str[] = {
	"Disable Type",
	"Always Report Type",
	"Buffer Type",
};

#define TCI_FR_BUF_LEN	10
#define TCI_FR_NUM	8

//"NUM" string must be last of the string array
static const char const *tci_debug_str[TCI_FR_NUM] = {
	[0] = "SUCCESS",
	[1] = "DISTANCE_INTER_TAP",
	[2] = "DISTANCE_TOUCHSLOP",
	[3] = "TIMEOUT_INTER_TAP",
	[4] = "MULTI_FINGER",
	[5] = "DELAY_TIME", //Over Tap
	[6] = "PALM_STATE",
	[7] = "OUT_OF_AREA",
};

#define SWIPE_FAILREASON_BUF_LEN    10
#define SWIPE_FAILREASON_DIR_NUM    4
#define SWIPE_FAILREASON_NUM 	9

//"NUM" string must be last of the string array
static const char const *swipe_debug_direction_str[SWIPE_FAILREASON_DIR_NUM] = {
	[0] = "SWIPE_DOWN",
	[1] = "SWIPE_UP",
	[2] = "SWIPE_LEFT",
	[3] = "SWIPE_RIGHT",
};

//"NUM" string must be last of the string array
static const char const *swipe_debug_str[SWIPE_FAILREASON_NUM] = {
	[0] = "SUCCESS",
	[1] = "FINGER_FAST_RELEASE",
	[2] = "MULTI_FINGER",
	[3] = "FAST_SWIPE",
	[4] = "SLOW_SWIPE",
	[5] = "WRONG_DIRECTION",
	[6] = "RATIO_FAIL",
	[7] = "OUT_OF_AREA",
	[8] = "PALM",
};
#define LCD_POWER_MODE_NUM 4
static const char const *lcd_power_mode_str[LCD_POWER_MODE_NUM] = {
	[0] = "DEEP_SLEEP_ENTER",
	[1] = "DEEP_SLEEP_EXIT",
	[2] = "DSV_TOGGLE",
	[3] = "DSV_ALWAYS_ON",
};

//static const char const *lcd_power_mode_str[LCD_POWER_MODE_NUM] = { };
#ifdef DEEP_SLEEP_CTRL
static int ili7807q_wakeup_by_cmd(struct device *dev)
{
	u8 data[2] = {0};
	//struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	 int ret =0;
	if((ts->lpwg.mode != LPWG_NONE
			|| !ts->swipe[SWIPE_D].enable
			|| !ts->swipe[SWIPE_U].enable
			|| !ts->swipe[SWIPE_L].enable
			|| !ts->swipe[SWIPE_R].enable)
			&&(atomic_read(&ts->state.sleep) != IC_DEEP_SLEEP)){
			data[0] = CONTROL_LPWG;
			data[1] = 0x00; // exit gesture
	}
	else {
			data[0] = CONTROL_SLEEP;
			data[1] = 0x01; // sleep out
	}
	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0) {
	   TOUCH_E("failed to wakeup_by_cmd\n");
	return ret;
	}
	touch_msleep(1);
//	ret = ili7807q_sense_ctrl(dev, false);
	ret = ili7807q_sense_ctrl(dev, true);
	return ret;
}
#endif
static int ili7807q_swipe_tunning(struct device *dev, bool enable)
{
 struct touch_core_data *ts = to_touch_core(dev);
 struct swipe_ctrl *ctrl[ILI7807Q_SWIPE_NUM] = { /* U, D, L, R */
        &ts->swipe[SWIPE_U],
        &ts->swipe[SWIPE_D],
        &ts->swipe[SWIPE_L],
        &ts->swipe[SWIPE_R],
 };
 int i, ret = 0;
 u8 index =0;
 uint8_t data[ILI7807Q_SWIPE_INFO_NUM] = {0x0, };
 TOUCH_I("%s: SWIPE U(%d),D(%d),L(%d),R(%d)\n", __func__,
		 ctrl[ILI7807Q_SWIPE_U]->enable, ctrl[ILI7807Q_SWIPE_D]->enable, ctrl[ILI7807Q_SWIPE_L]->enable, ctrl[ILI7807Q_SWIPE_R]->enable);

	data[0] = CONTROL_LPWG;
	data[1] = 0x12; // TCI / SWIPE enable
	data[2] = 0x03;
	index = 3;
 if (enable) {
      for (i = 0; i < ILI7807Q_SWIPE_NUM; i++) {
           if ((i == ILI7807Q_SWIPE_D)|(!ctrl[i]->enable))
                 continue;
           data[0+index] = ctrl[i]->enable << (i+1);
           data[1+index] = ctrl[i]->distance;
           data[2+index] = ctrl[i]->ratio_thres;
           data[3+index] = ctrl[i]->min_time >> 8;
           data[4+index] = ctrl[i]->min_time & 0xFF;
           data[5+index] = ctrl[i]->max_time >> 8;
           data[6+index] = ctrl[i]->max_time & 0xFF;
           data[7+index] = ctrl[i]->area.x1 >> 8;
           data[8+index] = ctrl[i]->area.x1 & 0xFF;
           data[9+index] = ctrl[i]->area.y1 >> 8;
           data[10+index] = ctrl[i]->area.y1 & 0xFF;
           data[11+index] = ctrl[i]->area.x2 >> 8;
           data[12+index] = ctrl[i]->area.x2 & 0xFF;
           data[13+index] = ctrl[i]->area.y2 >> 8;
           data[14+index] = ctrl[i]->area.y2 & 0xFF;
           data[15+index] = ctrl[i]->start_area.x1 >> 8;
           data[16+index] = ctrl[i]->start_area.x1 & 0xFF;
           data[17+index] = ctrl[i]->start_area.y1 >> 8;
           data[18+index] = ctrl[i]->start_area.y1 & 0xFF;
           data[19+index] = ctrl[i]->start_area.x2 >> 8;
           data[20+index] = ctrl[i]->start_area.x2 & 0xFF;
           data[21+index] = ctrl[i]->start_area.y2 >> 8;
           data[22+index] = ctrl[i]->start_area.y2 & 0xFF;
           data[23+index] = ctrl[i]->wrong_dir_thres;
           data[24+index] = ctrl[i]->init_ratio_chk_dist;
           data[25+index] = ctrl[i]->init_ratio_thres;
     	   //for debug Touch
		   ili7807q_dump_packet(data, 8, sizeof(data), 0, "swipe cmd");
	       ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
           if (ret < 0) {
               TOUCH_E("failed to register write %d (ret: %d)\n", i, ret);
               goto error;
            }
        }
    }
error:
 return ret;
}

static int ili_spi_pll_clk_wakeup(struct device *dev)
{
	int index = 0;
	int ret = 0;
	u8 wdata[32] = {0};
	u8 wakeup[9] = {0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3};
	u32 wlen = sizeof(wakeup);
	struct touch_bus_msg msg = {0, };

	wdata[0] = SPI_WRITE;
	wdata[1] = wlen >> 8;
	wdata[2] = wlen & 0xff;
	index = 3;
	wlen += index;

	ipio_memcpy(&wdata[index], wakeup, wlen, wlen);

	TOUCH_I("Write dummy to wake up spi pll clk\n");
	msg.tx_buf = &wdata[0];
	msg.tx_size = wlen;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	ret = touch_bus_write(dev, &msg);
	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
	}
	touch_msleep(1);
	return ret;
}

static int ili7807q_normal_recovery(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	int ret =0;
	TOUCH_I("Doing normal spi recovery\n");
	if(d->get_path) {
		ret = ili7807q_fw_upgrade(dev, d->fwpath);
		if (ret < 0) {
			TOUCH_E("FW upgrade failed during gesture recovery ret =%d\n", ret);
		}
		else {
			TOUCH_I("do normal recovery,init ic again\n");
			ts->driver->init(dev);
		}
	}
	else {
		TOUCH_E("not get fw path\n");
	}
	return ret;
}
static int ili7807q_gesture_recovery(struct device *dev)
{
	int ret = 0, retry = 100;
	u32 answer = 0;
	int ges_pwd_addr = SPI_ESD_GESTURE_CORE146_PWD_ADDR;
	int ges_pwd = ESD_GESTURE_CORE146_PWD;
	int ges_run = SPI_ESD_GESTURE_CORE146_RUN;
	int pwd_len = 2;
	u32 core_ver = 0x0;

	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_core_info *core_info = &d->ic_info.core_info;
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_I("Doing gesture spi recovery\n");
	core_ver = COM_3BYTE(core_info->code_base, core_info->minor, core_info->revision_major);
	if (core_ver < CORE_VER_1460) {
		if (core_ver >= CORE_VER_1420)
			ges_pwd_addr = I2C_ESD_GESTURE_PWD_ADDR;
		else
			ges_pwd_addr = SPI_ESD_GESTURE_PWD_ADDR;
		ges_pwd = ESD_GESTURE_PWD;
		ges_run = SPI_ESD_GESTURE_RUN;
		pwd_len = 4;
	}

	TOUCH_I("ESD Gesture PWD Addr = 0x%X, PWD = 0x%X, GES_RUN = 0%X, core_ver = 0x%X\n",
		ges_pwd_addr, ges_pwd, ges_run,core_ver);

	ret = ili7807q_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Enable ice mode failed during gesture recovery\n");
		goto fail;
	}

	/* write a special password to inform FW go back into gesture mode */
	ret = ili7807q_ice_reg_write(dev,ges_pwd_addr, ges_pwd, pwd_len);
	if (ret < 0) {
		TOUCH_E("write password failed\n");
		goto fail;
	}

	/* Host download gives effect to FW receives password successed */
	d->actual_fw_mode= FIRMWARE_DEMO_MODE;
	if(d->get_path) {
		ret = ili7807q_fw_upgrade(dev, d->fwpath);
		if (ret < 0) {
			TOUCH_E("FW upgrade failed during gesture recovery ret =%d\n", ret);
			goto out;
		}
	}
	else {
		TOUCH_E("not get fw path\n");
		goto fail;
	}

	/* Wait for fw running code finished. */
	//if (d->info_from_hex || (core_ver >= CORE_VER_1410))
	//	touch_msleep(50);


	ret = ili7807q_ice_mode_enable(dev, MCU_STOP);
	if (ret < 0) {
		TOUCH_E("Enable ice mode failed during gesture recovery\n");
		goto fail;
	}

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		ret = ili7807q_ice_reg_read(dev,ges_pwd_addr, &answer, pwd_len);
		if (ret < 0) {
			TOUCH_E("Read gesture answer error\n");
			goto fail;
		}

		if (answer != ges_run)
			TOUCH_I("ret = 0x%X, answer = 0x%X\n", answer, ges_run);

		touch_msleep(2);
	} while (answer != ges_run && --retry > 0);

	if (retry <= 0) {
		TOUCH_E("Enter gesture failed\n");
		ret = -1;
		goto fail;
	}

	TOUCH_I("Enter gesture successfully\n");

	ret = ili7807q_ice_mode_disable(dev);
	if (ret < 0) {
		TOUCH_E("disable ice mode failed during gesture recovery\n");
		goto fail;
	}

	TOUCH_I("Gesture code loaded by %s\n","firmware");

	ili7807q_gesture_ctrl(dev, ts->lpwg.mode,
			ts->swipe[SWIPE_D].enable || ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable);
    ili7807q_gesture_failreason_ctrl(dev, d->tci_debug_type, d->swipe_debug_type);
//	ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE); -> modify
out:
    ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
	return ret;

fail:
	ret = ili7807q_ice_mode_disable(dev);
	if (ret < 0) {
		TOUCH_E("disable ice mode failed during gesture recovery\n");
	}
	ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
	return ret;
}

int ili7807q_host_download_write(struct device *dev,void *data, int size)
{
	int ret = 0;
//	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_bus_msg msg = {0, };

	TOUCH_TRACE();
	if(size > MAX_BUF_SIZE) {
		TOUCH_E("download buf over : %d\n", size);
	}
	//ipio_memcpy(&msg.tx_buf, data, size, size);
	msg.tx_buf = data;
	msg.tx_size = size;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	ret = touch_bus_write(dev, &msg);
	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
	}
	return ret;
}

static inline int ili7807q_spi_write(struct device *dev,void *data, int size)
{
	int ret = 0;
	int index = 0;
	u8 wdata[128] = {0};
	u8 checksum = 0;
	int wlen = size;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_bus_msg msg = {0, };
	bool ice = atomic_read(&d->ice_stat);
//    TOUCH_TRACE();
		/* 3 bytes data consist of length and header */
		if ((wlen+ 3) > sizeof(wdata)) {
			TOUCH_E("WARNING! wlen(%d) > wdata(%d), using wdata length to transfer\n", wlen, (int)sizeof(wdata));
			wlen= sizeof(wdata) - 4;
		}

		if (ice) {
			wdata[0] = SPI_WRITE;
			index = 1;
		} else {
			wdata[0] = SPI_WRITE;
			wdata[1] = wlen >> 8;
			wdata[2] = wlen & 0xff;
			index = 3;
		}

		ipio_memcpy(&wdata[index], data, wlen, wlen);
		wlen += index;
		if (atomic_read(&d->mp_stat) && wdata[index] == CMD_SET_CDC_INIT) {
			checksum = ili7807q_calc_data_checksum(&wdata[index], wlen- index);
			wdata[wlen] = checksum;
			wlen++;
			wdata[1] = (wlen- index) >> 8;
			wdata[2] = (wlen- index) & 0xff;
			ili7807q_dump_packet(wdata, 8, wlen, 0, "mp cdc cmd with checksum");
		}
		//ipio_memcpy(&msg.tx_buf, &wdata[0], wlen, wlen);
		msg.tx_buf = &wdata[0];
		msg.tx_size = wlen;
		msg.rx_buf = NULL;
		msg.rx_size = 0;

		ret = touch_bus_write(dev, &msg);
		if (ret < 0) {
			TOUCH_E("touch bus error : %d\n", ret);
		}
	return ret;
}
static inline int ili7807q_spi_read(struct device *dev,void *rdata, int rsize)
{
//	int ret = 0;
	int status = -1, duplex_len = 0;
	int offset = 0;

#ifdef USE_XFER
	struct touch_xfer_msg xfer;
#else
    struct touch_bus_msg msg = {0,};
//	struct touch_core_data *ts = to_touch_core(dev);
#endif
	struct ili7807q_data *d = to_ili7807q_data(dev);

#ifdef USE_XFER
	//todo
#else
	u8 cmd_head = SPI_READ;
#endif
//    TOUCH_TRACE();
	if (!atomic_read(&d->ice_stat))
		offset = 2;

	duplex_len = rsize + W_CMD_HEADER_SIZE + offset;
	if (duplex_len > SPI_BUF_SIZE)
	 {
		TOUCH_E("duplex_len is over than dma buf, abort\n");
		status = -ENOMEM;
		return status;
	}
#ifdef USE_XFER
	xfer.data[0].rx.size =  duplex_len;
	xfer.data[0].tx.data[0] = SPI_READ;
	xfer.msg_count = 1;
	status= touch_bus_xfer(dev,&xfer);
#else
	memset(d->spi_tx, 0x0, SPI_BUF_SIZE);
	memset(d->spi_rx, 0x0, SPI_BUF_SIZE);
	memcpy(d->spi_tx, &cmd_head, W_CMD_HEADER_SIZE);
	msg.tx_buf = d->spi_tx;
	msg.rx_buf = d->spi_rx;
	msg.rx_size = duplex_len;

   status = touch_bus_read(dev,&msg);
#endif
	if (status == 0){
#ifdef USE_XFER
		if (xfer.data[0].rx.data[W_CMD_HEADER_SIZE] != SPI_ACK && !atomic_read(&d->ice_stat)) {
#else
		if (d->spi_rx[W_CMD_HEADER_SIZE] != SPI_ACK && !atomic_read(&d->ice_stat)) {
#endif
			status = DO_SPI_RECOVER;
#ifdef USE_XFER
		TOUCH_E("Do spi recovery: rxbuf[1] = 0x%x, ice = %d\n", xfer.data[0].rx.data[1] , atomic_read(&d->ice_stat));
#else
	    TOUCH_E("Do spi recovery: rxbuf[1] = 0x%x, ice = %d\n", d->spi_rx[W_CMD_HEADER_SIZE] , atomic_read(&d->ice_stat));
#endif
			return status;
		}
#ifdef USER_XFER
	memcpy((u8 *)rdata, &xfer.data[0].rx.data[W_CMD_HEADER_SIZE+offset] , rsize);
#else
		memcpy((u8 *)rdata, d->spi_rx+W_CMD_HEADER_SIZE+offset , rsize);
#endif
	} else {
		TOUCH_E("spi read fail, status = %d\n", status);
	}
	return status;
}
int ili7807q_report_read(struct device *dev,void *rdata, int rsize)
{
	int ret = 0;
	TOUCH_TRACE();
	ret = ili7807q_spi_read(dev,rdata,rsize);
	if (ret <0) {
		TOUCH_E("report read fail, status = %d\n", ret);
	}
	return ret;
}
inline int ili7807q_reg_read(struct device *dev, u8 cmd, void *data, int size)
{
	//struct touch_core_data *ts = to_touch_core(dev);  // compile error unused ts
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
	u8 read_list[2];
//	TOUCH_TRACE();
	mutex_lock(&d->io_lock);

	switch (cmd) {
		case CMD_NONE:
			read_list[0] = CMD_READ_DATA_CTRL;
			ret = ili7807q_spi_write(dev,&read_list,W_CMD_HEADER_SIZE);
			if(ret < 0) {
				TOUCH_E("write reg  fail, status = %d\n", ret);
				break;
			}
		    touch_msleep(5);
			ret = ili7807q_spi_read(dev,data,size);
			if(ret < 0) {
				TOUCH_E("reg read fail, status = %d\n", ret);
			}
			break;
        case CMD_READ_DIRECT:
            touch_msleep(5);
           	ret = ili7807q_spi_read(dev,data,size);
			if(ret < 0) {
				TOUCH_E("CMD_READ_DIRECT read fail, status = %d\n", ret);
			}
			break;

		default:
			read_list[0] = cmd;
			ret = ili7807q_spi_write(dev,&read_list,W_CMD_HEADER_SIZE);
			if(ret < 0) {
				TOUCH_E("write reg  fail, status = %d\n", ret);
				break;
			}
			touch_msleep(5);
			ret = ili7807q_spi_read(dev,data,size);
			if(ret < 0) {
				TOUCH_E("reg read fail, status = %d\n", ret);
			}
			break;
	}

	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}

	//memcpy(data, &ts->rx_buf[1], size);
	mutex_unlock(&d->io_lock);
	return 0;
}

inline int ili7807q_ice_reg_read(struct device *dev, u32 addr, void* data, int size)
{
//	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
	u8 read_list[4]={0,};
//    TOUCH_TRACE();

	mutex_lock(&d->io_lock);
	read_list[0]= CMD_GET_MCU_STOP_INTERNAL_DATA;
	read_list[1] = (char)((addr & 0x000000FF) >> 0);
	read_list[2] = (char)((addr & 0x0000FF00) >> 8);
	read_list[3] = (char)((addr & 0x00FF0000) >> 16);


	ret = ili7807q_spi_write(dev,&read_list,W_ICE_HEADER_SIZE);
	if(ret < 0) {
		TOUCH_E("ICE write reg  fail, status = %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}
	//touch_msleep(1);
	ret = ili7807q_spi_read(dev,data,size);

	if(ret < 0) {
		TOUCH_E("ICE reg read fail, status = %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}
	//data = (ts->rx_buf[0] | (ts->rx_buf[1] << 8) | (ts->rx_buf[2] << 16) | (ts->rx_buf[3] << 24));
	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}
	mutex_unlock(&d->io_lock);

	return 0;
}

inline int ili7807q_reg_write(struct device *dev, u8 cmd, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
	int wlen;
//	TOUCH_TRACE();

	mutex_lock(&d->io_lock);
	if ((d->pll_clk_wakeup == true) && (d->tp_suspended == true)) {
			ret = ili_spi_pll_clk_wakeup(dev);
			if (ret < 0) {
				TOUCH_E("Wakeup pll clk error\n");
				mutex_unlock(&d->io_lock);
				return ret;
			}
	}

	ts->tx_buf[0] = cmd;
	memcpy(&ts->tx_buf[W_CMD_HEADER_SIZE], data, size);

	wlen = W_CMD_HEADER_SIZE + size;

	ret = ili7807q_spi_write(dev,ts->tx_buf,wlen);

	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}

	mutex_unlock(&d->io_lock);

	return 0;
}

inline int ili7807q_ice_reg_write(struct device *dev, u32 addr, u32 data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
	int wlen;
	int i;

	//TOUCH_TRACE();

	mutex_lock(&d->io_lock);
	ts->tx_buf[0] = CMD_GET_MCU_STOP_INTERNAL_DATA;
	ts->tx_buf[1] = (char)((addr & 0x000000FF) >> 0);
	ts->tx_buf[2] = (char)((addr & 0x0000FF00) >> 8);
	ts->tx_buf[3] = (char)((addr & 0x00FF0000) >> 16);
	//memcpy(&ts->tx_buf[W_ICE_HEADER_SIZE], data, size);
	for (i = 0; i < size; i++) {
		ts->tx_buf[W_ICE_HEADER_SIZE + i] = (char)(data >> (8 * i));
	}

	wlen= W_ICE_HEADER_SIZE + size;

	ret = ili7807q_spi_write(dev,ts->tx_buf,wlen);

	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}
	mutex_unlock(&d->io_lock);

	return 0;
}

int ili7807q_ice_mode_disable(struct device *dev)
{
	int ret = 0, retry = 0;
	u8 buf[3] = {0x62, 0x10, 0x18};
    struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_I("ICE Mode disabled\n");

	if (!atomic_read(&d->ice_stat)) {
		TOUCH_I("ice mode already disabled\n");
		return 0;
	}

	do {
		ret = ili7807q_reg_write(dev, CMD_ICE_MODE_EXIT, &buf, 3);
		if (ret >= 0)
			break;

		TOUCH_E("ice_mode disabled error %d time: %d\n", (retry + 1), ret);
	} while (++retry < 3);

	d->pll_clk_wakeup = true;
	atomic_set(&d->ice_stat, DISABLE);
    atomic_set(&d->init, IC_INIT_DONE);

	if (ret < 0) {
		TOUCH_E("ice_mode enable fail %d\n", ret);
		atomic_set(&d->ice_stat, ENABLE);
		atomic_set(&d->init, IC_INIT_NEED);
	}

	return ret;
}

int ili7807q_ice_mode_enable(struct device *dev, int mcu_status)
{
	int ret = 0, retry = 0;
	u8 buf[3] = {0x62, 0x10, 0x18}, cmd = 0x0;
	u32 pid = 0x0;
	struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_I("%s enable ice mode\n", (mcu_status == true)? "MCU stop" : "MCU on");

	if (atomic_read(&d->ice_stat)) {
		TOUCH_I("ice mode already enabled\n");
		return 0;
	}
	atomic_set(&d->ice_stat, ENABLE);
	if (mcu_status == MCU_ON)
		cmd = CMD_GET_MCU_ON_INTERNAL_DATA;
	else
		cmd = CMD_GET_MCU_STOP_INTERNAL_DATA;

	do {
		ret = ili7807q_reg_write(dev, cmd, &buf, 3);
		if (ret)
			TOUCH_E("Failed to enter ice mode, %d\n", ret);

		ret = ili7807q_ice_reg_read(dev, ILI7807q_PID_ADDR, &pid, sizeof(pid));
		if (ret)
			TOUCH_E("Failed to read chip id, %d\n", ret);

		/* Getting correct chip id can guarantee that ice mode is enabled. */
		if ((pid >> 16) == CHIP_ID) {
			TOUCH_I("ice_mode enable sucess\n");
			break;
		}
		TOUCH_E("ice_mode enable error %d time: %d\n", (retry + 1), ret);
	} while (++retry < 3);

    d->pll_clk_wakeup = false;
	if (ret) {
		atomic_set(&d->ice_stat, DISABLE);
		TOUCH_I("ice_mode enable fail\n");
	}
	return ret;
}

/* Print data for debug */
void ili7807q_dump_packet(void *data, int type, int len, int row_len, const char *name)
{
	int i, row = 32;
	u8 *p8 = NULL;
	int32_t *p32 = NULL;
	int16_t *p16 = NULL;
	u8 buf[256] = {0, };
	int offset = 0;

	if (row_len > 0)
		row = row_len;

	if (data == NULL) {
		TOUCH_E("The data going to dump is NULL\n");
		return;
	}

	if((touch_debug_mask&GET_DATA)!=GET_DATA) return;

	TOUCH_D(GET_DATA, "== Dump %s data ==\n", name);
#ifdef DEBUG_ORIGINAL
	if (type == 8)
		p8 = (u8 *) data;
	if (type == 32 || type == 10)
		p32 = (int32_t *) data;
	if (type == 16)
		p16 = (int16_t *) data;

	for (i = 0; i < len; i++) {
		if (type == 8)
			offset += snprintf(buf + offset, sizeof(buf) - offset, "%2x ", p8[i]);
		else if (type == 32)
			offset += snprintf(buf + offset, sizeof(buf) - offset, "%4x ", p32[i]);
		else if (type == 10)
			offset += snprintf(buf + offset, sizeof(buf) - offset, "%4d ", p32[i]);
		else if (type == 16)
			offset += snprintf(buf + offset, sizeof(buf) - offset, "%d ", p16[i]);

		if ((i % row) == row-1 || i == len - 1) {
			TOUCH_D(GET_DATA, "%s\n", buf);
			memset(buf, 0x0, sizeof(buf));
			offset = 0;
		}
	}
#else
	switch(type)
	{
	  case 8:
		   p8 = (u8 *) data;
		   for (i = 0; i < len; i++)
		   {
			  offset += snprintf(buf + offset, sizeof(buf) - offset, "%2x ", p8[i]);
			  if ((i % row) == row-1 || i == len - 1) {
				  TOUCH_D(GET_DATA, "%s\n", buf);
				  memset(buf, 0x0, sizeof(buf));
				  offset = 0;
				}
		   }
		   break;
	  case 16:
		   p16 = (int16_t *) data;
		   for (i = 0; i < len; i++)
		   {
			  offset += snprintf(buf + offset, sizeof(buf) - offset, "%4x ", p32[i]);
			  if ((i % row) == row-1 || i == len - 1) {
				  TOUCH_D(GET_DATA, "%s\n", buf);
				  memset(buf, 0x0, sizeof(buf));
				  offset = 0;
				}
		   }
		   break;
	  case 10:
		   p32 = (int32_t *) data;
   		   for (i = 0; i < len; i++)
		   {
			  offset += snprintf(buf + offset, sizeof(buf) - offset, "%4d ", p32[i]);
			  if ((i % row) == row-1 || i == len - 1) {
				  TOUCH_D(GET_DATA, "%s\n", buf);
				  memset(buf, 0x0, sizeof(buf));
				  offset = 0;
				}			  
		   }
		   break;
	  case 32:
		   p32 = (int32_t *) data;
   		   for (i = 0; i < len; i++)
		   {
			  offset += snprintf(buf + offset, sizeof(buf) - offset, "%4x ", p32[i]);
			  if ((i % row) == row-1 || i == len - 1) {
				  TOUCH_D(GET_DATA, "%s\n", buf);
				  memset(buf, 0x0, sizeof(buf));
				  offset = 0;
				}			  
		   }
		   break;
	  default:
		   TOUCH_E("UNKNOWN Type\n");
		   break;
	}
#endif
}

int ili7807q_sense_ctrl(struct device *dev, bool start)
{
	int ret = 0;
	u8 data[2] = {0};

	data[0] = CONTROL_SENSE;
	data[1] = start ? 1 : 0; // 0x0 : sensing stop, 0x1 : sensing start

	TOUCH_TRACE();
	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write sense cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ? "Sensing Start" : "Sensing Stop");

	return ret;
}

int ili7807q_sleep_ctrl(struct device *dev, int command)
{
	int ret = 0;
	u8 data[2] = {0};
#ifdef DEEP_SLEEP_CTRL
	struct ili7807q_data *d = to_ili7807q_data(dev);
#endif

	TOUCH_TRACE();
#ifdef DEEP_SLEEP_CTRL
	if(d->sleep_mode_choice==0) //deep sleep in
	{
		data[0] = CONTROL_SLEEP;
		data[1] = 0x03; // 0x0 : sleep in, 0x1 : sleep out, 0x3 : deep sleep in
	}
	else {
		data[0] = CONTROL_SLEEP;
		data[1] = 0x00; // 0x0 : sleep in, 0x1 : sleep out, 0x3 : deep sleep in
	}
#else
	data[0] = CONTROL_SLEEP;
	data[1] = command; // 0x0 : sleep in, 0x1 : sleep out, 0x3 : deep sleep in
#endif
	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write sleep cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ? ((data[1] == 1) ? "Sleep out" : "Sleep Deep") : "Sleep in");

	return ret;
}

int ili7807q_plug_ctrl(struct device *dev, bool plugin)
{
	int ret = 0;
	u8 data[2] = {0};

	TOUCH_TRACE();
	data[0] = CONTROL_PLUG;
	data[1] = plugin ? 0 : 1; // 0x0 : plug in, 0x1 : plug out

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write plug cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ? "Plug out" : "Plug in");

	return ret;
}

int ili7807q_gesture_ctrl(struct device *dev, int tci_mode, bool swipe_enable)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u8 data[3] = {0, }, dir = 0;

	TOUCH_TRACE();

	if (swipe_enable)
		dir = (ts->swipe[SWIPE_U].enable << 0 |  ts->swipe[SWIPE_D].enable << 1 | ts->swipe[SWIPE_L].enable << 2 | ts->swipe[SWIPE_R].enable << 3);

	data[0] = CONTROL_LPWG;
	data[1] = 0x08; // TCI / SWIPE enable
	data[2] = (tci_mode ? 0x1 : 0x0) | (dir << 1);

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));

	TOUCH_I("%s - TCI : %s, SWIPE : %s\n", __func__,
			tci_mode ? "Enable" : "Disable", swipe_enable ? "Enable" : "Disable");

	if(swipe_enable) {
		ret = ili7807q_swipe_tunning(dev,swipe_enable);
	}
	return ret;
}

int ili7807q_ime_ctrl(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u8 data[2] = {0};
	u8 ime_status = 0;

	TOUCH_TRACE();

	ime_status = atomic_read(&ts->state.ime);

	data[0] = CONTROL_IME;
	data[1] = ime_status ? 0x1 : 0x0; // 0x0 : keyboard_off, 0x1 : keyboard_on

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write keyboard(ime) cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ?  "IME On" : "IME Off");

	return ret;
}

int ili7807q_gesture_failreason_ctrl(struct device *dev, u8 tci_type, u8 swipe_type)
{
	int ret = 0;
	u8 data[6] = {0, };

	TOUCH_TRACE();

	data[0] = CONTROL_LPWG;
	data[1] = 0x10; // Failreason enable
	data[2] = 0x01; // Knock on mode
	data[3] = (tci_type | swipe_type) ? 0xFF : 0x00;
	data[4] = (tci_type | swipe_type) ? 0x01 : 0x00;
	data[5] = tci_type;
	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));

	data[2] = 0x03; // Swipe mode
	data[5] = swipe_type;
	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));

	TOUCH_I("%s - knock on: %s(%d) / swipe: %s(%d)\n", __func__, tci_type ? "Enable" : "Disable", tci_type, swipe_type ? "Enable" : "Disable", swipe_type);

	return ret;
}

void ili7807q_get_trans_touch_status(struct device *dev)
{
	u8 data[6] = {0};
	u8 cmd = CMD_GET_PANEL_INFO;
	u32 core_ver = 0x0;
	int ret = 0;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_core_info *core_info = &d->ic_info.core_info;
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	core_ver = COM_3BYTE(core_info->code_base, core_info->minor, core_info->revision_major);

	if (core_ver <= CORE_VER_1420) {
		d->trans_xy = false;
		TOUCH_I("Core ver (%x), set transfer touch as disabled\n", core_ver);
		return;
	}

	ret = ili7807_get_fw_data_wrapper(dev, &cmd, 0, data, sizeof(data));
	if (ret < 0)
		TOUCH_E("Write panel cmd error\n");

	ili7807q_dump_packet(data, 8, sizeof(data), 0, "Transfer coodination");

	d->trans_xy = data[5];

	d->panel_wid = ts->caps.max_x;
	d->panel_hei = ts->caps.max_y;
	TOUCH_I("%s - %s\n", __func__, d->trans_xy ? "Enable" : "Disable");
}

int ili7807q_gesture_mode(struct device *dev, bool enable)
{
	int ret = 0;
	u8 data[2] = {0};

	data[0] = CONTROL_LPWG;
	data[1] = enable ? 2 : 0; // 0x0: off, 0x2: info mode (108byte), 0x1: normal mode (8byte)

	TOUCH_TRACE();
	ret = ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &data[0], 1);
	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write gesture cmd error\n");

	TOUCH_I("%s - %s\n", __func__, data[1] ? "Enable" : "Disable");

	return ret;
}

int ili7807q_grip_suppression_ctrl(struct device *dev, u16 x, u16 y)
{
	int ret = 0;
	u8 data[5] = {0};

	data[0] = CONTROL_GRIP_SUPPRESSION;
	data[1] = x >> 8;// high,unit: 0.1mm = 1
	data[2] = x & 0xFF;// low,unit: 0.1mm = 1
	data[3] = y >> 8;// high,unit: 0.1mm = 1
	data[4] = y & 0xFF;// low,unit: 0.1mm = 1

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write grip suppression cmd error\n");

	TOUCH_I("%s - x: %d*0.1mm, y: %d*0.1mm\n", __func__, x, y);
	return ret;
}

int ili7807q_check_int_status(struct device *dev, bool high)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int timer = 3000, ret = -1;

	TOUCH_TRACE();
	/* From FW request, timeout should at least be 5 sec */
	while (timer) {
		if(high) {
			if (gpio_get_value(ts->int_pin)) {
				TOUCH_I("Check busy is free\n");
				ret = 0;
				break;
			}
		} else {
			if (!gpio_get_value(ts->int_pin)) {
				TOUCH_I("Check busy is free\n");
				ret = 0;
				break;
			}
		}

		touch_msleep(1);
		timer--;
	}

	if (ret < -1)
		TOUCH_I("Check busy timeout !!\n");

	return ret;
}

/* CDC = Capacitive to digital conversion */
int ili7807q_check_cdc_busy(struct device *dev, int count, int delay)
{
	int timer = count, ret = -1;
	u8 busy = 0, busy_byte = 0;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	u8 buf = 0;

	TOUCH_TRACE();
	if (d->actual_fw_mode == FIRMWARE_DEMO_MODE) {
		busy_byte = 0x41;
	} else if (d->actual_fw_mode == FIRMWARE_TEST_MODE) {
		busy_byte = 0x51;
	} else {
		TOUCH_E("Unknown fw mode (0x%x)\n", d->actual_fw_mode);
		return -EINVAL;
	}

	TOUCH_I("busy byte = %x\n", busy_byte);

	while (timer > 0) {
		buf = CMD_CDC_BUSY_STATE;
		ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &buf, 1);
		ili7807q_reg_read(dev, CMD_CDC_BUSY_STATE, &busy, 1);

		TOUCH_I("busy status = 0x%x\n", busy);

		if (busy == busy_byte) {
			TOUCH_I("Check busy is free\n");
			ret = 0;
			break;
		}
		timer--;
		touch_msleep(delay);
	}

	if (ret < -1) {
		TOUCH_E("Check busy (0x%x) timeout\n", busy);
		ili7807q_get_pc_latch(dev);
	}

	return ret;
}

void ili7807q_get_pc_latch(struct device *dev)
{
	u32 pc = 0, latch = 0;

	/*
	 * When there's unknown errors occurred at firmware, we can read
	 * program counter and latch from the registers to see what's going on firmware.
	 */

	TOUCH_TRACE();
	if (ili7807q_ice_mode_enable(dev, MCU_STOP) < 0)
		TOUCH_E("Enable ice mode failed while reading pc counter\n");

	if (ili7807q_ice_reg_read(dev, ILI7807q_PC_ADDR, &pc, sizeof(u32)) < 0)
		TOUCH_E("Read pc conter error\n");

	if (ili7807q_ice_reg_read(dev, ILI7807q_LATCH_ADDR, &latch, sizeof(u32)) < 0)
		TOUCH_E("Read latch error\n");

	TOUCH_E("read pc (addr: 0x%x) = 0x%x, latch (addr: 0x%x) = 0x%x\n",
		ILI7807q_PC_ADDR, pc, ILI7807q_LATCH_ADDR, latch);

	if (ili7807q_ice_mode_disable(dev) < 0)
		TOUCH_E("Disable ice mode failed while reading pc counter\n");
}

static int ili7807q_chip_init(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_chip_info *chip_info = &d->ic_info.chip_info;
	int ret = 0;
	u32 PIDData = 0;
	u32 OTPData = 0;
	u32 ANAData = 0;

	TOUCH_TRACE();
	/* Get Chip ID */
	ret = ili7807q_ice_reg_read(dev, ILI7807q_PID_ADDR, &PIDData, sizeof(PIDData));
	if (ret) {
		TOUCH_E("Failed to read Chip ID, %d\n", ret);
		return ret;
	}

	ret = ili7807q_ice_reg_read(dev, ILI7807q_OTP_ID_ADDR, &OTPData, sizeof(OTPData));
	if (ret) {
		TOUCH_E("Failed to read OTP, %d\n", ret);
		return ret;
	}
	ret = ili7807q_ice_reg_read(dev, ILI7807q_ANA_ID_ADDR, &ANAData, sizeof(ANAData));
	if (ret) {
		TOUCH_E("Failed to read ANA, %d\n", ret);
		return ret;
	}

	chip_info->pid = PIDData;
	chip_info->id = PIDData >> 16;
	chip_info->type = (PIDData & 0x0000FF00) >> 8;
	chip_info->ver = PIDData & 0xFF;

	chip_info->otp_id = OTPData&0xFF;
	chip_info->ana_id = ANAData&0xFF;
	TOUCH_I("chipinfo.pid = 0x%x, id = 0x%x, type = 0x%x, ver = 0x%x,otp_id = 0x%x,ana_id = 0x%x\n",
		chip_info->pid, chip_info->id, chip_info->type, chip_info->ver,chip_info->otp_id,chip_info->ana_id);
	
	if (chip_info->id != CHIP_ID) {
		TOUCH_E("CHIP ID ERROR: ID = 0x%x, Type = 0x%x\n", chip_info->id, chip_info->type);
		ret = -ENODEV;
	    return ret;
	}
	return ret;
}
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
static int ili7807q_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *ev = (struct fb_event *)data;

	TOUCH_TRACE();
	if (ev && ev->data && event == FB_EVENT_BLANK) {
		int *blank = (int *)ev->data;

		if (*blank == FB_BLANK_UNBLANK)
			TOUCH_I("FB_UNBLANK\n");
		else if (*blank == FB_BLANK_POWERDOWN)
			TOUCH_I("FB_BLANK\n");
	}

	return 0;
}
#endif
static int ili7807q_sw_reset(struct device *dev, int mode)
{
//	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	bool ice = atomic_read(&d->ice_stat);
	int ret;

	TOUCH_I("%s : SW Reset(mode%d)\n", __func__, mode);

	if(mode == SW_RESET) {
		// TODO do_ic_reset variable setting and i2c skip

		if (!ice)
			if (ili7807q_ice_mode_enable(dev, MCU_STOP) < 0)
				TOUCH_E("Enable ice mode failed before code reset\n");

		ret = ili7807q_ice_reg_write(dev,0x40040, 0xAE, 1);
		if (ret < 0)
			TOUCH_E("ic code reset failed\n");

		if (!ice)
			if (ili7807q_ice_mode_disable(dev) < 0)
				TOUCH_E("Enable ice mode failed after code reset\n");
	} else {
		TOUCH_E("%s Invalid SW reset mode!!\n", __func__);
	}

	atomic_set(&d->init, IC_INIT_NEED);
//Do not need do init work 1005
//	queue_delayed_work(ts->wq, &ts->init_work, msecs_to_jiffies(ts->caps.sw_reset_delay));

	return ret;
}

static int ili7807q_hw_reset(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_I("%s : HW Reset(mode:%d)\n", __func__, mode);
	//touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	touch_gpio_direction_output(ts->reset_pin, 0);
	touch_msleep(5);
	touch_gpio_direction_output(ts->reset_pin, 1);
   // touch_msleep(10);//check

	atomic_set(&d->init, IC_INIT_NEED);
	atomic_set(&d->ice_stat, DISABLE);

	if (mode == HW_RESET_ASYNC){
		queue_delayed_work(ts->wq, &ts->init_work, msecs_to_jiffies(ts->caps.hw_reset_delay));
	} else if (mode == HW_RESET_SYNC) {
		touch_msleep(ts->caps.hw_reset_delay);
		ts->driver->init(dev);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	} else if (mode == HW_RESET_ONLY) {
		touch_msleep(ts->caps.hw_reset_delay);
	//	d->actual_fw_mode = FIRMWARE_DEMO_MODE;
		d->pll_clk_wakeup = true;
		TOUCH_I("%s HW reset pin toggle only, need to call init func\n", __func__);
	} else {
		TOUCH_E("%s Invalid HW reset mode!!\n", __func__);
	}

	return 0;
}

int ili7807q_reset_ctrl(struct device *dev, int ctrl)
{
	TOUCH_TRACE();

	switch (ctrl) {
		default :
		case SW_RESET:
			ili7807q_sw_reset(dev, ctrl);
			break;

		case HW_RESET_ASYNC:
		case HW_RESET_SYNC:
		case HW_RESET_ONLY:
			ili7807q_hw_reset(dev, ctrl);
			break;
	}

	return 0;
}

int ili7807q_display_power_ctrl(struct device *dev, int ctrl)
{
	TOUCH_TRACE();
#if 1//def NOT_USED//This mode doesn't control LCD pannel
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	lge_panel_set_power_mode(ctrl);
#elif defined(CONFIG_LGE_TOUCH_CORE_MTK)
	primary_display_set_deep_sleep(ctrl);
#endif
#else
   TOUCH_I("%s, mode : %s, but do noting\n", __func__, lcd_power_mode_str[ctrl]);
#endif
	if (ctrl >= DEEP_SLEEP_ENTER && ctrl <= DSV_ALWAYS_ON) {
		TOUCH_I("%s, mode : %s\n", __func__, lcd_power_mode_str[ctrl]);
	}
	return 0;
}

static int ili7807q_power(struct device *dev, int ctrl)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_I("%s,ctrl:%d \n", __func__, ctrl);

	switch (ctrl) {
		case POWER_OFF:
			if(d->p_param.touch_power_control_en == FUNC_ON) {
				atomic_set(&d->init, IC_INIT_NEED);
				touch_gpio_direction_output(ts->reset_pin, 0);
				touch_power_1_8_vdd(dev, 0);
				touch_power_3_3_vcl(dev, 0);
				touch_msleep(1);
			} else {
				TOUCH_I("%s, off Not Supported\n", __func__);
			}
			break;

		case POWER_ON:
			if(d->p_param.touch_power_control_en == FUNC_ON) {
				touch_power_1_8_vdd(dev, 1);
				touch_power_3_3_vcl(dev, 1);
				touch_gpio_direction_output(ts->reset_pin, 1);
			} else {
				TOUCH_I("%s, on Not Supported\n", __func__);
			}
			break;
		case POWER_HW_RESET_ASYNC:
			TOUCH_I("%s, reset async\n", __func__);
			ili7807q_reset_ctrl(dev, HW_RESET_ASYNC);
			break;
		case POWER_HW_RESET_SYNC:
			TOUCH_I("%s, reset sync\n", __func__);
			ili7807q_reset_ctrl(dev, HW_RESET_SYNC);
			break;
		case POWER_SW_RESET:
			ili7807q_reset_ctrl(dev, SW_RESET);
			break;
		case POWER_SLEEP:
			if (d->p_param.deep_sleep_power_control == FUNC_ON) {
				ili7807q_display_power_ctrl(dev, DEEP_SLEEP_ENTER);
			}
			break;
		case POWER_WAKE:
			if (d->p_param.deep_sleep_power_control == FUNC_ON) {
				ili7807q_display_power_ctrl(dev, DEEP_SLEEP_EXIT);
			}
			//illek need this api to exit deep_sleep
			ili7807q_display_power_ctrl(dev, DEEP_SLEEP_EXIT);
			break;
		case POWER_DSV_TOGGLE:
			if (d->p_param.dsv_toggle == FUNC_ON) {
				ili7807q_display_power_ctrl(dev, DSV_TOGGLE);
			}
			break;
		case POWER_DSV_ALWAYS_ON:
			if (d->p_param.dsv_toggle == FUNC_ON) {
				ili7807q_display_power_ctrl(dev, DSV_ALWAYS_ON);
			}
			break;
		default:
			TOUCH_I("%s, Not Supported. case: %d\n", __func__, ctrl);
			break;
	}

	return 0;
}
static void ili7807q_get_tci_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->tci.info[TCI_1].tap_count = 2;
	ts->tci.info[TCI_1].min_intertap = 6;
	ts->tci.info[TCI_1].max_intertap = 70;
	ts->tci.info[TCI_1].touch_slop = 100;
	ts->tci.info[TCI_1].tap_distance = 10;
	ts->tci.info[TCI_1].intr_delay = 0;

	ts->tci.info[TCI_2].min_intertap = 6;
	ts->tci.info[TCI_2].max_intertap = 70;
	ts->tci.info[TCI_2].touch_slop = 100;
	ts->tci.info[TCI_2].tap_distance = 255;
	ts->tci.info[TCI_2].intr_delay = 20;
}
int ili7807_get_fw_data_wrapper(struct device *dev, u8 *wdata, int wsize, void *rdata, int rsize) {

	u8 ret, temp[2] = {0}, *ptr;
	char str[256];

	temp[0] = wdata[0];  //201005

	if (wsize == 0)
		ptr = temp;
	else
		ptr = &wdata[1];

	TOUCH_TRACE();

	/* pre cmd */
	ret = ili7807q_reg_write(dev, CMD_READ_DATA_CTRL, &temp, 1);


	/* cmd */
/*	ret = ili7807q_reg_write(dev, wdata[0], ptr, wsize);
	if (ret < 0) {
		TOUCH_E("Write (0x%x) error\n", wdata[0]);
		return ret;
	}
*/
	/* Waiting for FW to prepare data */
	//touch_msleep(5);

	ret = ili7807q_reg_read(dev, wdata[0], rdata, rsize);
	if (ret < 0) {
		TOUCH_E("Read data error, len = %d\n", rsize);
		return ret;
	}

	ptr = (u8 *)rdata;

	TOUCH_E("DEBUG: ptr[0] =%x, wdata[0] = 0x%x\n", ptr[0], wdata[0]);
	/* check data */
	if(ptr[0] != wdata[0]) {
		ret = -1;
		TOUCH_E("Read data not correct, rdata[0] = 0x%x\n", ptr[0]);
	}

	/* dump all data for debug */
	snprintf(str, sizeof(str), "CMD = 0x%02X", wdata[0]);

	ili7807q_dump_packet(rdata, 8, rsize, 16, str);

	return ret;
}


int ili7807q_tp_info(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_panel_info *panel_info = &d->tp_info.panel_info;
	//struct ili7807q_panel_extra_info *panel_extra_info = &d->tp_info.panel_extra_info;
	int ret = 0;
	u8 cmd;

	TOUCH_TRACE();

	memset(panel_info, 0x0, sizeof(*panel_info));
	//memset(panel_extra_info, 0x0, sizeof(*panel_extra_info));


	/* GEt TP info */
	// ret = ili7807q_reg_read(dev, CMD_GET_TP_INFORMATION, panel_info, sizeof(*panel_info));
	cmd = CMD_GET_TP_INFORMATION;
	ret = ili7807_get_fw_data_wrapper(dev, &cmd, 0, panel_info, sizeof(*panel_info));
	if (ret) {
		TOUCH_E("Failed to read touch panel information, %d\n", ret);
		goto out;
	}

	/*No need in this project*/
	/* Get TP extra info */
	// ret = ili7807_get_fw_data_wrapper(dev, CMD_GET_TP_SIGNAL_INFORMATION, panel_info, sizeof(*panel_info));
	// if (ret) {
	// 	TOUCH_E("Failed to read touch panel signal information, %d\n", ret);
	// 	goto out;
	// }

	/*No need in this project*/
	// ret = ili7807q_get_glass_id(dev);
	// if (ret) {
	// 	TOUCH_E("Failed to read touch panel glass id information, %d\n", ret);
	// 	goto out;
	// }
	TOUCH_I("==================== TouchPanel Info ====================\n");

	TOUCH_I("minX = %d, minY = %d, maxX = %d, maxY = %d\n",
		 panel_info->nMinX, panel_info->nMinY,
		 ((u16)panel_info->nMaxX_High << 8) | (panel_info->nMaxX_Low),
		 ((u16)panel_info->nMaxY_High << 8) | (panel_info->nMaxY_Low));
	TOUCH_I("xChannel = %d, yChannel = %d, self_tx = %d, self_rx = %d\n",
		 panel_info->nXChannelNum, panel_info->nYChannelNum,
		 panel_info->self_tx_channel_num, panel_info->self_rx_channel_num);
	TOUCH_I("side_touch_type = %d, max_touch_point= %d, max_key_num = %d\n",
		 panel_info->side_touch_type, panel_info->nMaxTouchPoint,
		 panel_info->nTouchKeyNum);
	// TOUCH_I("tp_signal_info = %s, glass_id = %x%x%x%x%x\n",
	// 		(panel_extra_info->signal == SIGNAL_HIGH_SAMPLE) ? "HIGH" : "LOW",
	// 		panel_extra_info->glass_id[4], panel_extra_info->glass_id[3],
	// 		panel_extra_info->glass_id[2], panel_extra_info->glass_id[1],
	// 		panel_extra_info->glass_id[0]);
	TOUCH_I("======================================================\n");
    return ret;
out:
	
	TOUCH_E("get tp info failed, try get from hex\n");
	panel_info->nMinX = d->hex_info->fw_info[5];
	panel_info->nMinY = d->hex_info->fw_info[7];
	panel_info->nMaxX_High  = d->hex_info->fw_info[9];
	panel_info->nMaxX_Low = d->hex_info->fw_info[8];
	panel_info->nMaxY_High = d->hex_info->fw_info[11];
	panel_info->nMaxY_Low = d->hex_info->fw_info[10];
	panel_info->nXChannelNum = d->hex_info->fw_info[12];
	panel_info->nYChannelNum = d->hex_info->fw_info[14];
	panel_info->self_tx_channel_num = d->hex_info->fw_info[12];
	panel_info->self_rx_channel_num = d->hex_info->fw_info[14];
	panel_info->side_touch_type = 0;
	panel_info->nMaxTouchPoint = 10;
	panel_info->nTouchKeyNum = 0;
	TOUCH_I("==================== TouchPanel Info ====================\n");
	TOUCH_I("get tp info from hex\n");
	TOUCH_I("minX = %d, minY = %d, maxX = %d, maxY = %d\n",
		 panel_info->nMinX, panel_info->nMinY,
		 ((u16)panel_info->nMaxX_High << 8) | (panel_info->nMaxX_Low),
		 ((u16)panel_info->nMaxY_High << 8) | (panel_info->nMaxY_Low));
	TOUCH_I("xChannel = %d, yChannel = %d, self_tx = %d, self_rx = %d\n",
		 panel_info->nXChannelNum, panel_info->nYChannelNum,
		 panel_info->self_tx_channel_num, panel_info->self_rx_channel_num);
	TOUCH_I("side_touch_type = %d, max_touch_point= %d, max_key_num = %d\n",
		 panel_info->side_touch_type, panel_info->nMaxTouchPoint,
		 panel_info->nTouchKeyNum);
	// TOUCH_I("tp_signal_info = %s, glass_id = %x%x%x%x%x\n",
	// 		(panel_extra_info->signal == SIGNAL_HIGH_SAMPLE) ? "HIGH" : "LOW",
	// 		panel_extra_info->glass_id[4], panel_extra_info->glass_id[3],
	// 		panel_extra_info->glass_id[2], panel_extra_info->glass_id[1],
	// 		panel_extra_info->glass_id[0]);
	TOUCH_I("======================================================\n");
		
	return ret;
}
int ili7807q_ic_info(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_fw_info *fw_info = &d->ic_info.fw_info;
	struct ili7807q_protocol_info *protocol_info = &d->ic_info.protocol_info;
	struct ili7807q_core_info *core_info = &d->ic_info.core_info;
	struct touch_core_data *ts = to_touch_core(dev);
	/*No need build version */
	// struct ili7807q_fw_extra_info *fw_extra_info = &d->ic_info.fw_extra_info;
	//struct ili7807q_panel_extra_info *panel_extra_info = &d->tp_info.panel_extra_info;
	int ret = 0;
	u8 cmd;
	u32 core_ver = 0x0;

//	TOUCH_TRACE();

	/* Get Chip info */
/*	ret = ili7807q_chip_init(dev);
	if (ret) {
		TOUCH_E("Failed to get chip info, %d\n", ret);
		goto error;
	}
*/
	/* Get Fw info */
	// ret = ili7807q_reg_read(dev, CMD_GET_FIRMWARE_VERSION, fw_info, sizeof(*fw_info));
	cmd = CMD_GET_FIRMWARE_VERSION;
	ret = ili7807_get_fw_data_wrapper(dev, &cmd, 0, fw_info, sizeof(*fw_info));
	if (ret) {
		TOUCH_E("Failed to read firmware version, %d\n", ret);
		goto error;
	}

	/*No need in this project*/
	// /* Get FW build info */
	// ret = ili7807q_reg_read(dev, CMD_GET_FRIMWARE_BUILD_VERSION, &(fw_extra_info->build), sizeof(u8));
	// if (ret) {
	// 	TOUCH_E("Failed to read firmware build version, %d\n", ret);
	// 	goto error;
	// }

	/* Get Protocol ver */
	// ret = ili7807q_reg_read(dev, CMD_GET_PROTOCOL_VERSION, protocol_info, sizeof(*protocol_info));
	cmd = CMD_GET_PROTOCOL_VERSION;
	ret = ili7807_get_fw_data_wrapper(dev, &cmd, 0, protocol_info, sizeof(*protocol_info));
	if (ret) {
		TOUCH_E("Failed to read protocol version, %d\n", ret);
		goto error;
	}

	/* Get FW core ver */
	// ret = ili7807q_reg_read(dev, CMD_GET_CORE_VERSION, core_info, sizeof(core_info));
	cmd = CMD_GET_CORE_VERSION;
	ret = ili7807_get_fw_data_wrapper(dev, &cmd, 0, core_info, sizeof(core_info));
	if (ret) {
		TOUCH_E("Failed to read core version, %d\n", ret);
		goto error;
	}

	/* Get transferring touch status */
	ili7807q_get_trans_touch_status(dev);

	/*No need in this project*/
	/* Get Tp extra info(signal_info, glass_id_info) */
	// ret = ili7807q_reg_read(dev, CMD_GET_TP_SIGNAL_INFORMATION, &(panel_extra_info->signal), sizeof(u8));
	// if (ret) {
	// 	TOUCH_E("Failed to read touch panel signal information, %d\n", ret);
	// 	goto error;
	// }

	/* Need to be block, Knock-on is not working
	ret = ili7807q_get_glass_id(dev);
	if (ret) {
		TOUCH_E("Failed to read touch panel glass id information, %d\n", ret);
		goto error;
	}*/

	TOUCH_I("==================== Version Info ====================\n");
	/*No need build version */
	// TOUCH_I("Version = v%d.%02d, Build = %d, fw_core.customer_id = %d.%d\n",
	// 		fw_info->major, fw_info->minor, fw_extra_info->build,
	// 		fw_info->core, fw_info->customer_code);
	TOUCH_I("Version = v%d.%02d, fw_core.customer_id = %d.%d, product = %d",
			fw_info->major, fw_info->minor,
			fw_info->core, fw_info->customer_code, ((fw_info->core >> 6) && 1));
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
    TOUCH_I("Panel ID =%d(0:HLT,1:DPT,other:NONE)\n",d->panel_type);
#endif	
	TOUCH_I("Procotol Version = %d.%d.%d\n",
			protocol_info->major, protocol_info->mid, protocol_info->minor);
	TOUCH_I("Core Version = %d.%d.%d.%d\n",
			core_info->code_base, core_info->minor, core_info->revision_major, core_info->revision_minor);
	// TOUCH_I("tp_signal_info = %s, glass_id = %x%x%x%x%x\n",
	// 		panel_extra_info->signal ? "HIGH" : "LOW",
	// 		panel_extra_info->glass_id[4], panel_extra_info->glass_id[3],
	// 		panel_extra_info->glass_id[2], panel_extra_info->glass_id[1],
	// 		panel_extra_info->glass_id[0]);
	TOUCH_I("======================================================\n");

	// TODO fw version check (0.00 or F.FF) and goto error

	return ret;

error:
/******************************************************
 * no flash mode, if read info failed, do not read retry init
 *
 * todo (get info from hex)
 * ****************************************************/
    touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
    TOUCH_E("get fw info failed, tretyr get from hex\n");
	fw_info->major = d->hex_info->fw_info[50];
	fw_info->minor = d->hex_info->fw_info[51];
	fw_info->core = d->hex_info->fw_info[48];
	fw_info->customer_code = d->hex_info->fw_info[49];
	protocol_info->major = d->hex_info->fw_info[72];
	protocol_info->mid = d->hex_info->fw_info[73];
	protocol_info->minor = d->hex_info->fw_info[74];
	core_info->code_base = d->hex_info->fw_info[68];
	core_info->minor= d->hex_info->fw_info[69];
	core_info->revision_major= d->hex_info->fw_info[70];
	core_info->revision_minor= d->hex_info->fw_info[71];
	core_ver = COM_3BYTE(core_info->code_base, core_info->minor, core_info->revision_major);

	if (core_ver <= CORE_VER_1420) {
		d->trans_xy = false;
		TOUCH_I("Core ver (%x), set transfer touch as disabled\n", core_ver);
	}
	else {
		d->trans_xy  = (((d->hex_info->fw_info[0])&0x01)>0) ? true : false;
	}
		TOUCH_I("==================== Version Info ====================\n");
		TOUCH_I("Version = v%d.%02d, fw_core.customer_id = %d.%d, product = %d",
				fw_info->major, fw_info->minor,
				fw_info->core, fw_info->customer_code, ((fw_info->core >> 6) && 1));
		TOUCH_I("Procotol Version = %d.%d.%d\n",
				protocol_info->major, protocol_info->mid, protocol_info->minor);
		TOUCH_I("Core Version = %d.%d.%d.%d\n",
				core_info->code_base, core_info->minor, core_info->revision_major, core_info->revision_minor);
		TOUCH_I("%s - %s\n", __func__, d->trans_xy ? "Enable" : "Disable");
		TOUCH_I("======================================================\n");	
	return ret;
}

#ifdef LG_KNOCK_CODE
static int ili7807q_tci_password(struct device *dev)
{
	return ili7807q_tci_knock(dev);
}
#endif

static int ili7807q_lpwg_proximity_recovery(struct device *dev, int tci_mode, bool swipe_enable)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
	char* tci_mode_str[4] = {"Lpwg_None", "Knock-On", "Knock-On/Code", "Knock-Code"};


	if (tci_mode >= LPWG_NONE && tci_mode <= LPWG_PASSWORD_ONLY) {
		TOUCH_I("ili7807q_lpwg_control - tci_mode = %s, swipe_enable = %s\n",
				tci_mode_str[tci_mode], swipe_enable? "Swipe_Enable":"Swipe_Disable");
	}
	TOUCH_I("%s - tci_mode = %d, swipe_enable = %d\n", __func__, tci_mode, swipe_enable);

	if (tci_mode || swipe_enable) {
		ret = ili7807q_gesture_ctrl(dev, tci_mode, swipe_enable);
		ret = ili7807q_gesture_failreason_ctrl(dev, d->tci_debug_type, d->swipe_debug_type);
		ret = ili7807q_gesture_mode(dev, true);
	} else {
		ret = ili7807q_gesture_mode(dev, false);
	}

	return ret;
}

static int ili7807q_lpwg_control(struct device *dev, int tci_mode, bool swipe_enable)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
	char* tci_mode_str[4] = {"Lpwg_None", "Knock-On", "Knock-On/Code", "Knock-Code"};

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("Skip tci control in deep sleep\n");
		return 0;
	}

	if (tci_mode >= LPWG_NONE && tci_mode <= LPWG_PASSWORD_ONLY) {
		TOUCH_I("ili7807q_lpwg_control - tci_mode = %s, swipe_enable = %s\n",
				tci_mode_str[tci_mode], swipe_enable? "Swipe_Enable":"Swipe_Disable");
	}
	TOUCH_I("%s - tci_mode = %d, swipe_enable = %d\n", __func__, tci_mode, swipe_enable);

	if (tci_mode || swipe_enable) {
		ret = ili7807q_gesture_ctrl(dev, tci_mode, swipe_enable);
		ret = ili7807q_gesture_failreason_ctrl(dev, d->tci_debug_type, d->swipe_debug_type);
		ret = ili7807q_gesture_mode(dev, true);
	} else {
		ret = ili7807q_gesture_mode(dev, false);
	}

	return ret;
}

static int ili7807q_deep_sleep_ctrl(struct device *dev, int state)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;
#ifndef DEEP_SLEEP_CTRL
	int timeout = 20;
	int value = 0;
#endif
	TOUCH_I("%s - %s / incoming_call = %d\n", __func__, (state == IC_NORMAL) ? "Lpwg Mode" : "Deep Sleep", atomic_read(&ts->state.incoming_call));

	if (!atomic_read(&ts->state.incoming_call)) { /* IDLE status */
		if (state == IC_DEEP_SLEEP) { // IC_DEEP_SLEEP
			if (d->actual_fw_mode != FIRMWARE_GESTURE_MODE) {
				if (d->sense_stop) {
					ili7807q_sense_ctrl(dev, false);
					ili7807q_check_cdc_busy(dev, 10, 50);
				}
			}
			ili7807q_sleep_ctrl(dev, SLEEP_DEEP);
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			touch_msleep(150);

			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);

			ili7807q_power(dev, POWER_SLEEP); // Reset Low
			d->actual_fw_mode = FIRMWARE_UNKNOWN_MODE;
		} else { // IC_NORMAL
#ifndef DEEP_SLEEP_CTRL
			ili7807q_power(dev, POWER_WAKE); // Reset High
			/* Gesture can be reentered successfully when the status of proximity turns FAR from NEAR,
			 * 1. Write a speical key to a particular register.
			 * 2. FW saw the register modified, gesture should be enable by itself after did TP Reset.
			 * 3. Polling another specific register to see if gesutre is enabled properly. */
			//touch_msleep(ts->caps.hw_reset_delay);

			ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
			ili7807q_ice_mode_enable(dev, MCU_ON);
			ili7807q_ice_reg_write(dev, ILI7807q_IC_RESET_GESTURE_ADDR,
					ILI7807q_IC_RESET_GESTURE_KEY, sizeof(u32));
			ili7807q_reset_ctrl(dev, HW_RESET_ONLY);

			touch_msleep(30);

			ili7807q_ice_mode_enable(dev, MCU_ON);
			while (timeout > 0) {
				ili7807q_ice_reg_read(dev, ILI7807q_IC_RESET_GESTURE_ADDR, &value, sizeof(value));

				if (value == ILI7807q_IC_RESET_GESTURE_RUN) {
					TOUCH_I("Gesture mode set successfully\n");
					break;
				}
				TOUCH_I("Waiting gesture mode - value = 0x%x\n", value);
				touch_msleep(50);
				timeout--;
			}
			ili7807q_ice_mode_disable(dev);
			if (timeout <= 0) {
				TOUCH_I("Gesture mode set failed\n");
			}

			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			atomic_set(&d->init, IC_INIT_DONE);
			atomic_set(&ts->state.sleep, IC_NORMAL);
			// d->actual_fw_mode = FIRMWARE_GESTURE_MODE;
			ili7807q_lpwg_proximity_recovery(dev, ts->lpwg.mode,
					ts->swipe[SWIPE_D].enable || ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable);
			ili7807q_gesture_failreason_ctrl(dev, d->tci_debug_type, d->swipe_debug_type);
			ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
#else
			if(d->sleep_mode_choice ==0) {
			ili7807q_power(dev, POWER_WAKE); // Reset High
			/* Gesture can be reentered successfully when the status of proximity turns FAR from NEAR,
			 * 1. Write a speical key to a particular register.
			 * 2. FW saw the register modified, gesture should be enable by itself after did TP Reset.
			 * 3. Polling another specific register to see if gesutre is enabled properly. */
			//touch_msleep(ts->caps.hw_reset_delay);				
				ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
				ret = ili7807q_gesture_recovery(dev);
				if(ret<0) {
					TOUCH_I("Gesture mode set failed\n");
				}
			}
			else {
				ili7807q_lpwg_proximity_recovery(dev, ts->lpwg.mode,
					ts->swipe[SWIPE_D].enable || ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable);
				// d->actual_fw_mode = FIRMWARE_GESTURE_MODE;
				ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
			}
			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			atomic_set(&d->init, IC_INIT_DONE);
			atomic_set(&ts->state.sleep, IC_NORMAL);
#endif
		}

		TOUCH_I("%s - %s\n", __func__, (state == IC_NORMAL) ? "Lpwg Mode" : "Deep Sleep");
	} else { /* RINGING or OFFHOOK status */
		if (state == IC_DEEP_SLEEP) { // IC_DEEP_SLEEP
			if (d->actual_fw_mode != FIRMWARE_GESTURE_MODE) {
				if (d->sense_stop) {
					ili7807q_sense_ctrl(dev, false);
					ili7807q_check_cdc_busy(dev, 10, 50);
				}
			}
			ili7807q_sleep_ctrl(dev, SLEEP_IN);

			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
			d->actual_fw_mode = FIRMWARE_UNKNOWN_MODE;
		} else { // IC_NORMAL
			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			atomic_set(&ts->state.sleep, IC_NORMAL);
#ifndef DEEP_SLEEP_CTRL
			ili7807q_lpwg_proximity_recovery(dev, ts->lpwg.mode,
				ts->swipe[SWIPE_D].enable || ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable);
			// d->actual_fw_mode = FIRMWARE_GESTURE_MODE;
			ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
#else
			if(d->sleep_mode_choice ==0) {
				ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
				ret = ili7807q_gesture_recovery(dev);
				if(ret<0) {
					TOUCH_I("Gesture mode set failed\n");
				}
			}
			else {
				ili7807q_lpwg_proximity_recovery(dev, ts->lpwg.mode,
					ts->swipe[SWIPE_D].enable || ts->swipe[SWIPE_U].enable || ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable);
				// d->actual_fw_mode = FIRMWARE_GESTURE_MODE;
				ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
			}
#endif
		}
		TOUCH_I("%s - %s But avoid deep sleep Power Sequence during Call\n",
				__func__,  (state == IC_NORMAL) ? "Lpwg Mode" : "Deep Sleep");
	}

	return ret;
}

int ili7807q_report_tci_fr_buffer(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0, i;
	u8 data[3] = {0, };
	u8 tci_fr_buffer[3 + TCI_FR_BUF_LEN], tci_fr;

	TOUCH_TRACE();

	if (d->tci_debug_type != FAIL_BUFFER_TYPE) {
		TOUCH_I("tci debug in buffer is disabled!!, current type is %s\n", fail_reason_type_str[d->tci_debug_type]);
		return 0;
	}

	if (ts->lpwg.mode == LPWG_NONE) {
		TOUCH_I("knock on is disabled(LPWG_NONE), we skip ili8707g_report_tci_fr_buffer()\n");
		return 0;
	}

	data[0] = CONTROL_LPWG;
	data[1] = CMD_GET_FAIL_REASON; // Fail reason buffer type cmd
	data[2] = 0x01; // Knock On Mode

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write buffer type cmd error\n");

//	ret = ili7807q_reg_read(dev, CMD_NONE, tci_fr_buffer, sizeof(tci_fr_buffer));
	ret = ili7807q_reg_read(dev, CMD_READ_DIRECT, tci_fr_buffer, sizeof(tci_fr_buffer));
	if (ret < 0) {
		TOUCH_E("Failed to read buffer type fail reason\n");
	}

	ili7807q_dump_packet(tci_fr_buffer, 8, sizeof(tci_fr_buffer), 16, "buffer type");

	if (tci_fr_buffer[0] != CMD_GET_FAIL_REASON) {
		TOUCH_E("Wrong packet buf[0] = 0x%x\n", tci_fr_buffer[0]);
		goto out;
	}


	if (tci_fr_buffer[1] != 0x1) {
		TOUCH_E("this is not knock on packet buf[1] = 0x%x\n", tci_fr_buffer[1]);
		goto out;
	}

	if (tci_fr_buffer[2] != 0x0) {
		TOUCH_E("this is not buffer type packet buf[2] = 0x%x\n", tci_fr_buffer[2]);
		goto out;
	}

	for (i = 0; i < TCI_FR_BUF_LEN; i++) {
		tci_fr = tci_fr_buffer[3 + i];

		if (tci_fr < TCI_FR_NUM) {
			TOUCH_I("Knock-on Failure Reason Buffer [%02d] : [%s]\n", i+1, tci_debug_str[tci_fr]);
		}
	}

out:
	return ret;

}

int ili7807q_report_swipe_fr_buffer(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0, i;
	u8 data[3] = {0, };
	u8 swipe_fr_buffer[3 + SWIPE_FAILREASON_BUF_LEN], swipe_fr, swipe_dir;

	TOUCH_TRACE();

	if (d->swipe_debug_type != FAIL_BUFFER_TYPE) {
		TOUCH_I("Swipe debug in buffer is disabled!!, current type is %s\n", fail_reason_type_str[d->swipe_debug_type]);
		return 0;
	}

	if (!ts->swipe[SWIPE_D].enable && !ts->swipe[SWIPE_U].enable && !ts->swipe[SWIPE_L].enable && !ts->swipe[SWIPE_R].enable) {
		TOUCH_I("Swipe Up & Swipe down & Swipe left & Swipe right feature is disable(%d/%d/%d/%d)\n", ts->swipe[SWIPE_U].enable,ts->swipe[SWIPE_D].enable, ts->swipe[SWIPE_L].enable, ts->swipe[SWIPE_R].enable);
		return 0;
	}

	data[0] = CONTROL_LPWG;
	data[1] = CMD_GET_FAIL_REASON; // Fail reason buffer type cmd
	data[2] = 0x03; // Swipe  Mode

	ret = ili7807q_reg_write(dev, CMD_CONTROL_OPTION, &data[0], sizeof(data));
	if (ret < 0)
		TOUCH_E("Write buffer type cmd error\n");

//	ret = ili7807q_reg_read(dev, CMD_NONE, swipe_fr_buffer, sizeof(swipe_fr_buffer));
	ret = ili7807q_reg_read(dev, CMD_READ_DIRECT, swipe_fr_buffer, sizeof(swipe_fr_buffer));
	if (ret < 0) {
		TOUCH_E("Failed to read buffer type fail reason\n");
	}

	ili7807q_dump_packet(swipe_fr_buffer, 8, sizeof(swipe_fr_buffer), 16, "buffer type");

	if (swipe_fr_buffer[0] != CMD_GET_FAIL_REASON) {
		TOUCH_E("Wrong packet buf[0] = 0x%X\n", swipe_fr_buffer[0]);
		goto out;
	}


	if (swipe_fr_buffer[1] != 0x3) {
		TOUCH_E("this is not Swipe packet buf[1] = 0x%X\n", swipe_fr_buffer[1]);
		goto out;
	}


	if (swipe_fr_buffer[2] < 0x1 || swipe_fr_buffer[2] >= 3) {
		TOUCH_E("this is not define Swipe direction packet buf[2] = 0x%X\n", swipe_fr_buffer[2]);
		goto out;
	}

	for (i = 0; i < TCI_FR_BUF_LEN; i++) {
		swipe_dir = swipe_fr_buffer[2]; // Direction
		swipe_fr = swipe_fr_buffer[3 + i]; // Error code

		if (swipe_fr < SWIPE_FAILREASON_NUM) {
			TOUCH_I("Swipe Failure Reason Buffer [%02d] : [%s][%s]\n",
					i + 1,
					swipe_debug_direction_str[swipe_dir],
					swipe_debug_str[swipe_fr]);
		}
	}

out:

	return ret;
}

static int ili7807q_lpwg_mode(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;


	TOUCH_I("lpwg.mode = %d/ ts->swipe[SWIPE_U/D/L/R].enable = %d/%d/%d/%d, d->lcd_mode = %d\n",
			ts->lpwg.mode, ts->swipe[SWIPE_U].enable, ts->swipe[SWIPE_D].enable,ts->swipe[SWIPE_L].enable, ts->swipe[SWIPE_R].enable, d->lcd_mode);

	if (atomic_read(&d->init) == IC_INIT_NEED) {
		TOUCH_I("Skip lpwg mode in IC_INIT_NEED status\n");
		return 0;
	}

	if (atomic_read(&d->reset) == LCD_EVENT_TOUCH_RESET_START) {
		TOUCH_I("Skip lpwg mode during ic reset\n");
		return 0;
	}

	if (atomic_read(&d->changing_display_mode) == CHANGING_DISPLAY_MODE_NOT_READY) {
		TOUCH_I("Skip lpwg mode during changing display mode\n");
		return 0;
	}

	if (atomic_read(&ts->state.fb) == FB_SUSPEND)//check
   	{
		if (ts->mfts_lpwg) {
			TOUCH_I("knock on setting (MINIOS/MFTS)\n");
			if (d->sense_stop) {
				ili7807q_sense_ctrl(dev, false);
				ili7807q_check_cdc_busy(dev, 10, 50);
			}
			ret = ili7807q_lpwg_control(dev, LPWG_DOUBLE_TAP, false);
			if(ret < 0){
				TOUCH_E("failed to set lpwg control (ret: %d)\n", ret);
				goto error;
			}
			ret = ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
			if(ret < 0){
				TOUCH_E("failed to switch_fw_mode (ret: %d)\n", ret);
				goto error;
			}
			return 0;
		}

		if (ts->lpwg.screen) {
			TOUCH_I("Skip lpwg setting\n");
			if(d->tci_debug_type & FAIL_BUFFER_TYPE)
				ili7807q_report_tci_fr_buffer(dev); // Report fr before touch IC reset
			if(d->swipe_debug_type & FAIL_BUFFER_TYPE)
				ili7807q_report_swipe_fr_buffer(dev); // Report fr before touch IC reset
			ili7807q_power(dev, POWER_DSV_ALWAYS_ON);
		} else if (ts->lpwg.sensor == PROX_NEAR) {
			/* deep sleep */
			TOUCH_I("suspend sensor == PROX_NEAR\n");
			if (atomic_read(&ts->state.sleep) != IC_DEEP_SLEEP){
				ret = ili7807q_deep_sleep_ctrl(dev, IC_DEEP_SLEEP);
				if (ret < 0) {
				   TOUCH_E("failed to set IC_DEEP_SLEEP (ret: %d)\n", ret);
     			   goto error;
				}
			}
		} else if (ts->lpwg.qcover == HALL_NEAR) {
			/* Deep Sleep same as Prox near  */
			TOUCH_I("Qcover == HALL_NEAR\n");
			if (atomic_read(&ts->state.sleep) != IC_DEEP_SLEEP){
				ret = ili7807q_deep_sleep_ctrl(dev, IC_DEEP_SLEEP);
				if (ret < 0) {
					TOUCH_E("failed to set IC_DEEP_SLEEP (ret: %d)\n", ret);
     				goto error;
				}
			}
		} else if (ts->lpwg.mode == LPWG_NONE
					&& !ts->swipe[SWIPE_U].enable
					&& !ts->swipe[SWIPE_D].enable
					&& !ts->swipe[SWIPE_L].enable
					&& !ts->swipe[SWIPE_R].enable
					&& d->lcd_mode == LCD_MODE_U0 //check
					) {
			/* knock on/code disable */
			TOUCH_I("Knock-on mode == LPWG_NONE & Swipe_enble == false & lcd mode == %d\n",d->lcd_mode);
			if (atomic_read(&ts->state.sleep) != IC_DEEP_SLEEP)
			{
				ret = ili7807q_deep_sleep_ctrl(dev, IC_DEEP_SLEEP);
				if (ret < 0) {
					TOUCH_E("failed to set IC_DEEP_SLEEP (ret: %d)\n", ret);
     				goto error;
				}
			}
		} else {
			/* knock on/code */
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
				ret = ili7807q_deep_sleep_ctrl(dev, IC_NORMAL);
				if( ret <0){
					TOUCH_E("failed to set IC_NORMAL (ret: %d)\n", ret);
					goto error;
				}
			}
          else { //Check
				TOUCH_I("knock on setting:fb_suspend\n");
				if(d->actual_fw_mode == FIRMWARE_GESTURE_MODE)
				{ 
			       TOUCH_I("alredy lpwg mode: Skip lpwg setting\n");
                   return 0;
				}	
				if (d->sense_stop) {
					ili7807q_sense_ctrl(dev, false);
					ili7807q_check_cdc_busy(dev, 10, 50);
				}
				ret = ili7807q_lpwg_control(dev, ts->lpwg.mode,
					(ts->swipe[SWIPE_U].enable ||ts->swipe[SWIPE_D].enable|| ts->swipe[SWIPE_L].enable || ts->swipe[SWIPE_R].enable));
				if(ret < 0){
				   TOUCH_E("failed to set lpwg control (ret: %d)\n", ret);
				   goto error;
			    }
				ret = ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
				if(ret < 0){
				   TOUCH_E("failed to switch_fw_mode (ret: %d)\n", ret);
				   goto error;
			    }
				if(d->p_param.dsv_toggle == FUNC_ON) {
					ili7807q_power(dev, POWER_DSV_TOGGLE);
				}
			}
		}
		return ret;
	}

	touch_report_all_event(ts);

	/* resume */
	if (ts->lpwg.screen) {
		/* normal */
		TOUCH_I("resume ts->lpwg.screen on\n");
	} else if (ts->lpwg.sensor == PROX_NEAR) {
		TOUCH_I("resume ts->lpwg.sensor == PROX_NEAR\n");
     } else if (ts->lpwg.qcover == HALL_NEAR) {
			/* set qcover action */
		    TOUCH_I("resume ts->lpwg.qcover == HALL_NEAR\n");
	} else {
		/* partial */
		TOUCH_I("resume Partial\n");
	}
error:
	return ret;
}

static int ili7807q_lpwg(struct device *dev, u32 code, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int *value = (int *)param;
	int ret = 0;

	switch (code) {
		case LPWG_TAP_COUNT:
			ts->tci.info[TCI_2].tap_count = value[0];
			break;

		case LPWG_DOUBLE_TAP_CHECK:
			ts->tci.double_tap_check = value[0];
			break;

		case LPWG_UPDATE_ALL:
			TOUCH_I("LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
					value[0],
					value[1] ? "ON" : "OFF",
					value[2] ? "FAR" : "NEAR",
					value[3] ? "CLOSE" : "OPEN");

			ts->lpwg.mode = value[0];
			ts->lpwg.screen = value[1];
			ts->lpwg.sensor = value[2];
			ts->lpwg.qcover = value[3];

			ret = ili7807q_lpwg_mode(dev);
			if (ret)
				TOUCH_E("failed to lpwg_mode, ret:%d", ret);
			break;

		case LPWG_REPLY:
			break;

		default:
			TOUCH_I("%s - Unknown Lpwg Code : %d\n", __func__, code);
			break;
	}

	return ret;
}

static void ili7807q_connect(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int charger_state = atomic_read(&ts->state.connect);
	//int wireless_state = atomic_read(&ts->state.wireless);

	d->charger = false;
	/* wire */
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	if (charger_state >= CONNECT_SDP && charger_state <= CONNECT_FLOATED)
		d->charger = true;
#elif defined(CONFIG_LGE_TOUCH_CORE_MTK)
	//if (charger_state >= STANDARD_HOST && charger_state <= APPLE_0_5A_CHARGER)
	if (charger_state)
		d->charger = true;
#endif

	/* wireless */
	/*if (wireless_state)
		d->charger = d->charger | CONNECT_WIRELESS;*/

	TOUCH_I("%s: write charger_state = %s\n", __func__, d->charger?"Connect":"Disconnect");
	if (atomic_read(&ts->state.pm) > DEV_PM_RESUME) {
		TOUCH_I("DEV_PM_SUSPEND - Don't try I2C\n");
		return;
	}

	ili7807q_plug_ctrl(dev, d->charger);
}
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
static void ili7807q_lcd_mode(struct device *dev, u32 mode)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);

	TOUCH_I("lcd_mode: %d (prev: %d)\n", mode, d->lcd_mode);

	d->lcd_mode = mode;
}
#endif
static int ili7807q_usb_status(struct device *dev, u32 mode)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_I("TA Type: %d\n", atomic_read(&ts->state.connect));
	ili7807q_connect(dev);
	return 0;
}

static int ili7807q_wireless_status(struct device *dev, u32 onoff)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_I("Wireless charger: 0x%02X\n", atomic_read(&ts->state.wireless));
	ili7807q_connect(dev);
	return 0;
}

static int ili7807q_earjack_status(struct device *dev, u32 onoff)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_I("Earjack Type: 0x%02X\n", atomic_read(&ts->state.earjack));
	return 0;
}
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
static void ili7807q_fb_notify_work_func(struct work_struct *fb_notify_work)
{
	struct ili7807q_data *d = container_of(to_delayed_work(fb_notify_work),
			struct ili7807q_data, fb_notify_work);
	int ret = 0;

	if (d->lcd_mode == LCD_MODE_U0 || d->lcd_mode == LCD_MODE_U2)
		ret = FB_SUSPEND;
	else
		ret = FB_RESUME;

	touch_notifier_call_chain(NOTIFY_FB, &ret);
}
#endif
static int ili7807q_notify_charger(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;

	switch (event) {
		case LCD_EVENT_TOUCH_RESET_START:
			TOUCH_I("LCD_EVENT_TOUCH_RESET_START!\n");

			atomic_set(&d->reset, event);
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			touch_gpio_direction_output(ts->reset_pin, 0);
			break;

		case LCD_EVENT_TOUCH_RESET_END:
			TOUCH_I("LCD_EVENT_TOUCH_RESET_END!\n");

			atomic_set(&d->reset, event);
			touch_gpio_direction_output(ts->reset_pin, 1);
			break;

		default:
			TOUCH_E("%lu is not supported in charger mode\n", event);
			break;
	}

	return ret;
}
static int ili7807q_notify_normal(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
	struct lge_panel_notifier *panel_data = data;
#endif
	int ret = 0;

	switch (event) {
		case NOTIFY_TOUCH_RESET:
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
			if (panel_data->state == LGE_PANEL_RESET_LOW) {
				TOUCH_I("NOTIFY_TOUCH_RESET_LOW!\n");
				touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
				touch_gpio_direction_output(ts->reset_pin, 0);
			} else if (panel_data->state == LGE_PANEL_RESET_HIGH) {
				TOUCH_I("NOTIFY_TOUCH_RESET_HIGH!\n");
				touch_gpio_direction_output(ts->reset_pin, 1);
				atomic_set(&d->init, IC_INIT_NEED);
			}
#endif
			break;

		case LCD_EVENT_LCD_BLANK:
			TOUCH_I("LCD_EVENT_LCD_BLANK! - Skip lpwg mode - START\n");
			atomic_set(&ts->state.fb, FB_SUSPEND);
			atomic_set(&d->changing_display_mode, CHANGING_DISPLAY_MODE_NOT_READY);

			break;

		case LCD_EVENT_LCD_UNBLANK:
			TOUCH_I("LCD_EVENT_LCD_UNBLANK! - Skip lpwg mode - START\n");
			atomic_set(&ts->state.fb, FB_RESUME);
			atomic_set(&d->changing_display_mode, CHANGING_DISPLAY_MODE_NOT_READY);

			break;
		case LCD_EVENT_LCD_MODE:
			TOUCH_I("LCD_EVENT_LCD_MODE!\n");
			TOUCH_I("lcd_mode : %lu\n", (unsigned long)*(u32 *)data);
			d->lcd_mode = (unsigned long)*(u32 *)data;
			break;

		case NOTIFY_CONNECTION:
			TOUCH_I("NOTIFY_CONNECTION!\n");
			ret = ili7807q_usb_status(dev, *(u32 *)data);
			break;

		case NOTIFY_WIRELESS:
			TOUCH_I("NOTIFY_WIRELESS!\n");
			ret = ili7807q_wireless_status(dev, *(u32 *)data);
			break;

		case NOTIFY_EARJACK:
			TOUCH_I("NOTIFY_EARJACK!\n");
			ret = ili7807q_earjack_status(dev, *(u32 *)data);
			break;

		case NOTIFY_IME_STATE:
			TOUCH_I("NOTIFY_IME_STATE!\n");
			ret = ili7807q_ime_ctrl(dev);
			break;

		case NOTIFY_CALL_STATE:
			/* Notify Touch IC only for GSM call and idle state */
			if (*(u32*)data >= INCOMING_CALL_IDLE && *(u32*)data <= INCOMING_CALL_LTE_OFFHOOK) {
				TOUCH_I("NOTIFY_CALL_STATE!\n");
				//ret = ili7807q_reg_write(dev, SPECIAL_CALL_INFO_CTRL, (u32*)data, sizeof(u32));
			}
			break;

		case NOTIFY_QMEMO_STATE:
			TOUCH_I("NOTIFY_QMEMO_STATE!\n");
			break;

		default:
			TOUCH_E("%lu is not supported\n", event);
			break;
	}

	return ret;
}

static int ili7807q_notify(struct device *dev, ulong event, void *data)
{
//	TOUCH_TRACE();
	if (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE) {
		TOUCH_I("CHARGER MODE notify\n");
		return ili7807q_notify_charger(dev,event,data);
	}
	return ili7807q_notify_normal(dev,event,data);
}

static void ili7807q_init_works(struct ili7807q_data *d)
{
//	INIT_DELAYED_WORK(&d->fb_notify_work, ili7807q_fb_notify_work_func);
	init_waitqueue_head(&(d->inq));
}

static void ili7807q_init_locks(struct ili7807q_data *d)
{
	mutex_init(&d->io_lock);
	mutex_init(&d->apk_lock);
	mutex_init(&d->debug_lock);
}
static void ili7807q_init_swipe_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	//float mm_to_point = 15.8f; // 1mm -> pixel
	int mm_to_point = 16;

	TOUCH_TRACE();

	ts->swipe[SWIPE_U].enable = false;
	ts->swipe[SWIPE_U].debug_enable = false;
	ts->swipe[SWIPE_U].distance = 20;
	ts->swipe[SWIPE_U].ratio_thres = 150;
	ts->swipe[SWIPE_U].min_time = 4;
	ts->swipe[SWIPE_U].max_time = 150;
	ts->swipe[SWIPE_U].wrong_dir_thres = 5;
	ts->swipe[SWIPE_U].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_U].init_ratio_thres = 100;
	ts->swipe[SWIPE_U].area.x1 = 0 + LPWG_SENSELESS_AREA_W;
	ts->swipe[SWIPE_U].area.y1 = 0;
	ts->swipe[SWIPE_U].area.x2 = ts->caps.max_x - LPWG_SENSELESS_AREA_W;
	ts->swipe[SWIPE_U].area.y2 = ts->caps.max_y;
	ts->swipe[SWIPE_U].start_area.x1 = ((ts->caps.max_x+1)>>1) - (mm_to_point * 18);		// start_area_width 36mm
	ts->swipe[SWIPE_U].start_area.y1 = ts->swipe[SWIPE_U].area.y2 - (mm_to_point * 10);	// start_area_height 10mm
	ts->swipe[SWIPE_U].start_area.x2 = ((ts->caps.max_x+1)>>1) + (mm_to_point * 18);		// start_area_width 36mm
	ts->swipe[SWIPE_U].start_area.y2 = ts->swipe[SWIPE_U].area.y2;
	ts->swipe[SWIPE_U].border_area.x1 = 0;
	ts->swipe[SWIPE_U].border_area.y1 = 0;
	ts->swipe[SWIPE_U].border_area.x2 = 0;
	ts->swipe[SWIPE_U].border_area.y2 = 0;
	ts->swipe[SWIPE_U].start_border_area.x1 = 0;
	ts->swipe[SWIPE_U].start_border_area.y1 = 0;
	ts->swipe[SWIPE_U].start_border_area.x2 = 0;
	ts->swipe[SWIPE_U].start_border_area.y2 = 0;

	ts->swipe[SWIPE_L].enable = false;
	ts->swipe[SWIPE_L].debug_enable = false;
	ts->swipe[SWIPE_L].distance = 12;
	ts->swipe[SWIPE_L].ratio_thres = 150;
	ts->swipe[SWIPE_L].min_time = 4;
	ts->swipe[SWIPE_L].max_time = 150;
	ts->swipe[SWIPE_L].wrong_dir_thres = 5;
	ts->swipe[SWIPE_L].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_L].init_ratio_thres = 100;
	ts->swipe[SWIPE_L].area.x1 = 0;
	ts->swipe[SWIPE_L].area.y1 = 0;
	ts->swipe[SWIPE_L].area.x2 = ts->caps.max_x;
	ts->swipe[SWIPE_L].area.y2 = ts->caps.max_y;
	ts->swipe[SWIPE_L].start_area.x1 = ts->caps.max_x - (mm_to_point * 10);
	ts->swipe[SWIPE_L].start_area.y1 = 305;
	ts->swipe[SWIPE_L].start_area.x2 = ts->caps.max_x;
	ts->swipe[SWIPE_L].start_area.y2 = 1655;
	ts->swipe[SWIPE_L].border_area.x1 = 0;
	ts->swipe[SWIPE_L].border_area.y1 = 0;
	ts->swipe[SWIPE_L].border_area.x2 = 0;
	ts->swipe[SWIPE_L].border_area.y2 = 0;
	ts->swipe[SWIPE_L].start_border_area.x1 = 0;
	ts->swipe[SWIPE_L].start_border_area.y1 = 0;
	ts->swipe[SWIPE_L].start_border_area.x2 = 0;
	ts->swipe[SWIPE_L].start_border_area.y2 = 0;

	ts->swipe[SWIPE_R].enable = false;
	ts->swipe[SWIPE_R].debug_enable = false;
	ts->swipe[SWIPE_R].distance = 12;
	ts->swipe[SWIPE_R].ratio_thres = 150;
	ts->swipe[SWIPE_R].min_time = 4;
	ts->swipe[SWIPE_R].max_time = 150;
	ts->swipe[SWIPE_R].wrong_dir_thres = 5;
	ts->swipe[SWIPE_R].init_ratio_chk_dist = 4;
	ts->swipe[SWIPE_R].init_ratio_thres = 100;
	ts->swipe[SWIPE_R].area.x1 = 0;
	ts->swipe[SWIPE_R].area.y1 = 0;
	ts->swipe[SWIPE_R].area.x2 = ts->caps.max_x;
	ts->swipe[SWIPE_R].area.y2 = ts->caps.max_y;
	ts->swipe[SWIPE_R].start_area.x1 = 0;
	ts->swipe[SWIPE_R].start_area.y1 = 305;
	ts->swipe[SWIPE_R].start_area.x2 = (mm_to_point * 10);
	ts->swipe[SWIPE_R].start_area.y2 = 1655;
	ts->swipe[SWIPE_R].border_area.x1 = 0;
	ts->swipe[SWIPE_R].border_area.y1 = 0;
	ts->swipe[SWIPE_R].border_area.x2 = 0;
	ts->swipe[SWIPE_R].border_area.y2 = 0;
	ts->swipe[SWIPE_R].start_border_area.x1 = 0;
	ts->swipe[SWIPE_R].start_border_area.y1 = 0;
	ts->swipe[SWIPE_R].start_border_area.x2 = 0;
	ts->swipe[SWIPE_R].start_border_area.y2 = 0;
}
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
int ili7807q_get_panel_type(void)
{
	int ret = TP_DPT;
	//TODO ADD DISPLAY FUNCTION 
	// ret = ;
	TOUCH_I("%s-TP_TYPE(%d)\n",__func__,ret);
	return ret;
}
#endif

static int ili7807q_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = NULL;
	int ret = 0;

	TOUCH_TRACE();

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);

	if (!d) {
		TOUCH_E("failed to allocate ic data\n");
		return -ENOMEM;
	}

	d->dev = dev;
	d->boot = false;
	d->update_buf= kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(d->update_buf)) {
		TOUCH_E("fw kzalloc error\n");
		return -ENOMEM;
	}
#ifdef USE_XFER
	//Todo
#else
	d->spi_tx= kzalloc(SPI_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(d->spi_tx)) {
		TOUCH_E("spi tx kzalloc error\n");
		//return -ENOMEM;
		ret = -ENOMEM;
		goto error;
	}
	d->spi_rx= kzalloc(SPI_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(d->spi_rx)) {
		TOUCH_E("spi rx kzalloc error\n");
		//return -ENOMEM;
		ret = -ENOMEM;
		goto error;
	}
#endif

	touch_set_device(ts, d);

	project_param_set(dev);

	ret = touch_gpio_init(ts->reset_pin, "touch_reset");
	if (ret) {
		TOUCH_E("failed to touch gpio init\n");
		//return ret;
		goto error;
	}

	ret = touch_gpio_direction_output(ts->reset_pin, 1);
	if (ret) {
		TOUCH_E("failed to touch gpio direction output\n");
		goto error;
	}

	ret = touch_gpio_init(ts->int_pin, "touch_int");
	if (ret) {
		TOUCH_E("failed to touch gpio init\n");
		goto error;
	}
	ret = touch_gpio_direction_input(ts->int_pin);
	if (ret) {
		TOUCH_E("failed to touch gpio direction input\n");
		goto error;
	}

	if(d->p_param.touch_maker_id_control_en == FUNC_ON) {
		ret = touch_gpio_init(ts->maker_id_pin, "touch_make_id");
		if (ret) {
			TOUCH_E("failed to touch gpio init\n");
			goto error;
		}
		ret = touch_gpio_direction_input(ts->maker_id_pin);
		if (ret) {
			TOUCH_E("failed to touch gpio direction input\n");
			goto error;
		}
	}
	/*******************************************************
	 * Display driver does control the power in ili7807q IC *
	 * due to its design from INCELL 1-chip. Here we skip  *
	 * the control power.                                  *
	 *******************************************************/
	if(d->p_param.touch_power_control_en == FUNC_ON) {
		ret = touch_power_init(dev);
		if (ret) {
			TOUCH_E("failed to touch power init\n");
			//return ret;
			goto error;
		}
	}

	ret = touch_bus_init(dev, SPI_BUF_SIZE);
	if (ret) {
		TOUCH_E("failed to touch bus init\n");
		//return ret;
		goto error;
	}

	ili7807q_init_works(d);
	ili7807q_init_locks(d);

	if (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE) {
		ret = touch_gpio_init(ts->reset_pin, "touch_reset");
		if (ret) {
			TOUCH_E("failed to touch gpio init\n");
			//return ret;
			goto error;
		}
		ret = touch_gpio_direction_output(ts->reset_pin, 0);
		if (ret) {
			TOUCH_E("failed to touch gpio direction output\n");
			//return ret;
			goto error;
		}

		return ret;
	}

	ili7807q_reset_ctrl(dev, HW_RESET_ONLY);
#ifdef NOT_USED// this model use iram
	/* Get flash size info */
	ret = ili7807q_read_flash_info(dev);
	if (ret) {
		TOUCH_E("failed to read flash info\n");
		//return ret;
		goto error;
	}
#endif
	/* Read touch panel information */
	if (ili7807q_ice_mode_enable(dev, MCU_STOP) < 0)
		TOUCH_E("Failed to enable ice mode during probe\n");
	ret = ili7807q_chip_init(dev);

	if (ret) {
		TOUCH_E("Failed to get chip info, %d\n", ret);
		goto error;
	}


	/* Initialize knock on tuning parameter info */
	ili7807q_get_tci_info(dev);

	/* Initialize ILITEK's APK for fw debug */
	ili7807q_apk_init(dev);
    /* Initialize swipe info */
	ili7807q_init_swipe_info(dev);

	d->actual_fw_mode = FIRMWARE_DEMO_MODE;
	d->err_cnt = 0;
	d->lcd_mode = LCD_MODE_U3;
	TOUCH_I("lcd_mode: %d\n", d->lcd_mode);
	d->int_action = true;
#ifdef DEEP_SLEEP_CTRL
	d->sleep_mode_choice= 0; /* 0 : deep sleep(HW reset+DL), 1: sleep(SW reset) */
#endif
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
	 d->panel_type = ili7807q_get_panel_type();
#endif

    if(DEBUG_ENABLE)
    {
	    d->tci_debug_type = FAIL_REAL_TYPE;
	    d->swipe_debug_type = FAIL_REAL_TYPE;
    }else{
	    d->tci_debug_type = FAIL_BUFFER_TYPE;
	    d->swipe_debug_type = FAIL_BUFFER_TYPE;
   	}

     // do not need kfree at here?   ask ilitek's engineer
	return ret;
#ifndef USE_XFER
error:
    ipio_kfree((void **)&d->spi_rx);
    ipio_kfree((void **)&d->spi_tx);
	ipio_kfree((void **)&d->update_buf);
#else
error:
	ipio_kfree((void **)&d->update_buf);
#endif
	return ret;

}

static int ili7807q_remove(struct device *dev)
{
	TOUCH_TRACE();

	return 0;
}

static int ili7807q_shutdown(struct device *dev)
{
	TOUCH_TRACE();

	return 0;
}

static int ili7807q_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	//struct ili7807q_panel_extra_info *panel_extra_info = &d->tp_info.panel_extra_info;
//	const struct firmware *fw = NULL;
	//char fwpath[256] = {0};
	int ret = 0;
	d->get_path = false;
//	TOUCH_TRACE();

//	if ((lge_get_laf_mode() == LGE_LAF_MODE_LAF) || (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE)) { Todo compile error
	if (/*(lge_get_laf_mode() == LGE_LAF_MODE_LAF) ||*/ (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE)) {
		TOUCH_I("laf & charger mode booting fw upgrade skip!!\n");
		return -EPERM;
	}

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	if (check_recovery_boot == LGE_RECOVERY_BOOT) {
		TOUCH_I("recovery mode booting fw upgrade skip!!\n");
		return -EPERM;
	}
#endif

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		return -EPERM;
	}

	if (ts->test_fwpath[0]) {
		memcpy(d->fwpath, &ts->test_fwpath[0], sizeof(d->fwpath));
		TOUCH_I("get fwpath from test_fwpath:%s\n", &ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
	switch(d->panel_type){
		case TP_DPT:
            memcpy(d->fwpath, ts->def_fwpath[TP_DPT], sizeof(d->fwpath));
     		break;
		case TP_HLT:
        default:
			memcpy(d->fwpath, ts->def_fwpath[TP_HLT], sizeof(d->fwpath));
            break;
	}
     TOUCH_I("fw num(%d)\n",ts->def_fwcnt);
#else	
		memcpy(d->fwpath, ts->def_fwpath[0], sizeof(d->fwpath));
#endif		
		TOUCH_I("get fwpath from def_fwpath : %s\n", d->fwpath);
	} else {
		TOUCH_E("no firmware file\n");
		return -EPERM;
	}

	d->fwpath[sizeof(d->fwpath) - 1] = '\0';

	if (strlen(d->fwpath) <= 0) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	TOUCH_I("fwpath[%s]\n", d->fwpath);
	d->get_path = true;
//-------------------------------------------------
	// ret = request_firmware(&fw, fwpath, dev);
	// if (ret) {
	// 	TOUCH_E("fail to request_firmware fwpath: %s (ret:%d)\n", fwpath, ret);
	// 	goto error;
	// }

	// TOUCH_I("fw size:%zu, data: %p\n", fw->size, fw->data);

	d->actual_fw_mode = FIRMWARE_DEMO_MODE;

	ret = ili7807q_fw_upgrade(dev, d->fwpath);
	if (ret)
		TOUCH_E("fail to firmware upgrade");
	else
        TOUCH_I("Firmware upgrade success:%d\n",ret);
	//error:
	// release_firmware(fw);
//-------------------------------------------------
	return ret;
}

#if defined(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
static int ili7807q_app_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

//	if ((lge_get_laf_mode() == LGE_LAF_MODE_LAF) || (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE)) {
	if (/*(lge_get_laf_mode() == LGE_LAF_MODE_LAF) || */(touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE)) {
		TOUCH_I("laf & charger mode booting fw upgrade skip!!\n");
		return -EPERM;
	}

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	if (check_recovery_boot == LGE_RECOVERY_BOOT) {
		TOUCH_I("recovery mode booting fw upgrade skip!!\n");
		return -EPERM;
	}
#endif

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		return -EPERM;
	}

	ret = ili7807q_app_fw_upgrade(dev);
	if (ret)
		TOUCH_E("fail to firmware upgrade");

	return ret;
}
#endif

static int ili7807q_suspend(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int mfts_mode = 0;
	int ret = 0;

	if (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE)
		return -EPERM;

	mfts_mode = touch_check_boot_mode(dev);
	if ((mfts_mode >= TOUCH_MINIOS_MFTS_FOLDER && mfts_mode <= TOUCH_MINIOS_MFTS_CURVED) && !ts->mfts_lpwg) {
		TOUCH_I("%s : touch_suspend - MFTS\n", __func__);
		ili7807q_power(dev, POWER_OFF);
		return -EPERM;
	} else {
		TOUCH_I("%s : touch_suspend start\n", __func__);
	}

	atomic_set(&d->changing_display_mode, CHANGING_DISPLAY_MODE_READY);
	TOUCH_I("Skip lpwg mode - END (suspend)\n");

	if (atomic_read(&d->init) == IC_INIT_DONE) {
		ret = ili7807q_lpwg_mode(dev);
		if (ret)
			TOUCH_E("failed to lpwg_mode, ret:%d", ret);
	} else { /* need init */
		if(ts->lpwg.mode != LPWG_NONE
			|| !ts->swipe[SWIPE_U].enable
			|| !ts->swipe[SWIPE_D].enable
			|| !ts->swipe[SWIPE_L].enable
			|| !ts->swipe[SWIPE_R].enable
			|| d->lcd_mode == LCD_MODE_U0) {
			TOUCH_I("%s : ic not init,set the status first\n", __func__);
			ret = ili7807q_switch_fw_mode(dev, FIRMWARE_GESTURE_MODE);
		}
//		d->tp_suspended =true;
//		return 1;
	}
	d->tp_suspended =true;
	return ret;
}

static int ili7807q_resume(struct device *dev)
{
#ifdef NOT_USED
   	/* FW-upgrade not working at MFTS mode */
	int mfts_mode = 0;
#endif

	int ret =0;
    struct ili7807q_data *d = to_ili7807q_data(dev);
#ifdef DEEP_SLEEP_CTRL
    struct touch_core_data *ts = to_touch_core(dev);
#endif
//	TOUCH_TRACE();

    d->tp_suspended =false;
#ifdef NOT_USED
   	/* FW-upgrade not working at MFTS mode */
	mfts_mode = touch_check_boot_mode(dev);
	TOUCH_I("%s:mfts_mode:%d,ts->mfst_lpwg:%d\n",__func__,mfts_mode,ts->mfts_lpwg);
	if ((mfts_mode >= TOUCH_MINIOS_MFTS_FOLDER && mfts_mode <= TOUCH_MINIOS_MFTS_CURVED) && !ts->mfts_lpwg) {
		ili7807q_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
		ili7807q_ic_info(dev);
		if (ili7807q_upgrade(dev) == 0) {
			ili7807q_power(dev, POWER_OFF);
			ili7807q_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
		}
	}
#endif
	if (touch_check_boot_mode(dev) == TOUCH_CHARGER_MODE) {
		ili7807q_deep_sleep_ctrl(dev, IC_DEEP_SLEEP);
		return -EPERM;
	}

	/****************do host download*************************/
#ifndef DEEP_SLEEP_CTRL
	else {
		ret = ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE);
		if(ret <0) {
			TOUCH_E("resume download failed\n");
		}
	}
#else
	else {
		if(d->sleep_mode_choice ==0) {
			if(d->actual_fw_mode == FIRMWARE_TEST_MODE) {
                ret = ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE);
                if(ret <0) {
                TOUCH_E("resume download failed\n");
            }
        }
        else if((ts->lpwg.mode != LPWG_NONE
			|| !ts->swipe[SWIPE_D].enable
			|| !ts->swipe[SWIPE_U].enable
			|| !ts->swipe[SWIPE_L].enable
			|| !ts->swipe[SWIPE_R].enable)
			&&(atomic_read(&ts->state.sleep) != IC_DEEP_SLEEP)){
				touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
				d->tp_suspended =true;
				ret = ili7807q_wakeup_by_cmd(dev);
				d->tp_suspended =false;
				if(ret <0) {
					TOUCH_E("resume by cmd failed,retry by host download\n");
						ret = ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE);
						if(ret <0) {
							TOUCH_E("resume download failed\n");
						}
				}
				d->actual_fw_mode = FIRMWARE_DEMO_MODE;
				touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			}
			else {
				ret = ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE);
				if(ret <0) {
					TOUCH_E("resume download failed\n");
				}
			}
		}
		else {
						if(d->actual_fw_mode !=FIRMWARE_TEST_MODE) {
							touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
							d->tp_suspended =true;
							ret = ili7807q_wakeup_by_cmd(dev);
							d->tp_suspended =false;
							
							if(ret <0) {
								TOUCH_E("resume by cmd failed,retry by host download\n");
									ret = ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE);
									if(ret <0) {
										TOUCH_E("resume download failed\n");
									}
							}
							d->actual_fw_mode = FIRMWARE_DEMO_MODE;
							touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
						}
						else {
							ret = ili7807q_switch_fw_mode(dev, FIRMWARE_DEMO_MODE);
							if(ret <0) {
								TOUCH_E("resume download failed\n");
							}
			 			}
		}
	}
#endif
	return ret;
}

static int ili7807q_init(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;

//	TOUCH_TRACE();
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
	if (atomic_read(&ts->state.core) == CORE_PROBE) {
		TOUCH_I("fb_notif change\n");
		fb_unregister_client(&ts->fb_notif);
		ts->fb_notif.notifier_call = ili7807q_fb_notifier_callback;
		fb_register_client(&ts->fb_notif);
	}
#endif
	if (atomic_read(&ts->state.core) == CORE_PROBE) {
		TOUCH_E("ili7807q need to host download first\n");
		return 0;
	}
	ret = ili7807q_ic_info(dev);
	if (ret) {
		TOUCH_E("ili7807q_ic_info failed, ret:%d\n", ret);
		return ret;
	}
	/* Read touch panel information */
	ret = ili7807q_tp_info(dev);
	if (ret) {
		TOUCH_E("failed to read touch panel info\n");
        return ret;
	}

	/* Control option usb plug */
	TOUCH_I("%s: charger_state = 0x%s\n", __func__, d->charger? "Connect" : "Disconnect");
	ili7807q_plug_ctrl(dev, d->charger);

	/* TODO Control option ime status */
	ili7807q_ime_ctrl(dev);

	/* TODO Control option phone cover */

	/* TODO Control grip suppresion value */
	ili7807q_grip_suppression_ctrl(dev, 15, 400);

	atomic_set(&d->init, IC_INIT_DONE);
	atomic_set(&ts->state.sleep, IC_NORMAL);
	atomic_set(&d->changing_display_mode, CHANGING_DISPLAY_MODE_READY);
	TOUCH_I("Skip lpwg mode - END (init)\n");
	d->actual_fw_mode = FIRMWARE_DEMO_MODE;
	d->err_cnt = 0;

	ret = ili7807q_lpwg_mode(dev);
	if (ret)
		TOUCH_E("failed to lpwg_mode, ret:%d", ret);

	return 0;
}

int ili7807q_switch_fw_mode(struct device *dev, u8 mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0, prev_mode = 0;
	struct ili7807q_data *d = to_ili7807q_data(dev);
	//char fwpath[256] = {0};
	//bool get_path = true;
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	if (mode < FIRMWARE_DEMO_MODE || mode > FIRMWARE_GESTURE_MODE) {
		TOUCH_E("Arguments from user space are invaild\n");
		//goto out;
		return -1;
	}

	prev_mode = d->actual_fw_mode;
	d->actual_fw_mode = mode;

	TOUCH_I("Switch FW mode= %x, Prev FW mode = %x\n", mode, prev_mode);
	switch (mode) {
		case FIRMWARE_DEMO_MODE:
			if(d->get_path) {
				ret = ili7807q_fw_upgrade(dev, d->fwpath);
				if (ret < 0) {
					TOUCH_E("switch DEMO failed:%d\n", ret);
				}
			}
			else {
				TOUCH_E("not get fw path\n");
			}
			break;
		case FIRMWARE_DEBUG_MODE:
			if (ili7807q_reg_write(dev, CMD_MODE_CONTROL, &mode, sizeof(mode)) < 0) {
				TOUCH_E("Failed to switch Debug mode\n");
				ret = -1;
			}
			break;
		case FIRMWARE_GESTURE_MODE:
			if (prev_mode == FIRMWARE_DEBUG_MODE)
				d->gesture_debug = true;
			else
				d->gesture_debug = false;
			break;
		case FIRMWARE_TEST_MODE:
			TOUCH_I("Download MP code to iram\n");
			if(d->get_path) {
				ret = ili7807q_fw_upgrade(dev, d->fwpath);
				if (ret < 0) {
					TOUCH_E("switch MP failed:%d\n", ret);
				}
			}
			else {
				ret = -1;
				TOUCH_E("not get fw path\n");
			}
			break;
		default:
			TOUCH_E("unknown firwmare mode\n");
			break;
		}
	if (ret)
		d->actual_fw_mode = prev_mode;

	TOUCH_I("Actual FW mode = %d\n", d->actual_fw_mode);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	return ret;
}

u8 ili7807q_calc_data_checksum(void *pMsg, u32 nLength)
{
	int i;
	u8 *p8 = NULL;
	u32 nCheckSum = 0;

//	TOUCH_TRACE();

	if (pMsg == NULL) {
		TOUCH_E("The data going to dump is NULL\n");
		return -1;
	}

	p8 = (u8 *) pMsg;

	for (i = 0; i < nLength; i++) {
		nCheckSum += p8[i];
	}

	return (u8) ((-nCheckSum) & 0xFF);
}


static int ili7807q_get_gesture_data(struct device *dev, int count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	u8 *data = d->gesture_info.data;
	u16 nX = 0, nY = 0;
	u8 i = 0;

	if (!count)
		return 0;

	ts->lpwg.code_num = count;

	for (i = 0; i < count; i++) {

	//	nX = (((data[(3 * i) + 4] & 0xF0) << 4) | data[(3 * i) + 5]);
	//	nY = (((data[(3 * i) + 4] & 0x0F) << 8) | data[(3 * i) + 6]);
	nX = (((data[(3 * i) + 10] & 0xF0) << 4) | (data[(3 * i) + 11]));
	nY = (((data[(3 * i) + 10] & 0x0F) << 8) | (data[(3 * i) + 12]));

		if (d->trans_xy) {
			ts->lpwg.code[i].x = nX;
			ts->lpwg.code[i].y = nY;
		} else {
			ts->lpwg.code[i].x = nX * (ts->caps.max_x) / TPD_WIDTH;
			ts->lpwg.code[i].y = nY * (ts->caps.max_y) / TPD_WIDTH;
		}

		if (ts->lpwg.mode >= LPWG_PASSWORD)
			TOUCH_I("LPWG data xxxx, xxxx\n");
		else
			TOUCH_I("LPWG data %d, %d\n",
				ts->lpwg.code[i].x, ts->lpwg.code[i].y);
	}
	ts->lpwg.code[count].x = -1;
	ts->lpwg.code[count].y = -1;

	return 0;
}

static int ili7807q_get_swipe_data(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	u8 *data = d->gesture_info.data;
	u16 X1 = 0, Y1 = 0, X2 = 0, Y2 = 0;

	/* start (X, Y), end (X, Y), time = 3/3/2 bytes  */
	if (d->trans_xy) {
		X1 = (((data[4] & 0xF0) << 4) | data[5]);
		Y1 = (((data[4] & 0x0F) << 8) | data[6]);
		X2 = (((data[7] & 0xF0) << 4) | data[8]);
		Y2 = (((data[7] & 0x0F) << 8) | data[9]);
	} else {
		X1 = (((data[4] & 0xF0) << 4) | data[5]) * (ts->caps.max_x) / TPD_WIDTH;
		Y1 = (((data[4] & 0x0F) << 8) | data[6]) * (ts->caps.max_y) / TPD_WIDTH;
		X2 = (((data[7] & 0xF0) << 4) | data[8]) * (ts->caps.max_x) / TPD_WIDTH;
		Y2 = (((data[7] & 0x0F) << 8) | data[9]) * (ts->caps.max_y) / TPD_WIDTH;
	}

	TOUCH_I("Swipe Gesture: start(%4d,%4d) end(%4d,%4d) swipe_time(%dms)\n",
		X1, Y1, X2, Y2, (((data[160] & 0xFF) << 8) | data[161]));


	//ts->lpwg.code_num = 1;
	//ts->lpwg.code[0].x = X2;
	//ts->lpwg.code[0].y = Y2;

	ts->lpwg.code_num = 2;
	ts->lpwg.code[0].x = X1;
	ts->lpwg.code[0].y = Y1;

	//ts->lpwg.code[1].x = -1;
	//ts->lpwg.code[1].y = -1;
	ts->lpwg.code[1].x = X2;
	ts->lpwg.code[1].y = Y2;

	return 0;

}

static int ili7807q_irq_uart(struct device *dev, u32 len, void *data)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	u8 *buf;
	int type = 0;
	int need_read_len = 0, one_data_bytes = 0;
	int actual_len = len - 5;
	int uart_len;
	int ret = 0;

	buf = data;
	type = buf[3] & 0x0F;

	TOUCH_I("data[3] = %x, type = %x, actual_len = %d\n",
					buf[3], type, actual_len);

	need_read_len = buf[1] * buf[2];

	if (type == 0 || type == 1 || type == 6) {
		one_data_bytes = 1;
	} else if (type == 2 || type == 3) {
		one_data_bytes = 2;
	} else if (type == 4 || type == 5) {
		one_data_bytes = 4;
	}

	need_read_len =  need_read_len * one_data_bytes + 1;
	TOUCH_D(GET_DATA, "need_read_len = %d  one_data_bytes = %d\n", need_read_len, one_data_bytes);

	memcpy(d->debug_info.uart, buf, len);
	if (need_read_len < actual_len)
		goto out;

	uart_len = need_read_len - actual_len;
	TOUCH_D(GET_DATA, "uart len = %d\n", uart_len);

	ret = ili7807q_report_read(dev,  &d->debug_info.uart[len], uart_len);
	if (ret) {
		TOUCH_E("read uart fail\n");
		ret = -1;
		goto error;
	}

out:
	/* Save debug raw data to apk buffer */
	if (d->debug_info.enable) {
		mutex_lock(&d->debug_lock);
		memset(d->debug_info.buf[d->debug_info.frame_cnt], 0x00, (u8) sizeof(u8) * 2048);
		memcpy(d->debug_info.buf[d->debug_info.frame_cnt], d->debug_info.uart, sizeof(d->debug_info.uart));
		d->debug_info.frame_cnt++;
		if (d->debug_info.frame_cnt > 1) {
			TOUCH_I("frame_cnt = %d\n", d->debug_info.frame_cnt);
		}
		if (d->debug_info.frame_cnt > 1023) {
			TOUCH_E("frame_cnt = %d > 1024\n",
				d->debug_info.frame_cnt);
			d->debug_info.frame_cnt = 1023;
		}
		mutex_unlock(&d->debug_lock);
		wake_up(&d->inq);
	}
error:
	return ret;

}

static int ili7807q_irq_abs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_touch_abs_data *abs_data = d->touch_info.abs_data;
	//struct ili7807q_touch_shape_data *shape_data = d->touch_info.shape_data;
	struct touch_data *tdata;
	u8 finger_index = 0;
	int ret = 0;
	int i = 0;
	u16 nX = 0, nY = 0;
	u8 check_sum = 0;

	ts->new_mask = 0;
	memset(&d->touch_info, 0x0, sizeof(struct ili7807q_touch_info));

	ret = ili7807q_report_read(dev, &d->touch_info, sizeof(d->touch_info));
	if (ret) {
		TOUCH_E("touch_info read fail\n");
		if(ret == DO_SPI_RECOVER) {
			ili7807q_normal_recovery(dev);
//		    touch_msleep(60);
		}
		goto error;
	}

	// for touch data debug
	ili7807q_dump_packet(&d->touch_info, 8, sizeof(d->touch_info), 8, "DEMO");

	if (d->touch_info.packet_id != DEMO_PACKET_ID && d->touch_info.packet_id != I2CUART_PACKET_ID) {
		TOUCH_E("Packet ID Error, pid = 0x%x\n", d->touch_info.packet_id);
		//ili7807q_get_pc_latch(dev);
		ret = -1;
		goto error;
	}

	if (d->touch_info.packet_id == I2CUART_PACKET_ID) {
		ili7807q_irq_uart(dev, sizeof(d->touch_info), &d->touch_info);
		return ret;
	}

	//Compare checksum
	check_sum = ili7807q_calc_data_checksum(&d->touch_info, (sizeof(d->touch_info) - 1));
	if (check_sum != d->touch_info.checksum) {
		TOUCH_E("Packet check sum Error, check_sum = %d, abs_data = %d\n",
				check_sum, d->touch_info.checksum);
		ret = -1;
		goto error;
	}

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((abs_data[i].x_high == 0xF) && (abs_data[i].y_high & 0xF) &&
				(abs_data[i].x_low == 0xFF) && (abs_data[i].y_low == 0xFF)) {
			continue;
		}

		nX = abs_data[i].x_high << 8 | abs_data[i].x_low;
		nY = abs_data[i].y_high << 8 | abs_data[i].y_low;

		tdata = ts->tdata + i;
		tdata->id = i;
		if (d->trans_xy) {
			tdata->x = nX;
			tdata->y = nY;
		} else {
			tdata->x = nX * (ts->caps.max_x) / TPD_WIDTH;
			tdata->y = nY * (ts->caps.max_y) / TPD_WIDTH;//check
		}
		tdata->pressure = abs_data[i].pressure;
		tdata->width_major = 0;
		tdata->width_minor = 0;
		tdata->orientation = 0;
		/*
		tdata->width_major = shape_data[i].width_major_high << 8 | shape_data[i].width_major_low;
		tdata->width_minor = shape_data[i].width_minor_high << 8 | shape_data[i].width_minor_low;
		tdata->orientation = shape_data[i].degree;
		*/

		ts->new_mask |= (1 << i);
		finger_index++;

		TOUCH_D(ABS,
			"tdata [id:%d t:%d x:%d y:%d z:%d-%d,%d,%d]\n",
				tdata->id,
				tdata->type,
				tdata->x,
				tdata->y,
				tdata->pressure,
				tdata->width_major,
				tdata->width_minor,
				tdata->orientation);
	}

	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

		/* Save debug raw data to apk buffer */
	if (d->debug_info.enable) {
		mutex_lock(&d->debug_lock);
		memset(d->debug_info.buf[d->debug_info.frame_cnt], 0x00, (u8) sizeof(u8) * 2048);
		memcpy(d->debug_info.buf[d->debug_info.frame_cnt], &d->touch_info, sizeof(d->touch_info));
		d->debug_info.frame_cnt++;
		if (d->debug_info.frame_cnt > 1) {
			TOUCH_I("frame_cnt = %d\n", d->debug_info.frame_cnt);
		}
		if (d->debug_info.frame_cnt > 1023) {
			TOUCH_E("frame_cnt = %d > 1024\n",
				d->debug_info.frame_cnt);
			d->debug_info.frame_cnt = 1023;
		}
		mutex_unlock(&d->debug_lock);
		wake_up(&d->inq);
	}

error:
	return ret;
}

static void ili7807q_irq_lpwg_failreason(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	u8 *data = d->gesture_info.data;
	u8 tci_fail = 0x0, swipe_fail = 0x0, swipe_dir = 0x0;


	if ((d->swipe_debug_type == 0) && (d->tci_debug_type == 0))
		return;

	if (data[0] != FAILREASON_PACKET_ID) {
		TOUCH_E("not supported lpwg_failreason, pid = 0x%x\n", data[0]);
		return;
	}

	tci_fail = data[162];
	swipe_fail = data[163];
	swipe_dir = data[164];

	TOUCH_D(ABS, "tci_fail = %x, swipe_fail = %x\n", tci_fail, swipe_fail);

	if (tci_fail <= (TCI_FR_NUM - 1) && swipe_fail <= (SWIPE_FAILREASON_NUM - 1)) {
		TOUCH_I("TCI_FAILREASON_BUF = [0X%x] %s, SWIPE_FAILREASON_BUF = [0X%x] %s,%s\n",
			tci_fail, tci_debug_str[tci_fail],
			swipe_fail, swipe_debug_direction_str[swipe_dir], swipe_debug_str[swipe_fail]);
	} else if (swipe_fail <= (SWIPE_FAILREASON_NUM - 1)) {
		TOUCH_I("SWIPE_FAILREASON_BUF = [0X%x] %s,%s\n", swipe_fail,
		swipe_debug_direction_str[swipe_dir], swipe_debug_str[swipe_fail]);
	} else if (tci_fail <= (TCI_FR_NUM - 1)) {
		TOUCH_I("TCI_FAILREASON_BUF = [0X%x] %s\n", tci_fail, tci_debug_str[tci_fail]);
	} else {
		TOUCH_E("TCI[0X%x]/SWIPE[0X%x] FAILERASON DATA UNKNOWN\n", tci_fail, swipe_fail);
	}
}

static void ili7807q_irq_lpwg_gesture(struct device *dev, int gcode)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	switch (gcode) {
		case GESTURE_DOUBLECLICK:
			if (ts->lpwg.mode != LPWG_NONE) {
				ili7807q_get_gesture_data(dev, ts->tci.info[TCI_1].tap_count);
				ts->intr_status = TOUCH_IRQ_KNOCK;
			}
			break;
		case GESTURE_UP:
			if (ts->swipe[SWIPE_U].enable) {
				ts->intr_status = TOUCH_IRQ_SWIPE_UP;
				ili7807q_get_swipe_data(dev);
			}
			break;
		case GESTURE_DOWN:
			if (ts->swipe[SWIPE_D].enable) {
				ts->intr_status = TOUCH_IRQ_SWIPE_DOWN;
				ili7807q_get_swipe_data(dev);
			}
			break;
		case GESTURE_LEFT:
			if (ts->swipe[SWIPE_L].enable) {
				ts->intr_status = TOUCH_IRQ_SWIPE_LEFT2;//TOUCH_IRQ_SWIPE_UP;
				ili7807q_get_swipe_data(dev);
			}
			break;
		case GESTURE_RIGHT:
			if (ts->swipe[SWIPE_R].enable) {
				ts->intr_status = TOUCH_IRQ_SWIPE_RIGHT2;//TOUCH_IRQ_SWIPE_UP;
				ili7807q_get_swipe_data(dev);
			}
			break;
		default:
			TOUCH_I("not supported LPWG wakeup_type, gesture_code = 0x%x\n", gcode);
			break;
	}
}

int ili7807q_irq_lpwg(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	u8 *data = d->gesture_info.data;
	int ret = 0, pid = 0, gcode = 0;
	u8 check_sum = 0;

	ts->new_mask = 0;
	memset(&d->gesture_info, 0x0, sizeof(struct ili7807q_gesture_info));

	ret = ili7807q_report_read(dev, &d->gesture_info, sizeof(d->gesture_info));
	if (ret < 0) {
		TOUCH_E("touch_info read fail\n");
		if(ret == DO_SPI_RECOVER) {
			ili7807q_gesture_recovery(dev);
//			touch_msleep(60);
		}

		goto error;
	}

	ili7807q_dump_packet(data, 8, sizeof(d->gesture_info), 8, "Gesture");

	check_sum = ili7807q_calc_data_checksum(&d->gesture_info, (sizeof(d->gesture_info) - 1));
	if (check_sum != d->gesture_info.checksum) {
		TOUCH_E("Packet check sum Error, check_sum = %d, lpwg_data = %d\n",
				check_sum, d->gesture_info.checksum);
		ret = -1;
		goto error;
	}

	pid = data[0];
	gcode = data[1];

	TOUCH_I("irq_lpwg: pid = 0x%x, code = 0x%x\n", pid, gcode);

	switch (pid) {
		case GESTURE_PACKET_ID:
			ili7807q_irq_lpwg_gesture(dev, gcode);
			break;
		case FAILREASON_PACKET_ID:
			ili7807q_irq_lpwg_failreason(dev);
			break;
		case I2CUART_PACKET_ID:
			ili7807q_irq_uart(dev, sizeof(d->gesture_info), &d->gesture_info);
			break;
		default:
			TOUCH_E("Packet Gesture ID Error, pid = 0x%x\n", pid);
			//ili7807q_get_pc_latch(dev);
			break;
	}

error:
	return ret;
}

int ili7807q_irq_debug(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct touch_data *tdata;
	int debug_len = 0;
	u8 *data = d->debug_info.data;
	u8 finger_index = 0;
	int ret = 0;
	int i = 0;
	u16 nX = 0, nY = 0;

	ts->new_mask = 0;
	memset(data, 0x0, sizeof(*data));

	/* read debug packet header */
	ret = ili7807q_report_read(dev,&d->debug_info.data,
			DEBUG_MODE_PACKET_HEADER_LENGTH);
	if (ret) {
		TOUCH_E("d->debug_info.data header read fail\n");
		 if(ret == DO_SPI_RECOVER) {
			if(d->actual_fw_mode == FIRMWARE_GESTURE_MODE)
			ili7807q_gesture_recovery(dev);
//			touch_msleep(60);
		}
		else {
			ili7807q_normal_recovery(dev);
//			touch_msleep(60);
		}
		goto error;
	}
	if (d->debug_info.data[0] != DEBUG_PACKET_ID && d->debug_info.data[0] != I2CUART_PACKET_ID) {
		TOUCH_E("Packet ID Error, pid = 0x%x\n", d->debug_info.data[0]);
		goto error;
	}

	if (d->touch_info.packet_id == I2CUART_PACKET_ID) {
		ili7807q_irq_uart(dev, DEBUG_MODE_PACKET_HEADER_LENGTH, &d->debug_info.data);
		return ret;
	}

	debug_len = d->debug_info.data[1] << 8 | d->debug_info.data[2];
	TOUCH_D(ABS, "debug_length = %d\n", debug_len);
	ret = ili7807q_report_read(dev,  &(d->debug_info.data[5]),
			debug_len - DEBUG_MODE_PACKET_HEADER_LENGTH);
	if (ret) {
		TOUCH_E("d->debug_info.data read fail\n");
		goto error;
	}

	// for touch data debug
    ili7807q_dump_packet(data, 8, sizeof(d->debug_info.data), 8, "DEBUG");

	/* Decode debug report point*/
	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((data[(3 * i) + 5] == 0xFF) &&
				(data[(3 * i) + 6] & 0xFF) &&
				(data[(3 * i) + 7] == 0xFF)) {
			continue;
		}

		nX = (((data[(3 * i) + 5] & 0xF0) << 4) | (data[(3 * i) + 6]));
		nY = (((data[(3 * i) + 5] & 0x0F) << 8) | (data[(3 * i) + 7]));

		tdata = ts->tdata + i;
		tdata->id = i;
		if (d->trans_xy) {
			tdata->x = nX;
			tdata->y = nY;
		} else {
			tdata->x = nX * (ts->caps.max_x) / TPD_WIDTH;
			tdata->y = nY * (ts->caps.max_y) / TPD_WIDTH;
		}
		/* TODO TMP */
		tdata->pressure = 60;
		tdata->width_major = 1;
		tdata->width_minor = 1;
		tdata->orientation = 1;

		ts->new_mask |= (1 << i);
		finger_index++;

		TOUCH_D(ABS,
			"tdata [id:%d t:%d x:%d y:%d z:%d-%d,%d,%d]\n",
				tdata->id,
				tdata->type,
				tdata->x,
				tdata->y,
				tdata->pressure,
				tdata->width_major,
				tdata->width_minor,
				tdata->orientation);
	}

	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

	/* Save debug raw data to apk buffer */
	if (d->debug_info.enable) {
		mutex_lock(&d->debug_lock);
		memset(d->debug_info.buf[d->debug_info.frame_cnt], 0x00, (u8) sizeof(u8) * 2048);
		memcpy(d->debug_info.buf[d->debug_info.frame_cnt], d->debug_info.data, sizeof(d->debug_info.data));
		d->debug_info.frame_cnt++;
		if (d->debug_info.frame_cnt > 1) {
			TOUCH_I("frame_cnt = %d\n", d->debug_info.frame_cnt);
		}
		if (d->debug_info.frame_cnt > 1023) {
			TOUCH_E("frame_cnt = %d > 1024\n",
				d->debug_info.frame_cnt);
			d->debug_info.frame_cnt = 1023;
		}
		mutex_unlock(&d->debug_lock);
		wake_up(&d->inq);
	}

error:
	return ret;

}

int ili7807q_irq_handler(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;

//	TOUCH_TRACE();
	switch (d->actual_fw_mode) {
		case FIRMWARE_DEMO_MODE:
			ret = ili7807q_irq_abs(dev);
			break;
		case FIRMWARE_DEBUG_MODE:
			ret = ili7807q_irq_debug(dev);
			break;
		case FIRMWARE_GESTURE_MODE:
			if (d->gesture_debug)
				ret = ili7807q_irq_debug(dev);
			else
				ret = ili7807q_irq_lpwg(dev);
			break;
		default:
			TOUCH_E("Unknow fw mode (0x%x)\n", d->actual_fw_mode);
			break;
	}

	return ret;
}

static ssize_t store_reg_ctrl(struct device *dev, const char *buf, size_t count)
{
	char command[6] = {0};
	u32 cmd32 = 0;
	u8 cmd = 0;
	int data = 0;

	if (sscanf(buf, "%5s %x %d", command, &cmd32, &data) <= 0)
		return count;

	cmd = cmd32;
	if (!strcmp(command, "write")) {
		if (ili7807q_reg_write(dev, cmd, &data, sizeof(u32)) < 0)
			TOUCH_E("reg addr 0x%x write fail\n", cmd);
		else
			TOUCH_I("reg[%x] = 0x%x\n", cmd, data);
	} else if (!strcmp(command, "read")) {
		if (ili7807q_reg_read(dev, cmd, &data, sizeof(u32)) < 0)
			TOUCH_E("reg addr 0x%x read fail\n", cmd);
		else
			TOUCH_I("reg[%x] = 0x%x\n", cmd, data);
	} else {
		TOUCH_D(BASE_INFO, "Usage\n");
		TOUCH_D(BASE_INFO, "Write reg value\n");
		TOUCH_D(BASE_INFO, "Read reg\n");
	}
	return count;
}

static ssize_t show_tci_debug(struct device *dev, char *buf)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;

	ret = snprintf(buf + ret, PAGE_SIZE,
			"Current TCI Debug Type = %s\n",
			(d->tci_debug_type < 3) ? fail_reason_type_str[d->tci_debug_type] : "Invalid");

	TOUCH_I("Current TCI Debug Type = %s\n",
			(d->tci_debug_type < 3) ? fail_reason_type_str[d->tci_debug_type] : "Invalid");

	return ret;

}

static ssize_t store_tci_debug(struct device *dev, const char *buf, size_t count)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0 || value < 0 || value > 3) {
		TOUCH_I("Invalid TCI Debug Type, please input 0~2\n");
		return count;
	}

	d->tci_debug_type = (u8)value;

	TOUCH_I("Set TCI Debug Type = %s\n", (d->tci_debug_type < 3) ? fail_reason_type_str[d->tci_debug_type] : "Invalid");

	return count;

}

static ssize_t show_swipe_debug(struct device *dev, char *buf)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int ret = 0;

	ret = snprintf(buf + ret, PAGE_SIZE,
			"Current SWIPE Debug Type = %s\n",
			(d->swipe_debug_type < 3) ? fail_reason_type_str[d->swipe_debug_type] : "Invalid");

	TOUCH_I("Current Swipe Debug Type = %s\n",
			(d->swipe_debug_type < 3) ? fail_reason_type_str[d->swipe_debug_type] : "Invalid");

	return ret;

}

static ssize_t store_swipe_debug(struct device *dev, const char *buf, size_t count)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0 || value < 0 || value > 2) {
		TOUCH_I("Invalid SWIPE Debug Type, please input 0~3\n");
		return count;
	}

	d->swipe_debug_type = (u8)value;

	TOUCH_I("Set SWIPE Debug Type = %s\n", (d->swipe_debug_type < 3) ? fail_reason_type_str[d->swipe_debug_type] : "Invalid");

	return count;

}

static ssize_t store_reset_ctrl(struct device *dev, const char *buf, size_t count)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	ili7807q_reset_ctrl(dev, value);

	return count;
}

static ssize_t show_pinstate(struct device *dev, char *buf)
{
	int ret = 0;
	struct touch_core_data *ts = to_touch_core(dev);

	ret = snprintf(buf, PAGE_SIZE, "RST:%d, INT:%d\n",
			gpio_get_value(ts->reset_pin), gpio_get_value(ts->int_pin));
	TOUCH_I("%s() buf:%s",__func__, buf);
	return ret;
}

static ssize_t store_grip_suppression(struct device *dev, const char *buf, size_t count)
{
	int ret = 0;
	int value_x = 0;
	int value_y = 0;

	if (sscanf(buf, "%d %d", &value_x, &value_y) <= 0)
		return count;

	if (!(value_x >= 0 && value_x <= 32768) ||
			!(value_y >= 0 && value_y <= 32768)) {
		TOUCH_I("%s - wrong value, x : %d (0~32768), y : %d (0~32768)\n",
				__func__, value_x, value_y);
		return count;
	}

	ret = ili7807q_grip_suppression_ctrl(dev, value_x, value_y);
	if (ret < 0)
		TOUCH_E("Grip suppression ctrl error\n");

	return count;
}

#ifdef NOT_USED
int ili7807q_get_glass_id(struct device *dev)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_panel_extra_info *panel_extra_info = &d->tp_info.panel_extra_info;
	u8 *glass_id = panel_extra_info->glass_id;
	u8 mh4_glass_id[5] = {0x4D, 0x48, 0x34, 0x30, 0x31}; //MH401
	int i = 0, ret = 0;
	u32 addr = 0x1D000;

	TOUCH_TRACE();

	ret = ili7807q_ice_mode_enable(dev, MCU_ON);
	if (ret < 0) {
		TOUCH_E("Failed to enable ICE mode\n");
		return ret;
	}

	ili7807q_ice_reg_write(dev, 0x041000, 0x0, 1);  /* CS low */
	ili7807q_ice_reg_write(dev, 0x041004, 0x66AA55, 3); /* Key */

	ili7807q_ice_reg_write(dev, 0x041008, 0x03, 1);

	ili7807q_ice_reg_write(dev, 0x041008, (addr & 0xFF0000) >> 16, 1);
	ili7807q_ice_reg_write(dev, 0x041008, (addr & 0x00FF00) >> 8, 1);
	ili7807q_ice_reg_write(dev, 0x041008, (addr & 0x0000FF), 1);

	for (i = 0; i < GLASS_ID_SIZE; i++) {
		ili7807q_ice_reg_write(dev, 0x041008, 0xFF, 1);
		ili7807q_ice_reg_read(dev, 0x41010, &glass_id[i], sizeof(u8));
	}

	panel_extra_info->glass_info = SIGNAL_HIGH_SAMPLE;
	for (i = 0; i < GLASS_ID_SIZE; i++) {
		if (glass_id[i] != mh4_glass_id[i]) {
			panel_extra_info->glass_info = SIGNAL_LOW_SAMPLE;
			break;
		}
	}
	ili7807q_dma_clear(dev);

	ili7807q_ice_reg_write(dev, 0x041000, 0x1, 1); /* CS high */

	ret = ili7807q_ice_mode_disable(dev);
	if(ret < 0){
		TOUCH_E("Failed to disable ICE mode\n");
		return ret;
	}

	return ret;
}
static ssize_t show_glass_id(struct device *dev, char *buf)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_panel_extra_info *panel_extra_info = &d->tp_info.panel_extra_info;
	int ret = 0;

	ret = ili7807q_get_glass_id(dev);
	if (ret < 0)
		TOUCH_E("Get glass id error\n");

	ret = snprintf(buf, PAGE_SIZE, "GLASS ID: 0x%x%x%x%x%x\n",
			panel_extra_info->glass_id[4], panel_extra_info->glass_id[3],
			panel_extra_info->glass_id[2], panel_extra_info->glass_id[1],
			panel_extra_info->glass_id[0]);
	TOUCH_I("%s - GLASS ID : 0x%x%x%x%x%x\n", __func__,
			panel_extra_info->glass_id[4], panel_extra_info->glass_id[3],
			panel_extra_info->glass_id[2], panel_extra_info->glass_id[1],
			panel_extra_info->glass_id[0]);

	return ret;

}
#endif
static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(reset_ctrl, NULL, store_reset_ctrl);
static TOUCH_ATTR(pinstate, show_pinstate, NULL);
static TOUCH_ATTR(grip_suppression, NULL, store_grip_suppression);
static TOUCH_ATTR(tci_debug, show_tci_debug, store_tci_debug);
static TOUCH_ATTR(swipe_debug, show_swipe_debug, store_swipe_debug);


static struct attribute *ili7807q_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_tci_debug.attr,
	&touch_attr_swipe_debug.attr,
	&touch_attr_reset_ctrl.attr,
	&touch_attr_pinstate.attr,
	&touch_attr_grip_suppression.attr,
	NULL,
};

static const struct attribute_group ili7807q_attribute_group = {
	.attrs = ili7807q_attribute_list,
};

static int ili7807q_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
//	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &ili7807q_attribute_group);
	if (ret) {
		TOUCH_E("ili7807q sysfs register failed\n");

		goto error;
	}

	ili7807q_mp_register_sysfs(dev);
	if (ret) {
		TOUCH_E("ili7807q register failed\n");

		goto error;
	}

	return 0;

error:
	kobject_del(&ts->kobj);

	return ret;
}

static int ili7807q_get_cmd_version(struct device *dev, char *buf)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_fw_info *fw_info = &d->ic_info.fw_info;
	struct ili7807q_protocol_info *protocol_info = &d->ic_info.protocol_info;
	struct ili7807q_core_info *core_info = &d->ic_info.core_info;
	/*No need build version */
	// struct ili7807q_fw_extra_info *fw_extra_info = &d->ic_info.fw_extra_info;
	//struct ili7807q_panel_extra_info *panel_extra_info = &d->tp_info.panel_extra_info;
	int offset = 0;

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"==================== Version Info ====================\n");
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Product ID = [ILI7807q]\n");
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
    offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Panel ID =%d(0:HLT,1:DPT,other:NONE)\n",d->panel_type);
#endif	
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Version = v%d.%02d, fw_core.customer_id = %d.%d, product = %d\n",
			fw_info->major, fw_info->minor,
			fw_info->core, fw_info->customer_code, ((fw_info->core >> 6) && 1));

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Procotol Version = %d.%d.%d\n",
			protocol_info->major, protocol_info->mid, protocol_info->minor);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Core Version = %d.%d.%d.%d\n",
			core_info->code_base, core_info->minor, core_info->revision_major, core_info->revision_minor);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"======================================================\n");

	return offset;
}

static int ili7807q_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct ili7807q_data *d = to_ili7807q_data(dev);
	struct ili7807q_fw_info *fw_info = &d->ic_info.fw_info;
	int offset = 0;

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"v%d.%02d\n", fw_info->major, fw_info->minor);

	return offset;
}

static int ili7807q_swipe_enable(struct device *dev, bool enable)
{
//	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

//	ili7807q_lpwg_control(dev, ts->lpwg.mode, enable); => disable  if this api is called when LCM on, touch don't work

	return 0;
}

static int ili7807q_init_pm(struct device *dev)
{
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIF)
	struct touch_core_data *ts = to_touch_core(dev);
#endif

//	TOUCH_TRACE();

#if defined(CONFIG_DRM_MSM)
	TOUCH_I("%s: drm_notif change\n", __func__);
	ts->drm_notif.notifier_call = ili7807q_drm_notifier_callback;
#elif defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
	TOUCH_I("%s: fb_notif change\n", __func__);
	fb_unregister_client(&ts->fb_notif);
	ts->fb_notif.notifier_call = ili7807q_fb_notifier_callback;
#endif
	return 0;
}

static int ili7807q_esd_recovery(struct device *dev)
{
//	TOUCH_TRACE();
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	lge_mdss_report_panel_dead();
#endif

#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
//need to implement this function the display team.
	//	mtkfb_esd_recovery();
#endif
	return 0;
}

static int ili7807q_set(struct device *dev, u32 cmd, void *input, void *output)
{
	return 0;
}

static int ili7807q_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_D(BASE_INFO, "%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
		case CMD_VERSION:
			ret = ili7807q_get_cmd_version(dev, (char *)output);
			break;

		case CMD_ATCMD_VERSION:
			ret = ili7807q_get_cmd_atcmd_version(dev, (char *)output);
			break;

		default:
			break;
	}

	return ret;
}

static struct touch_driver touch_driver = {
	.probe = ili7807q_probe,
	.remove = ili7807q_remove,
	.shutdown = ili7807q_shutdown,
	.suspend = ili7807q_suspend,
	.resume = ili7807q_resume,
	.init = ili7807q_init,
	.irq_handler = ili7807q_irq_handler,
	.power = ili7807q_power,
	.upgrade = ili7807q_upgrade,
#if defined(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
	.app_upgrade = ili7807q_app_upgrade,
#endif
	.esd_recovery = ili7807q_esd_recovery,
	.lpwg = ili7807q_lpwg,
	.swipe_enable = ili7807q_swipe_enable,
	.notify = ili7807q_notify,
	.init_pm = ili7807q_init_pm,
	.register_sysfs = ili7807q_register_sysfs,
	.set = ili7807q_set,
	.get = ili7807q_get,
};

#define MATCH_NAME	"lge,ili7807q"

static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
	{ },
};

#ifdef  CONFIG_SPI

static struct touch_hwif hwif = {
	.bus_type = HWIF_SPI,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
	.bits_per_word = 8,
	.spi_mode = SPI_MODE_0,
	.max_freq = 12000000,/*MAX Guide 12000000, reference 10000000 */
};
#else
static struct touch_hwif hwif = {
	.bus_type = HWIF_I2C,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
};
#endif
static int __init touch_device_init(void)
{

	TOUCH_I("touch_device_init func\n");

#ifdef IS_LCM_NAME
	if (is_lcm_name("AUO-ili7807q")) //Todo compile error
	{
		//TOUCH_I("%s, ili7807q found!!lcm_name = %s\n",__func__,lge_get_lcm_name());//Todo compile error
		return touch_bus_device_init(&hwif, &touch_driver);
	}

	TOUCH_I("%s, ili7807q not found\n", __func__);

	return 0;
#else
	return touch_bus_device_init(&hwif, &touch_driver);
#endif
}

static void __exit touch_device_exit(void)
{
	TOUCH_TRACE();
	touch_bus_device_exit(&hwif);
}

module_init(touch_device_init);
module_exit(touch_device_exit);

MODULE_AUTHOR("BSP-TOUCH@lge.com");
MODULE_DESCRIPTION("LGE touch driver v3");
MODULE_LICENSE("GPL");

