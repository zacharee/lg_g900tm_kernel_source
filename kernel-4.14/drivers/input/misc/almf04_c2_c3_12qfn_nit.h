#ifndef __ALMF04_12QFN_H__
#define __ALMF04_12QFN_H__

#define ALMF04_DRIVER_VERSION 0x0005

/*
 * History
 * This section contains comments describing changes made to the module.
 * Notice that changes are listed in reverse chronolgical order
 *
 * when          version        who                what
 * -------------------------------------------------------------------------------------------------------------
 * 2020-07-08    0x0005   LGE    delay request irq after firmware loading
 * 2020-07-01    0x0005   ADSem  Change the variable type of duty_val and per_result for the following functions
 *                               almf04_store_regreset(), almf04_show_regproxdata(), almf04_show_check_far(), almf04_show_check_mid()
 * 2020-06-10    0x0004   ADSem   add SWReset of the Update_Register() function - SWReset After Roms Save
 *
 * 2020-06-03    0x0003   ADSem   1. add new register of Init Code (0x26)
 *                                2. change the almf04_show_regproxdata() function internal per_result variable type
 *
 * 2020-05-29    0x0002   LGE     add callback function for IC SW Reset.
 *                                ex) when detecting usb connection or change capacitive.
 * 2020-05-28    0x0001   ADSem   create almf04 driver for auto calibration with firmware(3.10)
 * -------------------------------------------------------------------------------------------------------------
 */

/*! ERROR LOG LEVEL */
#define LOG_LEVEL_E 3
/*! NOTICE LOG LEVEL */
#define LOG_LEVEL_N 5
/*! INFORMATION LOG LEVEL */
#define LOG_LEVEL_I 6
/*! DEBUG LOG LEVEL */
#define LOG_LEVEL_D 7

#ifndef LOG_LEVEL
/*! LOG LEVEL DEFINATION */
#define LOG_LEVEL LOG_LEVEL_I
#endif

#ifndef MODULE_TAG
/*! MODULE TAG DEFINATION */
#define MODULE_TAG "[ALMF04] "
#endif

#if (LOG_LEVEL >= LOG_LEVEL_E)
/*! print error message */
#define PERR(fmt, args...) \
	pr_err(MODULE_TAG \
	"%s: " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PERR(fmt, args...)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_N)
/*! print notice message */
#define PNOTICE(fmt, args...) \
	pr_notice(MODULE_TAG \
	"%s: " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PNOTICE(fmt, args...)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_I)
/*! print information message */
#define PINFO(fmt, args...) pr_info(MODULE_TAG \
	"%s: " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PINFO(fmt, args...)
#endif

#if (LOG_LEVEL >= LOG_LEVEL_D)
/*! print debug message */
#define PDEBUG(fmt, args...) pr_devel(MODULE_TAG \
	"%s: " fmt "\n", __func__, ##args)
#else
/*! invalid message */
#define PDEBUG(fmt, args...)
#endif
#define USE_ALMF04

#define SZ_PAGE_DATA                64

#define ADDR_EFLA_STS               0xFF	//eflash status register
#define ADDR_EFLA_PAGE_L            0xFD	//eflash page
#define ADDR_EFLA_PAGE_H            0xFE	//eflash page
#define ADDR_EFLA_CTRL              0xFC	//eflash control register

#if defined(USE_ALMF04)
#define ADDR_ROM_SAFE               0xFB	//eflash write protection
#define VAL_ROM_MASK1               0x02
#define VAL_ROM_MASK2               0x06

#define EMD_ALL_ERASE               (0x07 << 1)
#define EMD_PG_ERASE                (0x04 << 1)
#define EMD_PG_WRITE                (0x08 << 1)
#define EMD_PG_READ                 0x00

#define CMD_EEP_START               0x01
#define CMD_EUM_START               0x03

#define FLAG_DONE                   0x03
#define FLAG_DONE_ERASE             0x03
#else
#define CMD_EFL_L_WR                0x01	//Eflash Write
#define CMD_EFL_RD                  0x03	//Eflash Read
#define CMD_EFL_ERASE_ALL           0x07	//Eflash All Page Erase

#define CMD_EUM_WR                  0x21	//Extra user memory write
#define CMD_EUM_RD                  0x23	//Extra user memory read
#define CMD_EUM_ERASE               0x25	//Extra user memory erase

#define FLAG_DONE                   0x03
#define FLAG_DONE_ERASE             0x02
#endif

#define FLAG_FUSE                   1
#define FLAG_FW                     2

#define FL_EFLA_TIMEOUT_CNT         20
#define IC_TIMEOUT_CNT              1

#define RTN_FAIL                    0
#define RTN_SUCC                    1
#define RTN_TIMEOUT                 2

#define ON                          1
#define OFF                         2

enum {
	DEV_PM_RESUME = 0,
	DEV_PM_SUSPEND,
	DEV_PM_SUSPEND_IRQ,
};

#if 1 // debugging calibration paused
#define CAP_CAL_RESULT_PASS         "pass"
#define CAP_CAL_RESULT_FAIL         "fail"
#endif

//============================================================//
//[200603] ADS Change
//[START]=====================================================//
#define CNT_INITCODE					42
static const unsigned char InitCodeAddr[CNT_INITCODE] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B };
static const unsigned char InitCodeVal[CNT_INITCODE] =  { 0x00, 0x19, 0x00, 0x1F, 0x00, 0x19, 0x53, 0x13, 0x13, 0x13, 0x0F, 0x15, 0x82, 0x64, 0xDC, 0x64, 0xA0, 0x64, 0xD5, 0x89, 0x20, 0x10, 0x14, 0x0C, 0x0B, 0x07, 0x0B, 0x07, 0x43, 0x07, 0x19, 0x0E, 0x31, 0x0E, 0x31, 0xC2, 0x4F, 0x02, 0x8F, 0x03, 0x07, 0x04 };
//[END]=======================================================//

//Register Setting version Check Using
//#define REG_VER_CHK_USE

#define IDX_REG_VER						(CNT_INITCODE - 2)

/* I2C Register */
#define	I2C_ADDR_SSTVT_CH1_H			0x01
#define	I2C_ADDR_SSTVT_CH1_L			0x02
#define	I2C_ADDR_SSTVT_CH2_H			0x03
#define	I2C_ADDR_SSTVT_CH2_L			0x04
#define	I2C_ADDR_SSTVT_CH3_H			0x05
#define	I2C_ADDR_SSTVT_CH3_L			0x06

#define	I2C_ADDR_SFDT1_MIN				0x1F
#define	I2C_ADDR_SFDT1_MAX				0x20
#define	I2C_ADDR_SFDT2_MIN				0x21
#define	I2C_ADDR_SFDT2_MAX				0x22
#define	I2C_ADDR_SFDT3_MIN				0x23
#define	I2C_ADDR_SFDT3_MAX				0x24

//============================================================//
//[200512] ADS Add
//[START]=====================================================//
#define I2C_ADDR_USE_CH_INF				0x2A
//[END]=======================================================//

#define	I2C_ADDR_SYS_CTRL				0x31
#define	I2C_ADDR_SYS_STAT				0x32
#define	I2C_ADDR_SYS_STAT2				0x33
#define	I2C_ADDR_TCH_OUTPUT				0x34
#define	I2C_ADDR_CH1_PER_H				0x35
#define	I2C_ADDR_CH1_PER_L				0x36
#define	I2C_ADDR_CH2_PER_H				0x37
#define	I2C_ADDR_CH2_PER_L				0x38
#define	I2C_ADDR_CH3_PER_H				0x39
#define	I2C_ADDR_CH3_PER_L				0x3A
#define	I2C_ADDR_CR_DUTY_H				0x3B
#define	I2C_ADDR_CR_DUTY_L				0x3C
#define	I2C_ADDR_CS1_DUTY_H				0x3D
#define	I2C_ADDR_CS1_DUTY_L				0x3E
#define	I2C_ADDR_CS2_DUTY_H				0x3F
#define	I2C_ADDR_CS2_DUTY_L				0x40
#define	I2C_ADDR_CS3_DUTY_H				0x41
#define	I2C_ADDR_CS3_DUTY_L				0x42

#define	I2C_ADDR_REG_VER				0x29
#define	I2C_ADDR_USE_CH_INF				0x2A

#define I2C_ADDR_PGM_VER_MAIN			0x71
#define I2C_ADDR_PGM_VER_SUB			0x72

//Calibration Data Backup/Restore
#define I2C_ADDR_CMD_OPT 					0x7E
#define I2C_ADDR_COMMAND 					0x7F
#define I2C_ADDR_REQ_DATA					0x80
#define CMD_R_CD_DUTY						0x04		//Cal Data Duty Read
#define CMD_R_CD_REF						0x05		//Cal Data Ref Read
#define CMD_W_CD_DUTY						0x84		//Cal Data Duty Read
#define CMD_W_CD_REF						0x85		//Cal Data Ref Read

#ifdef CONFIG_LGE_SAR_CONTROLLER_USB_DETECT
#define USB_CONNECTION 0x01
#endif

#endif
