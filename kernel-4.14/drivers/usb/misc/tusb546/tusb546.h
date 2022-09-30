#ifndef __TUSB546_H__
#define __TUSB546_H__

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/rwsem.h>

#include "tusb546_i2c.h"
#include "tusb546_debug.h"


struct tusb546 {
	struct i2c_client  *i2c;
	struct regulator *avdd;

	/* gpio */
	int pwr_en_gpio;
	int resetn_gpio;

	/* cross switch */
	int edu_aux;		/* ANALOG_CTRL_2 */
	int ml_ss_path1;	/* ANALOG_CTRL_1 */
	int ml_ss_path2;	/* ANALOG_CTRL_5 */

	u8 dp0eq_sel;				/* DP0 EQ SEL VALUE */
	u8 dp1eq_sel;				/* DP1 EQ SEL VALUE */
	u8 dp2eq_sel;				/* DP2 EQ SEL VALUE */
	u8 dp3eq_sel;				/* DP3 EQ SEL VALUE */

	u8 ssrx_eq1_sel;				/* USB  3.1    RX   EQ1 SEL VALUE */
	u8 ssrx_eq2_sel;				/* USB  3.1    RX   EQ2 SEL VALUE */
	u8 sstx_eq_sel;				/* USB  3.1    TX   EQ SEL VALUE */

	int mode;

	atomic_t pwr_on;
};

/* TUSB546A I2C REGISTER */
#define GENERAL_REGISTER		0x0A
#define B_GR_CTLSEL				(BIT(0) | BIT(1))
#define B_GR_FLIPSEL			BIT(2)
#define B_GR_HPDIN_OVRRIDE		BIT(3)
#define B_GR_EQ_OVERRIDE		BIT(4)
#define B_GR_SWAP_HPDIN			BIT(5)

#define DISPLAY_CTRL_STS_1		0x10
#define B_DCS1_DP0EQ_SEL		(BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define B_DCS1_DP1EQ_SEL		(BIT(4) | BIT(5) | BIT(6) | BIT(7))

#define DISPLAY_CTRL_STS_2		0x11
#define B_DCS2_DP2EQ_SEL		(BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define B_DCS2_DP3EQ_SEL		(BIT(4) | BIT(5) | BIT(6) | BIT(7))

#define DISPLAY_CTRL_STS_3		0x12	/* R/U */
#define B_DCS3_LANE_COUNT_SET	(BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4))
#define B_DCS3_SET_POWER_STATE	(BIT(5) | BIT(6))

#define DISPLAY_CTRL_STS_4		0x13
#define B_DCS4_DP0_DISABLE		BIT(0)
#define B_DCS4_DP1_DISABLE		BIT(1)
#define B_DCS4_DP2_DISABLE		BIT(2)
#define B_DCS4_DP3_DISABLE		BIT(3)
#define B_DCS4_AUX_SBU_OVR		BIT(4) | BIT(5)
#define B_DCS4_AUX_SNOOP_DISABLE		BIT(7)

#define USB3_CTRL_STS_1			0x20
#define B_UCS1_EQ1_SEL			(BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define B_UCS1_EQ2_SEL			(BIT(4) | BIT(5) | BIT(6) | BIT(7))

#define USB3_CTRL_STS_2			0x21
#define B_UCS2_SSEQ_SEL			(BIT(0) | BIT(1) | BIT(2) | BIT(3))

#define USB3_CTRL_STS_3			0x22
#define B_UCS3_USB3_COMPLIANCE_CTRL		(BIT(0) | BIT(1))
#define B_UCS3_DFP_RXDET_INTERVAL		(BIT(2) | BIT(3))
#define B_UCS3_DISABLE_U2U3_RXDET		BIT(4)
#define B_UCS3_U2U3_LFPS_DEBOUNCE		BIT(5)
#define B_UCS3_LFPS_EQ					BIT(6)
#define B_UCS3_CM_ACTIVE				BIT(7)

#define DEFAULT_DP_EQ_VALUE		0x07
#define DEFAULT_SS_EQ_VALUE		0x07

enum lge_tusb_mode {
	LGE_TUSB_MODE_DISABLE = 0,
	LGE_TUSB_MODE_USB3_ONLY,
	LGE_TUSB_MODE_DP_4_LANE,
	LGE_TUSB_MODE_DP_2_LANE,
	LGE_TUSB_MODE_USB3_ONLY_FLIP = 5,
	LGE_TUSB_MODE_DP_4_LANE_FLIP,
	LGE_TUSB_MODE_DP_2_LANE_FLIP,
	LGE_TUSB_MODE_MAX,
};

enum lge_tusb_eq_number {
	LGE_TUSB_DP0_EQ = 0,
	LGE_TUSB_DP1_EQ,
	LGE_TUSB_DP2_EQ,
	LGE_TUSB_DP3_EQ,
	LGE_TUSB_SSRX1_EQ,
	LGE_TUSB_SSRX2_EQ,
	LGE_TUSB_SSTX_EQ,
	LGE_TUSB_EQ_MAX,
};


int tusb546_pwr_on(struct tusb546 *tusb, int is_on);
bool is_tusb546_pwr_on(void);
void set_tusb546_off(void);
void set_tusb546_rollback(void);

void tusb546_read_cross_switch(void);
void tusb546_update_cross_switch(int mode);
void tusb546_update_eq_val(int eq_num, u8 value);
void resetting_ds3_register(void);
bool is_tusb546_dp(void);

#endif /* __TUSB546_H__ */
