/* touch_ili7807q.h
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: PH1-BSP-Touch@lge.com
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

#ifndef LGE_TOUCH_ILI7807Q_H
#define LGE_TOUCH_ILI7807Q_H

#define DEEP_SLEEP_CTRL /* feature enable: don't go to deep sleep*/
#define DEBUG_ENABLE  1  /* ENABLE:1, DISABLE:0 */

#define CHIP_ID				0x7807
#define CHIP_TYPE			0x17
#define CORE_TYPE			0x03
#define TPD_WIDTH			2048

#define MAX_TOUCH_NUM			10

#define ILI7807q_SLAVE_I2C_ADDR		0x41
#define ILI7807q_ICE_MODE_ADDR		0x181062
#define ILI7807q_PID_ADDR		0x4009C
#define ILI7807q_OTP_ID_ADDR	0x400A0
#define ILI7807q_ANA_ID_ADDR	0x400A4
#define ILI7807q_WDT_ADDR		0x5100C
#define ILI7807q_PC_ADDR		0x44008
#define ILI7807q_LATCH_ADDR		0x51010
#define ILI7807q_IC_RESET_ADDR		0x040050
#define ILI7807q_IC_RESET_KEY		0x00019881
#define ILI7807q_IC_RESET_GESTURE_ADDR	0x04005C//0x040054
#define ILI7807q_IC_RESET_GESTURE_KEY	0xF38A//0xF38A94EF
#define ILI7807q_IC_RESET_GESTURE_RUN	0xA67C//0xA67C9DFE

#define ESD_GESTURE_PWD					0xF38A94EF
#define SPI_ESD_GESTURE_RUN				0xA67C9DFE
#define I2C_ESD_GESTURE_RUN				0xA67C9DFE
#define SPI_ESD_GESTURE_PWD_ADDR			0x25FF8
#define I2C_ESD_GESTURE_PWD_ADDR			0x40054

#define ESD_GESTURE_CORE146_PWD				0xF38A
#define SPI_ESD_GESTURE_CORE146_RUN			0xA67C
#define I2C_ESD_GESTURE_CORE146_RUN			0xA67C
#define SPI_ESD_GESTURE_CORE146_PWD_ADDR		0x4005C
#define I2C_ESD_GESTURE_CORE146_PWD_ADDR		0x4005C

#define W_CMD_HEADER_SIZE		1
#define W_ICE_HEADER_SIZE		4
#define GLASS_ID_SIZE			5

/* Firmware core version */
#define CORE_VER_1410			0x010401
#define CORE_VER_1420			0x010402
#define CORE_VER_1430			0x010403
#define CORE_VER_1460				0x010406
#define CORE_VER_1470				0x010407

#define SPI_WRITE					0x82
#define SPI_READ					0x83
#define SPI_ACK						0xA3

#define ENABLE			1
#define DISABLE			0
#define CONFIG_PLAT_SPRD		DISABLE
#define I2C_DMA_TRANSFER		DISABLE
#define SPI_DMA_TRANSFER_SPLIT		DISABLE
#define SPRD_SYSFS_SUSPEND_RESUME	DISABLE

#define DO_SPI_RECOVER 				-2
#if SPI_DMA_TRANSFER_SPLIT
#define SPI_UPGRADE_LEN				2048
#else
#define SPI_UPGRADE_LEN				0
#endif

#define CMD_READ_DIRECT     0xFF
#define CMD_NONE			0x00
#define CMD_GET_FAIL_REASON		        0x9
#define CMD_GET_FRIMWARE_BUILD_VERSION	0x15
#define CMD_GET_TP_SIGNAL_INFORMATION	0x17
#define CMD_GET_TP_INFORMATION		0x20
#define CMD_GET_FIRMWARE_VERSION	0x21
#define CMD_GET_PROTOCOL_VERSION	0x22
#define CMD_GET_CORE_VERSION		0x23
#define CMD_GET_MCU_STOP_INTERNAL_DATA	0x25
#define CMD_GET_MCU_ON_INTERNAL_DATA	0x1F
#define CMD_GET_KEY_INFORMATION		0x27
#define CMD_GET_PANEL_INFO		0x29
#define CMD_GET_IC_STATUS		0x2A
#define CMD_GET_TC_STATUS		0x2B
#define CMD_GET_FLASH_DATA		0x41
#define CMD_MODE_CONTROL		0xF0
#define CMD_SET_CDC_INIT		0xF1
#define CMD_GET_CDC_DATA		0xF2
#define CMD_CDC_BUSY_STATE		0xF3
#define CMD_READ_DATA_CTRL		0xF6
#define CMD_I2C_UART			0x40
#define CMD_ICE_MODE_EXIT		0x1B
#define CMD_READ_MP_TEST_CODE_INFO	0xFE

#define CMD_CONTROL_OPTION		0x01
#define CONTROL_SENSE			0x01
#define CONTROL_SLEEP			0x02
#define CONTROL_SCAN_RATE		0x03
#define CONTROL_CALIBRATION		0x04
#define CONTROL_INTERRUPT_OUTPUT	0x05
#define CONTROL_GLOVE			0x06
#define CONTROL_STYLUS			0x07
#define CONTROL_TP_SCAN_MODE		0x08
#define CONTROL_SW_RESET		0x09
#define CONTROL_LPWG			0x0A
#define CONTROL_GESTURE			0x0B
#define CONTROL_PHONE_COVER		0x0C
#define CONTROL_FINGER_SENSE		0x0F
#define CONTROL_PROXIMITY		0x10
#define CONTROL_PLUG			0x11
#define CONTROL_GRIP_SUPPRESSION	0x1C
// #define CONTROL_GRIP_SUPPRESSION_X	0x12
// #define CONTROL_GRIP_SUPPRESSION_Y	0x13
#define CONTROL_IME			0x14
#define CONTROL_INT_ACTION		0x1B
#define CONTROL_INT_INFO		0x21

#define FIRMWARE_UNKNOWN_MODE		0xFF
#define FIRMWARE_DEMO_MODE		0x00
#define FIRMWARE_TEST_MODE		0x01
#define FIRMWARE_DEBUG_MODE		0x02
#define FIRMWARE_I2CUART_MODE		0x03
#define FIRMWARE_GESTURE_MODE		0x04

#define DEMO_PACKET_ID			0x5A
#define DEBUG_PACKET_ID			0xA7
#define TEST_PACKET_ID			0xF2
#define GESTURE_PACKET_ID		0xAA
#define FAILREASON_PACKET_ID		0xAF
#define I2CUART_PACKET_ID		0x7A

#define GESTURE_DOUBLECLICK		0x58
#define GESTURE_UP			0x60
#define GESTURE_DOWN			0x61
#define GESTURE_LEFT			0x62
#define GESTURE_RIGHT			0x63
#define GESTURE_M			0x64
#define GESTURE_W			0x65
#define GESTURE_C			0x66
#define GESTURE_E			0x67
#define GESTURE_V			0x68
#define GESTURE_O			0x69
#define GESTURE_S			0x6A
#define GESTURE_Z			0x6B

#define ILI7807Q_SWIPE_INFO_NUM 29
#define ILI7807Q_SWIPE_NUM        4
#define ILI7807Q_SWIPE_U          0
#define ILI7807Q_SWIPE_D          1
#define ILI7807Q_SWIPE_L          2
#define ILI7807Q_SWIPE_R          3

#define DEBUG_MODE_PACKET_LENGTH	2048 //1308
#define DEBUG_MODE_PACKET_HEADER_LENGTH	5
#define DEBUG_MODE_PACKET_FRAME_LIMIT	1024
#define TEST_MODE_PACKET_LENGTH		1180
#define GESTURE_MODE_PACKET_INFO_LENGTH          170
#define GESTURE_MODE_PACKET_NORMAL_LENGTH        8

/* DMA Control Registers */
#define DMA_BASED_ADDR				0x72000
#define DMA48_ADDR				(DMA_BASED_ADDR + 0xC0)
#define DMA48_REG_DMA_CH0_TRIGGER_SEL		(BIT(16)|BIT(17)|BIT(18)|BIT(19))
#define DMA48_REG_DMA_CH0_START_CLEAR		BIT(25)
#define DMA49_ADDR				(DMA_BASED_ADDR + 0xC4)
#define DMA49_REG_DMA_CH0_SRC1_ADDR		DMA49_ADDR
#define DMA50_ADDR				(DMA_BASED_ADDR + 0xC8)
#define DMA50_REG_DMA_CH0_SRC1_STEP_INC		DMA50_ADDR
#define DMA50_REG_DMA_CH0_SRC1_FORMAT		(BIT(24)|BIT(25))
#define DMA50_REG_DMA_CH0_SRC1_EN		BIT(31)
#define DMA52_ADDR				(DMA_BASED_ADDR + 0xD0)
#define DMA52_REG_DMA_CH0_SRC2_EN		BIT(31)
#define DMA53_ADDR				(DMA_BASED_ADDR + 0xD4)
#define DMA53_REG_DMA_CH0_DEST_ADDR		DMA53_ADDR
#define DMA54_ADDR				(DMA_BASED_ADDR + 0xD8)
#define DMA54_REG_DMA_CH0_DEST_STEP_INC		DMA54_ADDR
#define DMA54_REG_DMA_CH0_DEST_FORMAT		(BIT(24)|BIT(25))
#define DMA54_REG_DMA_CH0_DEST_EN		BIT(31)
#define DMA55_ADDR				(DMA_BASED_ADDR + 0xDC)
#define DMA55_REG_DMA_CH0_TRAFER_COUNTS		DMA55_ADDR
#define DMA55_REG_DMA_CH0_TRAFER_MODE		(BIT(24)|BIT(25)|BIT(26)|BIT(27))

/* INT Function Registers */
#define INTR_BASED_ADDR				0x48000
#define INTR1_ADDR				(INTR_BASED_ADDR + 0x4)
#define INTR1_REG_DMA_CH0_INT_FLAG		BIT(17)
#define INTR1_REG_FLASH_INT_FLAG		BIT(25)
#define INTR2_ADDR				(INTR_BASED_ADDR + 0x8)
#define INTR2_TD_INT_FLAG_CLEAR			INTR2_ADDR
#define INTR2_TDI_ERR_INT_FLAG_CLEAR		BIT(18)
#define INTR32_ADDR				(INTR_BASED_ADDR + 0x80)
#define INTR32_REG_T0_INT_EN			BIT(24)
#define INTR32_REG_T1_INT_EN			BIT(25)
#define INTR33_ADDR				(INTR_BASED_ADDR + 0x84)
#define INTR33_REG_DMA_CH0_INT_EN		BIT(17)

/* FLASH Control Function Registers */
#define FLASH_BASED_ADDR			0x41000
#define FLASH0_ADDR				FLASH_BASED_ADDR+0x0
#define FLASH0_RESERVED_0			(BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define FLASH0_REG_PRECLK_SEL			BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)
#define FLASH0_REG_FLASH_CSB			FLASH0_ADDR
#define FLASH0_REG_RX_DUAL          		BIT(24)
#define FLASH0_RESERVED_26			(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))
#define FLASH1_ADDR				(FLASH_BASED_ADDR + 0x4)
#define FLASH1_REG_FLASH_KEY1			FLASH1_ADDR
#define FLASH1_RESERVED_0			(FLASH1_ADDR + 0x03)
#define FLASH2_ADDR				(FLASH_BASED_ADDR + 0x8)
#define FLASH2_REG_TX_DATA			FLASH2_ADDR
#define FLASH3_ADDR				FLASH_BASED_ADDR+0xC
#define FLASH3_REG_RCV_CNT			FLASH3_ADDR
#define FLASH4_ADDR				FLASH_BASED_ADDR+0x10
#define FLASH4_REG_RCV_DATA 			FLASH4_ADDR
#define FLASH4_REG_FLASH_DMA_TRIGGER_EN		BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)

#define CONNECT_NONE			(0x00)
#define CONNECT_USB			(0x01)
#define CONNECT_TA			(0x02)
#define CONNECT_OTG			(0x03)
#define CONNECT_WIRELESS		(0x10)

#define COM_2BYTE(X, Y)		(X << 8 | Y)
#define COM_3BYTE(X, Y, Z)		(X << 16 | Y << 8 | Z)
#define COM_4BYTE(X, Y, Z, A)	(X << 24 | Y << 16 | Z << 8 | A)
#define CHECK_EQUAL(X, Y) 		((X == Y) ? 0 : -1)
#define ERR_ALLOC_MEM(X)		((IS_ERR(X) || X == NULL) ? 1 : 0)
#define MIN(a,b)				(((a)<(b))?(a):(b))
#define MAX(a,b)				(((a)>(b))?(a):(b))

#define SPI_BUF_SIZE 4096
#define LPWG_SENSELESS_AREA_W		70	/* pixel */

struct project_param {
	u8 touch_power_control_en;
	u8 touch_maker_id_control_en;
	u8 dsv_toggle;
	u8 deep_sleep_power_control;
	u8 dump_packet;
};

struct bin_fw_info {
	u8 fw_info[75];
	u8 fw_mp_info[4];
	u32 core_ver;
	u32 mp_ver;
};
enum {
	MCU_ON = 0,
	MCU_STOP,
};

enum {
	SIGNAL_LOW_SAMPLE = 0,
	SIGNAL_HIGH_SAMPLE,
};


enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};


#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
enum {
	BOE_INCELL_ILI7807q = 0,
	CSOT_INCELL_FT8736,
	MH4_BOE_HX83102D,
};
#endif

enum {
	FUNC_OFF = 0,
	FUNC_ON,
};

enum {
	IC_INIT_NEED = 0,
	IC_INIT_DONE,
};

enum {
	CHANGING_DISPLAY_MODE_READY = 0,
	CHANGING_DISPLAY_MODE_NOT_READY,
};

enum {
	DEBUG_IDLE = 0,
	DEBUG_GET_DATA_DONE,
	DEBUG_GET_DATA,
};

enum {
	SW_RESET = 0,
	HW_RESET_ASYNC,
	HW_RESET_SYNC,
	HW_RESET_ONLY,
};

enum {
	IC_STATE_INIT = 0,
	IC_STATE_RESET,
	IC_STATE_WAKE,
	IC_STATE_SLEEP,
	IC_STATE_STANDBY,
};

enum {
	SLEEP_IN = 0,
	SLEEP_OUT = 1,
	SLEEP_DEEP = 3,
};

enum TP_ERR_CODE {
	EFW_CONVERT_FILE = 200,
	EFW_ICE_MODE,
	EFW_WDT,
	EFW_CRC,
	EFW_REST,
	EFW_ERASE,
	EFW_PROGRAM,
};

enum TP_FW_BLOCK_NUM {
	AP = 1,
	DATA = 2,
	TUNING = 3,
	GESTURE = 4,
	MP = 5,
	DDI = 6,
	TAG = 7,
	PARA_BACKUP = 8,
	RESERVE_BLOCK3 = 9,
	RESERVE_BLOCK4 = 10,
	RESERVE_BLOCK5 = 11,
	RESERVE_BLOCK6 = 12,
	RESERVE_BLOCK7 = 13,
	RESERVE_BLOCK8 = 14,
	RESERVE_BLOCK9 = 15,
	RESERVE_BLOCK10 = 16,
};

enum TP_FW_BLOCK_TAG {
	BLOCK_TAG_AF = 0xAF,
	BLOCK_TAG_B0 = 0xB0
};

enum TP_FW_UPGRADE_STATUS {
	FW_STAT_INIT = 0,
	FW_UPDATING = 90,
	FW_UPDATE_PASS = 100,
	FW_UPDATE_FAIL = -1
};

#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
enum TP_TYPE {
     TP_NONE =-1,
	 TP_HLT = 0,
	 TP_DPT,
	 TP_MAX,
};
#endif
struct ili7807q_touch_abs_data {
	u8 y_high:4;
	u8 x_high:4;
	u8 x_low;
	u8 y_low;
	u8 pressure;
} __packed;
/*
struct ili7807q_touch_shape_data {
	s8 degree;
	u8 width_major_high;
	u8 width_major_low;
	u8 width_minor_high;
	u8 width_minor_low;
} __packed;
*/
/* report packet */
struct ili7807q_touch_info {
	u8 packet_id;
	struct ili7807q_touch_abs_data abs_data[10];
	u8 p_sensor:4;
	u8 key:4;
	//struct ili7807q_touch_shape_data shape_data[10];
	u8 checksum;
} __packed;

struct ili7807q_debug_info {
	u8 data[DEBUG_MODE_PACKET_LENGTH];
	u8 uart[DEBUG_MODE_PACKET_LENGTH];
	u8 buf[DEBUG_MODE_PACKET_FRAME_LIMIT][2048];
	u16 frame_cnt;
	bool enable;
} __packed;

struct ili7807q_gesture_info {
	u8 data[GESTURE_MODE_PACKET_INFO_LENGTH - 1];
	u8 checksum;
} __packed;

struct ili7807q_chip_info {
	u32 pid;
	u32 otp_id;
	u32 ana_id;
	u16 id;
	u8 type;
	u8 ver;
};

struct ili7807q_fw_info {
	u8 command_id;
	u8 core;
	u8 customer_code;
	u8 major;
	u8 minor;
	// not used below
	u8 mp_major_core;
	u8 mp_core;
	u8 release_code;
	u8 test_code;
} __packed;

struct ili7807q_fw_extra_info {
	u8 build;
} __packed;
struct ili7807q_protocol_info {
	u8 command_id;
	u8 major;
	u8 mid;
	u8 minor;
} __packed;

struct ili7807q_core_info {
	u8 command_id;
	u8 code_base; // for algorithm modify
	u8 minor;
	u8 revision_major; // for tunning parameter change
	u8 revision_minor;
} __packed;

struct ili7807q_ic_info {
	struct ili7807q_chip_info chip_info;
	struct ili7807q_fw_info fw_info;
	struct ili7807q_fw_extra_info fw_extra_info;
	struct ili7807q_protocol_info protocol_info;
	struct ili7807q_core_info core_info;
};

struct ili7807q_panel_info {
	u8 command_id;
	u8 nMinX;
	u8 nMinY;
	u8 nMaxX_Low;
	u8 nMaxX_High;
	u8 nMaxY_Low;
	u8 nMaxY_High;
	u8 nXChannelNum;
	u8 nYChannelNum;
	u8 nMaxTouchPoint;
	u8 nTouchKeyNum;
	/* added for protocol v5 */
	u8 self_tx_channel_num;
	u8 self_rx_channel_num;
	u8 side_touch_type;
} __packed;

#if 0
struct ili7807q_panel_extra_info {
	u8 glass_id[GLASS_ID_SIZE];
	u8 glass_info; //0: SIGNAL_LOW_SAMPLE, 1: SIGNAL_HIGH_SAMPLE
	u8 signal;
} __packed;
#endif

struct ili7807q_tp_info {
	struct ili7807q_panel_info panel_info;
//	struct ili7807q_panel_extra_info panel_extra_info;
};

struct ili7807q_data {
	struct device *dev;
	struct kobject kobj;
	struct mutex io_lock;
	struct mutex apk_lock;
	struct mutex debug_lock;
//	struct delayed_work fb_notify_work;
	struct bin_fw_info *hex_info;
	u8  fw_info[75];
	u8 *update_buf;
#ifdef DEEP_SLEEP_CTRL
	bool sleep_mode_choice;
#endif
#ifdef USE_XFER
//
#else
	u8 *spi_tx;
	u8 *spi_rx;
#endif
	struct project_param p_param;

	struct ili7807q_ic_info ic_info;
	struct ili7807q_tp_info tp_info;
	struct ili7807q_touch_info touch_info;
	struct ili7807q_debug_info debug_info;
	struct ili7807q_gesture_info gesture_info;

	atomic_t ice_stat;
	atomic_t init;
	atomic_t reset;
	atomic_t mp_int;
	atomic_t mp_stat;
	atomic_t changing_display_mode;
	wait_queue_head_t inq;

	u8 lcd_mode;
	bool charger;
	u16 actual_fw_mode;
	u8 tci_debug_type;
	u8 swipe_debug_type;
	u8 err_cnt;
	bool int_action;
	bool gesture_debug;
	bool trans_xy;
	u16 panel_wid;
	u16 panel_hei;
	bool sense_stop;
	bool dma_reset;

	bool boot;
	bool force_fw_update;
	char fwpath[256];
	bool get_path;
	bool info_from_hex;
	bool pll_clk_wakeup;
	bool tp_suspended;
#if defined(CONFIG_LGE_TOUCH_MODULE_DETECT)
	int panel_type;
#endif
};

extern void ili7807q_get_pc_latch(struct device *dev);
extern void ili7807q_dump_iram_data(struct device *dev, u32 start, u32 end);
extern int ili7807q_switch_fw_mode(struct device *dev, u8 mode);
extern int ili7807q_ice_mode_disable(struct device *dev);
extern int ili7807q_ice_mode_enable(struct device *dev, int mcu_status);
extern int ili7807q_ice_reg_write(struct device *dev, u32 addr, u32 data, int size);
extern int ili7807q_ice_reg_read(struct device *dev, u32 addr, void* data, int size);
extern int ili7807q_reg_read(struct device *dev, u8 cmd, void *data, int size);
extern int ili7807q_reg_write(struct device *dev, u8 cmd, void *data, int size);
extern int ili7807q_host_download_write(struct device *dev,void *data, int size);
extern void ili7807q_get_trans_report(struct device *dev);
extern int ili7807q_tp_info(struct device *dev);
extern int ili7807q_reset_ctrl(struct device *dev, int ctrl);
extern int ili7807q_ic_info(struct device *dev);
extern int ili7807q_check_cdc_busy(struct device *dev, int conut, int delay);
extern void ili7807q_dump_packet(void *data, int type, int len, int row_len, const char *name);
extern u8 ili7807q_calc_data_checksum(void *pMsg, u32 nLength);
extern int ili7807q_report_read(struct device *dev,void *rdata, int rsize);
// int ili7807q_ic_info(struct device *dev);
// int ili7807q_tc_driving(struct device *dev, int mode);
// int ili7807q_irq_abs(struct device *dev);
// int ili7807q_irq_lpwg(struct device *dev);
// int ili7807q_irq_handler(struct device *dev);
// int ili7807q_check_status(struct device *dev);
// int ili7807q_debug_info(struct device *dev);
extern int ili7807q_gesture_ctrl(struct device *dev, int tci_mode, bool swipe_enable);
extern int ili7807q_gesture_failreason_ctrl(struct device *dev, u8 tci_type, u8 swipe_type);
static inline struct ili7807q_data *to_ili7807q_data(struct device *dev)
{
	return (struct ili7807q_data *)touch_get_device(to_touch_core(dev));
}

static inline struct ili7807q_data *to_ili7807q_data_from_kobj(struct kobject *kobj)
{
	return (struct ili7807q_data *)container_of(kobj,
			struct ili7807q_data, kobj);
}
static inline int ili7807q_read_value(struct device *dev,
		u16 addr, u32 *value)
{
	return ili7807q_reg_read(dev, addr, value, sizeof(*value));
}

static inline int ili7807q_write_value(struct device *dev,
		u16 addr, u32 value)
{
	return ili7807q_reg_write(dev, addr, &value, sizeof(value));
}

static inline void ipio_vfree(void **mem)
{
	if (*mem != NULL) {
		vfree(*mem);
		*mem = NULL;
	}
}

static inline void ipio_kfree(void **mem)
{
	if(*mem != NULL) {
		kfree(*mem);
		*mem = NULL;
	}
}
static inline void *ipio_memcpy(void *dest, const void *src, int n, int dest_size)
{
	if (n > dest_size)
		 n = dest_size;

	return memcpy(dest, src, n);
}

static inline int ipio_strcmp(const char *s1, const char *s2)
{
	return (strlen(s1) != strlen(s2)) ? -1 : strncmp(s1, s2, strlen(s1));
}
/* extern */
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
extern int check_recovery_boot;
#endif

#if defined(CONFIG_LGE_MODULE_DETECT)
extern int panel_id;
#endif

/* Extern our apk api for LG */
extern int ili7807q_apk_init(struct device *dev);
extern int ili7807q_check_int_status(struct device *dev, bool high);
extern int ili7807_get_fw_data_wrapper(struct device *dev, u8 *wdata, int wsize, void *rdata, int rsize);
extern void ili7807q_get_ddi_reg_onepage(u8 page, u8 reg, u8 *data);

#endif /* LGE_TOUCH_ILI7807q_H */
