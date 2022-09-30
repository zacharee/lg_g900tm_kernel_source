#ifndef __LM3697_H__
#define __LM3697_H__

#define LM3697_DEV_NAME        "I2C_LM3697"
#define LM3697_DTS_NODE        "mediatek,i2c_lm3697"

#define ENABLE                  1
#define DISABLE                 0

#define LM3697_REG10_ADDR   0x10
#define LM3697_REG11_ADDR   0x11
#define LM3697_REG12_ADDR   0x12
#define LM3697_REG13_ADDR   0x13
#define LM3697_REG14_ADDR   0x14
#define LM3697_REG16_ADDR   0x16
#define LM3697_REG17_ADDR   0x17
#define LM3697_REG18_ADDR   0x18
#define LM3697_REG19_ADDR   0x19
#define LM3697_REG1A_ADDR   0x1A
#define LM3697_REG1B_ADDR   0x1B
#define LM3697_REG1C_ADDR   0x1C
#define LM3697_REG20_ADDR   0x20
#define LM3697_REG21_ADDR   0x21
#define LM3697_REG22_ADDR   0x22
#define LM3697_REG23_ADDR   0x23
#define LM3697_REG24_ADDR   0x24

#define HVLED1_CONFIG_SHFT 			0
#define HVLED1_CONFIG_MASK 			0x01
#define HVLED2_CONFIG_SHFT 			1
#define HVLED2_CONFIG_MASK 			0x02
#define HVLED3_CONFIG_SHFT 			2
#define HVLED3_CONFIG_MASK 			0x04
#define MAPPING_MODE_SHFT 			0
#define MAPPING_MODE_MASK 			0x01
#define DITHER_DISABLE_CONTROL_A_SHFT 	2
#define DITHER_DISABLE_CONTROL_A_MASK 	0x04
#define DITHER_DISABLE_CONTROL_B_SHFT 	3
#define DITHER_DISABLE_CONTROL_B_MASK 	0x08
#define FULL_SCALE_CURRENT_SHFT 		0
#define FULL_SCALE_CURRENT_MASK 		0x1E
#define CURRENT_SINK_FEEDBACK_A_SHFT 	0
#define CURRENT_SINK_FEEDBACK_A_MASK 	0x01
#define CURRENT_SINK_FEEDBACK_B_SHFT 	1
#define CURRENT_SINK_FEEDBACK_B_MASK 	0x02
#define CURRENT_SINK_FEEDBACK_C_SHFT 	2
#define CURRENT_SINK_FEEDBACK_C_MASK 	0x04
#define BOOST_FREQUENCY_SHFT 		0
#define BOOST_FREQUENCY_MASK 		0x01
#define BOOST_OVP_SHFT 				1
#define BOOST_OVP_MASK 				0x06
#define PWM_ENABLE_A_SHFT 			0
#define PWM_ENABLE_A_MASK 			0x01
#define PWM_ENABLE_B_SHFT 			1
#define PWM_ENABLE_B_MASK 			0x02
#define CONTROL_A_ENABLE_SHFT 		0
#define CONTROL_A_ENABLE_MASK 		0x01
#define CONTROL_B_ENABLE_SHFT 		1
#define CONTROL_B_ENABLE_MASK 		0x02

typedef enum {
	VPOS = 0,
	VNEG,
	DISC,
	ENAR,
	KNOK
} LM3697_REG;

typedef struct lm3697_platdata {
	uint8_t hvled1;
	uint8_t hvled2;
	uint8_t hvled3;
	uint8_t mapping_mode;
	uint8_t dither_a;
	uint8_t dither_b;
	uint32_t full_scale_cur;
	uint8_t sink_feedback_a;
	uint8_t sink_feedback_b;
	uint8_t sink_feedback_c;
	uint8_t boost_freq;
	uint8_t boost_ovp;
	uint8_t pwm_en_a;
    uint8_t pwm_en_b;
    uint8_t control_a_en;
    uint8_t control_b_en;
}LM3697_BLED_DSV_DATA;

#endif
