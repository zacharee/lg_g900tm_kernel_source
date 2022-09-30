#ifndef __LM36272_H__
#define __LM36272_H__

/*****************************************************************************
 * Define
 *****************************************************************************/
#define LM36272_I2C_ID_NAME		"I2C_LM36272"
#define LM36272_DTS_NODE        "mediatek,i2c_lm36272"

#define LM36272_REG1_ADDR   0x01
#define LM36272_REG2_ADDR   0x02
#define LM36272_REG3_ADDR   0x03
#define LM36272_REG4_ADDR   0x04
#define LM36272_REG5_ADDR   0x05
#define LM36272_REG6_ADDR   0x06
#define LM36272_REG7_ADDR   0x07
#define LM36272_REG8_ADDR   0x08
#define LM36272_REG9_ADDR   0x09
#define LM36272_REG10_ADDR   0x0A
#define LM36272_REG11_ADDR   0x0B
#define LM36272_REG12_ADDR   0x0C
#define LM36272_REG13_ADDR   0x0D
#define LM36272_REG14_ADDR   0x0E

#define LM36272_BLED_OVP_SHFT (5)
#define LM36272_BLED_OVP_MASK (0xE0)
#define LM36272_BLED_OVPMODE_SHFT (4)
#define LM36272_BLED_OVPMODE_MASK (0x10)
#define LM36272_BLED_PWMRAMP_SHFT (1)
#define LM36272_BLED_PWMRAMP_MASK (0x02)
#define LM36272_BLED_PWMENABLE_SHFT (0)
#define LM36272_BLED_PWMENABLE_MASK (0x01)
#define LM36272_BLED_BOOSTFREQ_SHFT (7)
#define LM36272_BLED_BOOSTFREQ_MASK (0x80)
#define LM36272_BLED_PWMFSAMPLE_SHFT (1)
#define LM36272_BLED_PWMFSAMPLE_MASK (0x02)
#define LM36272_BLED_BLEN_SHFT (4)
#define LM36272_BLED_BLEN_MASK (0x10)
#define LM36272_BLED_LED1EN_SHFT (0)
#define LM36272_BLED_LED1EN_MASK (0x01)
#define LM36272_BLED_LED2EN_SHFT (1)
#define LM36272_BLED_LED2EN_MASK (0x02)
#define LM36272_DSV_VPOSDISCH_SHFT (4)
#define LM36272_DSV_VPOSDISCH_MASK (0x10)
#define LM36272_DSV_VNEGDISCH_SHFT (3)
#define LM36272_DSV_VNEGDISCH_MASK (0x08)
#define LM36272_DSV_VPOSEN_SHFT (2)
#define LM36272_DSV_VPOSEN_MASK (0x04)
#define LM36272_DSV_VNEGEN_SHFT (1)
#define LM36272_DSV_VNEGEN_MASK (0x02)
#define LM36272_DSV_EXTEN_SHFT (0)
#define LM36272_DSV_EXTEN_MASK (0x01)
#define LM36272_DSV_VPOS_SHFT (0)
#define LM36272_DSV_VPOS_MASK (0x3F)
#define LM36272_DSV_VNEG_SHFT (0)
#define LM36272_DSV_VNEG_MASK (0x3F)

/*****************************************************************************
 * Function variables
 *****************************************************************************/

typedef struct lm36272_platdata {
	uint8_t bl_ovp;
	uint8_t ovp_mode;
	uint8_t pwm_ramp;
	uint8_t pwm_enable;
	uint8_t boost_freq;
	uint8_t pwm_fsample;
	uint8_t bl_en;
	uint8_t led1_en;
	uint8_t led2_en;
	uint8_t vpos_disch;
    uint8_t vneg_disch;
    uint8_t vpos_en;
    uint8_t vneg_en;
    uint8_t ext_en;
    uint8_t vpos;
    uint8_t vneg;
}LM36272_BLED_DSV_DATA;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
#ifdef CONFIG_LGE_USE_BRIGHTNESS_TABLE
extern unsigned int* blmap_arr;
#endif

int chargepump_set_backlight_level(unsigned int level);
void lm36272_chargepump_dsv_enable(int enable, int delay);

#endif
