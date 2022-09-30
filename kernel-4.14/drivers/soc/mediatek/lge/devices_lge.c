#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <soc/mediatek/lge/board_lge.h>
#if defined(CONFIG_LGE_HANDLE_PANIC)
#include <soc/mediatek/lge/lge_handle_panic.h>
#endif
#include <mt-plat/mtk_printk_ctrl.h>

static hw_rev_type lge_bd_rev;
unsigned int laf_boot = 0;

/* CAUTION: These strings are come from LK. */
char *rev_str[] = {
	"rev_0",
	"rev_0_1",
	"rev_a",
	"rev_b",
	"rev_c",
#if defined(CONFIG_MACH_MT6762_MH55LM) || defined (CONFIG_MACH_MT6762_MH45LM)
	"rev_d",
#endif
	"rev_10",
	"rev_11",
	"reserved",
};

static int __init board_revno_setup(char *rev_info)
{
	int i;
	lge_bd_rev	= HW_REV_0;
	for (i = 0; i <= HW_REV_MAX; i++)	{
		if(!strncmp(rev_info, rev_str[i], (strlen(rev_info) > strlen(rev_str[i])) ? strlen(rev_info) : strlen(rev_str[i])))	{
			lge_bd_rev	= (hw_rev_type) i;
			/* it is defined externally in <asm/system_info.h> */
			/* system_rev = lge_bd_rev; */
			break;
		}
	}

	printk(KERN_ERR "unified LK bootcmd lge.rev setup: %s\n", rev_str[lge_bd_rev]);

	return 1;
}
__setup("androidboot.vendor.lge.hw.revision=", board_revno_setup);

#ifdef CONFIG_LGE_DRAM_WR_BIAS_OFFSET
static int wr_bias_offset = 0;
static int __init lge_get_wr_bias_offset_setup(char *bias_offset)
{
	int ret = 0;
	ret = kstrtoint(bias_offset, 10, &wr_bias_offset);
	if (!ret)
		printk("wr_bias_offset: %d\n", wr_bias_offset);
	else
		printk("wr_bias_offset: Couldn't get wr_bias_offset (error: %d)\n", ret);

	return 1;
}
__setup("lge.wr_bias=", lge_get_wr_bias_offset_setup);

int lge_get_wr_bias_offset(void)
{
	return wr_bias_offset;
}
#endif

hw_rev_type lge_get_board_revno(void)
{
	return lge_bd_rev;
}

static int lcm_revision = 1;
static int __init lcm_revision_setup(char *value)
{

	int ret = 0;

	ret = kstrtoint(value, 10, &lcm_revision);
	if (!ret)
		printk("lcm_revision: %d \n", lcm_revision );
	else
		printk("lcm_revision: Couldn't get lcm revision%d \n", ret );

	return 1;
}
__setup("lcm_rev=", lcm_revision_setup);

int lge_get_lcm_revision(void)
{
    return lcm_revision;
}

static char lcm_name[30];

static int __init lcm_name_setup(char *value)
{
    memset(lcm_name,0x00,sizeof(lcm_name));

    if( value != NULL ){
			strncpy(lcm_name,value,sizeof(lcm_name)-1);
			printk("lcm_name: %s\n", lcm_name);
    } else {
			printk("Couldn't get lcm name \n");
    }

    return 1;
}
__setup("lcm_name=", lcm_name_setup);

bool is_lcm_name(char *lcm_full_name)
{
    bool ret = false;

    if(!strcmp(lcm_full_name,lcm_name)) {
			ret = true;
    }
    return ret;
}
EXPORT_SYMBOL(is_lcm_name);

char* lge_get_lcm_name(void)
{
    return lcm_name;
}
static char device_name[30];

static int __init device_name_setup(char *value)
{
	memset(device_name, 0x00, sizeof(device_name));

	if (value != NULL) {
		strncpy(device_name, value, sizeof(device_name) - 1);
		printk("device_name: %s\n", device_name);
	} else {
		printk("Couldn't get device name\n");
	}

	return 1;
}
__setup("androidboot.vendor.lge.product.device=", device_name_setup);

char *lge_get_device_name(void)
{
	    return device_name;
}

static char device_lcm_name[30];

bool is_device_lcm_name(char *d_lcm_name)
{
	bool ret = false;

	memset(device_lcm_name, 0x00, sizeof(device_lcm_name));

	printk("[Touch] %s\n", __func__);

	strncat(device_lcm_name, device_name, strlen(device_name));
	strncat(device_lcm_name, "-", 1);
	strncat(device_lcm_name, lcm_name, strlen(lcm_name));

	printk("[Touch] device_lcm_name: %s\n", device_lcm_name);

	if(!strcmp(d_lcm_name,device_lcm_name)) {
		ret = true;
	}

	return ret;
}
EXPORT_SYMBOL(is_device_lcm_name);

char* lge_get_device_lcm_name(void)
{
    return device_lcm_name;
}

static enum lge_laf_mode_type lge_laf_mode = LGE_LAF_MODE_NORMAL;
static bool lge_laf_mid = false;

int __init lge_laf_mode_init(char *s)
{
    if (strcmp(s, "") && strcmp(s, "MID")){
        lge_laf_mode = LGE_LAF_MODE_LAF;
		laf_boot = 1;
    }

    if(!strcmp(s, "MID")) {
	lge_laf_mid = true;
    }
    return 1;
}
__setup("androidboot.vendor.lge.laf=", lge_laf_mode_init);

static int lge_wmc_support;
int lge_get_wmc_support(void)
{
	return lge_wmc_support;
}
EXPORT_SYMBOL(lge_get_wmc_support);

static int __init lge_wmc_support_setup(char *s)
{
	if (!strcmp(s, "1"))
		lge_wmc_support = 1;
	else if (!strcmp(s, "2"))
		lge_wmc_support = 2;
	else
		lge_wmc_support = 0;

	pr_info("[MME] lge_wmc_support: %d\n", lge_wmc_support);

	return 1;
}
__setup("androidboot.vendor.lge.wmc=", lge_wmc_support_setup);

enum lge_laf_mode_type lge_get_laf_mode(void)
{
    return lge_laf_mode;
}

bool lge_get_laf_mid(void)
{
    return lge_laf_mid;
}

static bool is_mfts_mode = 0;
static int __init lge_mfts_mode_init(char *s)
{
        if(strncmp(s,"1",1) == 0)
                is_mfts_mode = 1;
        return 0;
}
__setup("mfts.mode=", lge_mfts_mode_init);

bool lge_get_mfts_mode(void)
{
        return is_mfts_mode;
}

/*
 * get from LG QCT source code
 */
static int lge_boot_reason = -1; /* undefined for error checking */
static int __init lge_check_bootreason(char *reason)
{
	int ret = 0;

#if 0
	/* handle corner case of kstrtoint */
	if (!strcmp(reason, "0xffffffff")) {
		lge_boot_reason = 0xffffffff;
		return 1;
	}

	ret = kstrtoint(reason, 16, &lge_boot_reason);
#endif

	ret = kstrtoint(reason, 0, &lge_boot_reason);
	if (!ret)
		printk(KERN_INFO "LGE REBOOT REASON: %x\n", lge_boot_reason);
	else
		printk(KERN_INFO "LGE REBOOT REASON: "
					"Couldn't get bootreason - %d\n", ret);

	return 1;
}
__setup("androidboot.product.lge.bootreasoncode=", lge_check_bootreason);

int lge_get_bootreason(void)
{
	return lge_boot_reason;
}

static int __init lge_uart_setup(char *s)
{
	if (!strcmp(s, "enable")) {
		set_uartlog_status(1);
		printk(KERN_INFO "LGE UART Enable\n");
	}
	/*
	 * If lge.uart is disable, uart is enabled or disabled by printk.disable_uart
	 *
	else if (!strcmp(s, "disable")) {
		set_uartlog_status(0);
		printk(KERN_INFO "LGE UART Disable\n");
	}
	*/
	return 1;
}
__setup("lge.uart=", lge_uart_setup);


int of_scan_meid_node(char *meid, int len)
{
	struct device_node *node;
	const char *tmp;

	node = of_find_node_by_path("/chosen");
	if (node == NULL) {
	pr_err("%s: chosen node not found\n", __func__);
		return 0;
	}

	if (of_property_read_string(node, "lge,meid", &tmp)) {
	pr_err("%s: lge,meid not found\n", __func__);
		return 0;
	}
	memcpy(meid, tmp, len);

	return 1;
}
EXPORT_SYMBOL(of_scan_meid_node);

static char model_name[20];

static int __init lge_model_name_init(char *m_name)
{
	int len = 0;

	if (m_name == NULL) {
		pr_err("Failed get model.name. maybe null\n");
		return 0;
	}

	len = strlen(m_name);

	if (len == 0) {
		pr_info("Failed get model.name\n");
		return 0;
	}

	strncpy(model_name, m_name,
			(len < (sizeof(model_name) - 1) ?
			 len : (sizeof(model_name) - 1)));

	pr_info("model.name = %s\n", model_name);

	return 1;
}
__setup("androidboot.vendor.lge.model.name=", lge_model_name_init);

char *lge_get_model_name(void)
{
	return model_name;
}

static int lge_wm_id = -1; /* undefined for error checking */
static int __init lge_set_wm_id(char *wm_id)
{
	int ret = 0;

	ret = kstrtoint(wm_id, 0, &lge_wm_id);
	if (!ret)
		printk(KERN_INFO "LGE wm_id: %d\n", lge_wm_id);
	else
		printk(KERN_INFO "LGE wm_id: "
					"Couldn't get wm_id - %d\n", ret);

	return 1;
}
__setup("androidboot.vendor.lge.wm_id=", lge_set_wm_id);


static const int ap_md_wm_id_map_array[] = { 0x0,	/* 0-invalid */
	0x0,			/* 1-2g */
	0x0,			/* 2-3g */
	0x0,			/* 3-wg */
	0x0,			/* 4-tg */
	0x0,			/* 5-lwg */
	0x0,			/* 6-ltg */
	0x0,			/* 7-sglte */
	0x33,			/* 8-ultg */
	0x39,			/* 9-ulwg */
	0x3B,			/* 10-ulwtg */
	0x3D,			/* 11-ulwcg */
	0x3F,			/* 12-ulwctg */
	0x13,			/* 13-ulttg */
	0x29,			/* 14-ulfwg */
	0x2D,			/* 15-ulfwcg */
	0x37,			/* 16-ulctg */
	0x17,			/* 17-ultctg */
	0x19,			/* 18-ultwg */
	0x1D,			/* 19-ultwcg */
	0x23,			/* 20-ulftg */
	0x27,			/* 21-ulfctg */
	0x79,			/* 22-unlwg */
	0x7B,			/* 23-unlwtg */
	0x7F,			/* 24-unlwctg */
	0x7D,			/* 25-unlwcg */
	0x57,			/* 26-unltctg */
};

int lge_get_wm_id(void)
{
	return ap_md_wm_id_map_array[lge_wm_id];
}


#ifdef CONFIG_LGE_HANDLE_PANIC
static int __init lge_crash_handler(char *status)
{
	if (!strcmp(status, "on")) {
		lge_set_crash_handle_status(1);
		pr_info("LGE crash handler Enabled\n");
	} else {
		lge_set_crash_handle_status(0);
		pr_info("LGE crash handler Disabled\n");
	}
	return 1;
}
__setup("lge.crash_handler=", lge_crash_handler);
#endif

#if defined(CONFIG_LGE_DSV_DUALIZE)
static int dsv_id;
static int __init lge_dsv_id_setup(char *dsv)
{
	sscanf(dsv, "%d", &dsv_id);
	pr_info("bootcmd androidboot.vendor.lge.dsv_id setup: %d\n", dsv_id);
	return 1;
}

int lge_get_dsv_id(void)
{
	return dsv_id;
}
__setup("androidboot.vendor.lge.dsv_id=", lge_dsv_id_setup);
#endif

#if defined(CONFIG_LGE_HIFI_HWINFO)
static int lge_hifi_hwinfo = 0;
static int __init lge_hifi_hwinfo_setup(char *hifi)
{
	if (!strcmp(hifi, "1")) {
		lge_hifi_hwinfo = 1;
		pr_info("LGE Hi-Fi Support\n");
	} else if (!strcmp(hifi, "0")) {
		lge_hifi_hwinfo = 0;
		pr_info("LGE Hi-Fi Not Support\n");
	} else {
		lge_hifi_hwinfo = 0;
		pr_info("Undefined Hi-Fi Support Info. Disable Hi-Fi\n");
	}
	return 1;
}
__setup("androidboot.vendor.lge.hifi=", lge_hifi_hwinfo_setup);

int lge_get_hifi_hwinfo(void)
{
	return lge_hifi_hwinfo;
}
#endif

#if defined(CONFIG_LGE_BROADCAST_TDMB) || defined(CONFIG_LGE_BROADCAST_ISDBT_JAPAN) || defined(CONFIG_LGE_BROADCAST_SBTVD_LATIN)
static int lge_tdmb_hwinfo = 0;
static int __init lge_tdmb_hwinfo_setup(char *dtv)
{
	if (!strcmp(dtv, "3")) {
		lge_tdmb_hwinfo = 3;
		pr_info("LGE TDMB[3] Support\n");
    }else if (!strcmp(dtv, "2")) {
		lge_tdmb_hwinfo = 2;
		pr_info("LGE TDMB[2] Support\n");
	}else if (!strcmp(dtv, "1")) {
		lge_tdmb_hwinfo = 1;
		pr_info("LGE TDMB[1] Support\n");
	} else if (!strcmp(dtv, "0")) {
		lge_tdmb_hwinfo = 0;
		pr_info("LGE TDMB  Not Support\n");
	} else {
		lge_tdmb_hwinfo = 0;
		pr_info("Undefined TDMB Support Info. Disable TDMB\n");
	}
	return 1;
}
__setup("androidboot.vendor.lge.dtv=", lge_tdmb_hwinfo_setup);

int lge_get_tdmb_hwinfo(void)
{
	return lge_tdmb_hwinfo;
}
#endif

#ifdef CONFIG_LGE_USE_SAR_CONTROLLER
static int lge_sar_hwinfo = 0;
static int __init lge_sar_hwinfo_setup(char *str)
{
	if (!strcmp(str, "1")) {
		lge_sar_hwinfo = 1;
		printk(KERN_INFO "[SAR] support\n");
	} else {
		lge_sar_hwinfo = 0;
		printk(KERN_INFO "[SAR] doesn't support\n");
	}

	return 1;
}
__setup("androidboot.vendor.lge.capsensor=", lge_sar_hwinfo_setup);

int lge_get_sar_hwinfo(void)
{
	return lge_sar_hwinfo;
}
#endif

#ifdef CONFIG_MTK_FINGERPRINT_SUPPORT
static int lge_fingerprint_hwinfo = 0;
static int __init lge_fingerprint_hwinfo_setup(char *str)
{
	if (!strcmp(str, "1")) {
		lge_fingerprint_hwinfo = 1;
		printk(KERN_INFO "[FINGERPRINT] support\n");
	} else {
		lge_fingerprint_hwinfo = 0;
		printk(KERN_INFO "[FINGERPRINT] doesn't support\n");
	}

	return 1;
}
__setup("androidboot.vendor.lge.fingerprint_sensor=", lge_fingerprint_hwinfo_setup);

int lge_get_fingerprint_hwinfo(void)
{
	return lge_fingerprint_hwinfo;
}
#endif

#ifdef CONFIG_LGE_PM
static enum {
	REGION_CAN,
	REGION_JPN,
	REGION_KOR,
	REGION_USA,

	REGION_COM,
} bootmode_region = REGION_COM;

const char* unified_bootmode_region(void) {
	switch (bootmode_region) {
	case REGION_CAN :	return "CAN";
	case REGION_JPN :	return "JPN";
	case REGION_KOR :	return "KOR";
	case REGION_USA :	return "USA";

	case REGION_COM :	return "COM";
	default :
		break;
	}

	return "INVALID";
}

static char pm_sku_carrier[30];
static int __init lge_pm_check_sku_carrier(char *value)
{
	if (value!= NULL) {
		strncpy(pm_sku_carrier, value, sizeof(pm_sku_carrier) - 1);
		pm_sku_carrier[sizeof(pm_sku_carrier) - 1] = '\0';
		if (!strcmp(pm_sku_carrier, "NA_GSM")
			|| !strcmp(pm_sku_carrier, "NA_CDMA")
			|| !strcmp(pm_sku_carrier, "NA_ALL")
			|| !strcmp(pm_sku_carrier, "ATT")
			|| !strcmp(pm_sku_carrier, "CRK")
			|| !strcmp(pm_sku_carrier, "TMUS")
			|| !strcmp(pm_sku_carrier, "TRF"))
			bootmode_region = REGION_USA;
		else if (!strcmp(pm_sku_carrier, "CAN"))
			bootmode_region = REGION_CAN;
		else if (!strcmp(pm_sku_carrier, "KR_ALL"))
			bootmode_region = REGION_KOR;
		else
			bootmode_region = REGION_COM;
	} else {
		pr_info("%s : hardware_sku is NULL\n", __func__);
	}

	pr_info("%s : pm_sku_carrier : %s, region : %s\n", __func__,
		pm_sku_carrier, unified_bootmode_region());
	return 1;
}
__setup("androidboot.vendor.lge.sku_carrier=", lge_pm_check_sku_carrier);
#endif
