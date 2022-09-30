#ifndef __ATMF04_EFLASH_H__
#define __ATMF04_EFLASH_H__


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

//============================================================//
//[190306] ADS Add
//[START]=====================================================//
#define USE_ALMF04
//[END]======================================================//

#define SZ_PAGE_DATA                64

#define ADDR_EFLA_STS               0xFF //eflash status register
#define ADDR_EFLA_PAGE_L            0xFD //eflash page
#define ADDR_EFLA_PAGE_H            0xFE //eflash page
#define ADDR_EFLA_CTRL              0xFC //eflash control register

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

#endif // USE_ALMF04
//[END]======================================================//

#define FLAG_FUSE                   1
#define FLAG_FW                     2

#define FL_EFLA_TIMEOUT_CNT         20
#define IC_TIMEOUT_CNT        1

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

#endif

#define CONFIG_LGE_ATMF04_2CH

#if defined(CONFIG_LGE_ATMF04_2CH)
#define CNT_INITCODE               26
#else
#define CNT_INITCODE               17
#endif

// Each operator use different initcode value
#if defined(CONFIG_LGE_ATMF04_2CH)
static const unsigned char InitCodeAddr[CNT_INITCODE]     = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09, 0x0A, 0x0B, 0X0C, 0X0D, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D };

#if defined(CONFIG_MACH_MT6765_HDK)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0, 0x00, 0x00, 0x00, 0x00 };
#elif defined(CONFIG_MACH_MT6762_MH45LM)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0x1D, 0x00, 0x4E, 0x33, 0x0B, 0x0B, 0xE6, 0x82, 0xA0, 0x6E, 0x5F, 0x0B, 0x00, 0x10, 0x00, 0x4E, 0xD0, 0xA4, 0x0B, 0x04, 0x14, 0x07, 0x33, 0x01, 0x09 };
#elif defined(CONFIG_MACH_MT6762_MH55LM)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0xC5, 0x00, 0x31, 0x31, 0x0B, 0x0B, 0x82, 0x64, 0x82, 0x64, 0x5F, 0x9F, 0x00, 0xC5, 0x00, 0x21, 0xD0, 0xA4, 0x12, 0x07, 0x14, 0x07, 0x33, 0x02, 0x0A };
#elif defined(CONFIG_MACH_MT6762_DH10LM)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0xA4, 0x00, 0x52, 0x33, 0x0B, 0x0B, 0x82, 0x64, 0x82, 0x64, 0x5F, 0xA7, 0x00, 0x66, 0x00, 0x19, 0xD0, 0xA4, 0x12, 0x07, 0x14, 0x09, 0x33, 0x01, 0x07 };
#elif defined(CONFIG_MACH_MT6762_DH10XLM)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0xA4, 0x00, 0x52, 0x33, 0x0B, 0x0B, 0x82, 0x64, 0x82, 0x64, 0x5F, 0xA7, 0x00, 0x66, 0x00, 0x19, 0xD0, 0xA4, 0x12, 0x07, 0x14, 0x09, 0x33, 0x01, 0x07 };
#elif defined(CONFIG_MACH_MT6762_DH30LM)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0x52, 0x00, 0x52, 0x33, 0x0B, 0x0B, 0x82, 0x64, 0x81, 0x6A, 0x5F, 0x67, 0x00, 0x14, 0x00, 0x10, 0xD0, 0xA4, 0x14, 0x09, 0x12, 0x07, 0x33, 0x02, 0x2F };
#elif defined(CONFIG_MACH_MT6762_DH30XLM)
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0x52, 0x00, 0x52, 0x33, 0x0B, 0x0B, 0x82, 0x64, 0x81, 0x6A, 0x5F, 0x67, 0x00, 0x14, 0x00, 0x10, 0xD0, 0xA4, 0x14, 0x09, 0x12, 0x07, 0x33, 0x02, 0x2F };
#else // default
static const unsigned char InitCodeVal[CNT_INITCODE]      = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0, 0x00, 0x00, 0x00, 0x00 };
#endif

#else // 1CH
static const unsigned char InitCodeAddr[CNT_INITCODE]    = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0C, 0x0D, 0x0E, 0x1A, 0x1B, 0x1C, 0x1D, 0x20, 0x21 };
static const unsigned char InitCodeVal[CNT_INITCODE]     = { 0x00, 0x7A, 0x33, 0x0B, 0x08, 0x6B, 0x68, 0x17, 0x00, 0x14, 0x7F, 0x00, 0x0B, 0x00, 0x07, 0x81, 0x20 }; // High Band ANT , auto cal 15%, sensing 2.5%, LNF filter
#endif
