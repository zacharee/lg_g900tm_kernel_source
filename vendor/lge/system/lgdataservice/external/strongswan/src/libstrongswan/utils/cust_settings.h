#ifndef __WO_CFG_H__
#define __WO_CFG_H__

#include <stdbool.h>

typedef enum ike_idi_format_t {
	IKE_IDI_RFC822_ADDR = 0,
	IKE_IDI_RFC822_ADDR_MAC,
	IKE_IDI_RFC822_ADDR_MAC_EAP_NO_MAC,
} ike_idi_format_t;

typedef union cust_value_t {
	int  integer;
	char *str;
	bool boolean;
} cust_value_t;

typedef struct cust_setting_t {
	char* system_property_key;
	cust_value_t default_value;
} cust_setting_t;

typedef enum cust_setting_type_t {
	CUST_PCSCF_IP4_VALUE,
	CUST_PCSCF_IP6_VALUE,
	FORCE_TSI_64,
	FORCE_TSI_FULL,
	FORCE_TSR_64,
	FORCE_TSR_FULL,
	REKEY_FULL,
	REKEY_TSI_FULL,
	USE_CFG_VIP,
	CUST_IMEI_CP,
	IS_CERTIFICATE_USED,
	IKE_PORT,
	IKE_PORT_NATT,
	IKE_RETRAN_TO,
	IKE_RETRAN_TRIES,
	IKE_RETRAN_BASE,
	IKE_IDI_FORMAT,
	IKE_HASHANDURL,
	IKE_KEEP_ALIVE_TIMER,
	HW_KEEP_ALIVE_STATE,
	SW_KEEP_ALIVE_STATE,
	ADDR_CHANGE_N_REAUTH,
	REMOVE_EAPAUTH,
	STATUS_CODE,
	LGP_DATA_DEBUG_ENABLE_PRIVACY_LOG,
	/* 2017-02-16 protocol-iwlan@lge.com LGP_DATA_IWLAN_REAUTH_DELETE_ONLY [START] */
	REAUTH_DELETE_ONLY,
	/* 2017-02-16 protocol-iwlan@lge.com LGP_DATA_IWLAN_REAUTH_DELETE_ONLY [END] */
	REKEY_DELAY,
	DSCP,
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [START] */
	PEER_MOBIKE,
	MOBIKE_IFACE,
	IWLAN_HO_SRCIFACES,
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [END] */

	/* LGP_DATA_IWLAN support_5gs [START] */
	EPDG_SUPPORT_5GS,
	/* LGP_DATA_IWLAN support_5gs [END] */
	SLOT1_CUST_PCSCF_IP4_VALUE,
	SLOT1_CUST_PCSCF_IP6_VALUE,
	SLOT1_FORCE_TSI_64,
	SLOT1_FORCE_TSI_FULL,
	SLOT1_FORCE_TSR_64,
	SLOT1_FORCE_TSR_FULL,
	SLOT1_REKEY_FULL,
	SLOT1_REKEY_TSI_FULL,
	SLOT1_USE_CFG_VIP,
	SLOT1_CUST_IMEI_CP,
	SLOT1_IS_CERTIFICATE_USED,
	SLOT1_IKE_PORT,
	SLOT1_IKE_PORT_NATT,
	SLOT1_IKE_RETRAN_TO,
	SLOT1_IKE_RETRAN_TRIES,
	SLOT1_IKE_RETRAN_BASE,
	SLOT1_IKE_IDI_FORMAT,
	SLOT1_IKE_HASHANDURL,
	SLOT1_IKE_KEEP_ALIVE_TIMER,
	SLOT1_HW_KEEP_ALIVE_STATE,
	SLOT1_SW_KEEP_ALIVE_STATE,
	SLOT1_ADDR_CHANGE_N_REAUTH,
	SLOT1_REMOVE_EAPAUTH,
	SLOT1_STATUS_CODE,
	SLOT1_LGP_DATA_DEBUG_ENABLE_PRIVACY_LOG,
	SLOT1_REAUTH_DELETE_ONLY,
	SLOT1_REKEY_DELAY,
	SLOT1_DSCP,
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [START] */
	SLOT1_PEER_MOBIKE,
	SLOT1_MOBIKE_IFACE,
	SLOT1_IWLAN_HO_SRCIFACES,
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [END] */
	/* LGP_DATA_IWLAN support_5gs [START] */
	SLOT1_EPDG_SUPPORT_5GS,
	/* LGP_DATA_IWLAN support_5gs [END] */

	SETTING_END
} cust_setting_type_t;

static cust_setting_t cust_settings[SETTING_END] = {
	[CUST_PCSCF_IP4_VALUE]                    = {"net.wo.cust_pcscf_4",             {.integer = 20}},
	[CUST_PCSCF_IP6_VALUE]                    = {"net.wo.cust_pcscf_6",             {.integer = 21}},
	[FORCE_TSI_64]                            = {"net.wo.force_tsi_64",             {.boolean = true}},
	[FORCE_TSI_FULL]                          = {"net.wo.force_tsi_full",           {.boolean = true}},
	[FORCE_TSR_64]                            = {"net.wo.force_tsr_64",             {.boolean = true}},
	[FORCE_TSR_FULL]                          = {"net.wo.force_tsr_full",           {.boolean = false}},
	[REKEY_FULL]                              = {"net.wo.rekey_full",               {.boolean = false}},
	[REKEY_TSI_FULL]                          = {"net.wo.rekey_tsi_full",           {.boolean = false}},
	[USE_CFG_VIP]                             = {"net.wo.use_cfg_vip",              {.boolean = false}},
	[CUST_IMEI_CP]                            = {"net.wo.cust_imei_cp",             {.integer = 16391}},
	[IS_CERTIFICATE_USED]                     = {"net.wo.cert_used",                {.boolean = true}},
	[IKE_PORT]                                = {"net.wo.port",                     {.integer = -1}},
	[IKE_PORT_NATT]                           = {"net.wo.port_natt",                {.integer = -1}},
	[IKE_RETRAN_TO]                           = {"net.wo.retrans_to",               {.str = NULL}},
	[IKE_RETRAN_TRIES]                        = {"net.wo.retrans_tries",            {.str = NULL}},
	[IKE_RETRAN_BASE]                         = {"net.wo.retrans_base",             {.str = NULL}},
	[IKE_IDI_FORMAT]                          = {"net.wo.IDi",                      {.integer = 0}},
	[IKE_HASHANDURL]                          = {"net.wo.urlcert",                  {.boolean = false}},
	[IKE_KEEP_ALIVE_TIMER]                    = {"net.wo.keep_timer",               {.integer = -1}},
	[HW_KEEP_ALIVE_STATE]                     = {"net.wo.hw_keepalive_st",          {.integer = 0}},
	[SW_KEEP_ALIVE_STATE]                     = {"net.wo.sw_keepalive_st",          {.integer = 1}},
	[ADDR_CHANGE_N_REAUTH]                    = {"net.wo.reauth_addr",              {.boolean = false}},
	[REMOVE_EAPAUTH]                          = {"net.wo.remove_eapauth",           {.boolean = false}},
	[STATUS_CODE]                             = {"net.wo.statuscode",               {.integer = 0}},
	[LGP_DATA_DEBUG_ENABLE_PRIVACY_LOG]       = {"persist.product.lge.data.service.privacy.enable",  {.boolean = 1}},
	/* 2017-02-16 protocol-iwlan@lge.com LGP_DATA_IWLAN_REAUTH_DELETE_ONLY [START] */
	[REAUTH_DELETE_ONLY]                      = {"net.wo.reauth.delete",            {.boolean = false}},
	/* 2017-02-16 protocol-iwlan@lge.com LGP_DATA_IWLAN_REAUTH_DELETE_ONLY [END] */
	[REKEY_DELAY]                             = {"net.wo.rekey_delay",              {.boolean = false}},
	[DSCP]                                    = {"net.wo.dscp",                     {.integer = 0}},
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [START] */
	[PEER_MOBIKE]                             = {"net.wo.peer_mobike",              {.boolean = false}},
	[MOBIKE_IFACE]                            = {"net.wo.mobike_iface",             {.str = NULL}},
	[IWLAN_HO_SRCIFACES]                      = {"net.wo.ho.srcifaces",             {.str = NULL}},
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [END] */
	/* LGP_DATA_IWLAN support_5gs [START] */
	[EPDG_SUPPORT_5GS]			={"net.wo.epdg_support_5gs",             {.integer = 0}},
	/* LGP_DATA_IWLAN support_5gs [END] */

	[SLOT1_CUST_PCSCF_IP4_VALUE]              = {"net.wo.1.cust_pcscf_4",           {.integer = 20}},
	[SLOT1_CUST_PCSCF_IP6_VALUE]              = {"net.wo.1.cust_pcscf_6",           {.integer = 21}},
	[SLOT1_FORCE_TSI_64]                      = {"net.wo.1.force_tsi_64",           {.boolean = true}},
	[SLOT1_FORCE_TSI_FULL]                    = {"net.wo.1.force_tsi_full",         {.boolean = true}},
	[SLOT1_FORCE_TSR_64]                      = {"net.wo.1.force_tsr_64",           {.boolean = true}},
	[SLOT1_FORCE_TSR_FULL]                    = {"net.wo.1.force_tsr_full",         {.boolean = false}},
	[SLOT1_REKEY_FULL]                        = {"net.wo.1.rekey_full",             {.boolean = false}},
	[SLOT1_REKEY_TSI_FULL]                    = {"net.wo.1.rekey_tsi_full",         {.boolean = false}},
	[SLOT1_USE_CFG_VIP]                       = {"net.wo.1.use_cfg_vip",            {.boolean = false}},
	[SLOT1_CUST_IMEI_CP]                      = {"net.wo.1.cust_imei_cp",           {.integer = 16391}},
	[SLOT1_IS_CERTIFICATE_USED]               = {"net.wo.1.cert_used",              {.boolean = true}},
	[SLOT1_IKE_PORT]                          = {"net.wo.1.port",                   {.integer = -1}},
	[SLOT1_IKE_PORT_NATT]                     = {"net.wo.1.port_natt",              {.integer = -1}},
	[SLOT1_IKE_RETRAN_TO]                     = {"net.wo.1.retrans_to",             {.str = NULL}},
	[SLOT1_IKE_RETRAN_TRIES]                  = {"net.wo.1.retrans_tries",          {.str = NULL}},
	[SLOT1_IKE_RETRAN_BASE]                   = {"net.wo.1.retrans_base",           {.str = NULL}},
	[SLOT1_IKE_IDI_FORMAT]                    = {"net.wo.1.IDi",                    {.integer = 0}},
	[SLOT1_IKE_HASHANDURL]                    = {"net.wo.1.urlcert",                {.boolean = false}},
	[SLOT1_IKE_KEEP_ALIVE_TIMER]              = {"net.wo.1.keep_timer",             {.integer = -1}},
	[SLOT1_HW_KEEP_ALIVE_STATE]               = {"net.wo.1.hw_keepalive_st",        {.integer = 0}},
	[SLOT1_SW_KEEP_ALIVE_STATE]               = {"net.wo.1.sw_keepalive_st",        {.integer = 1}},
	[SLOT1_ADDR_CHANGE_N_REAUTH]              = {"net.wo.1.reauth_addr",            {.boolean = false}},
	[SLOT1_REMOVE_EAPAUTH]                    = {"net.wo.1.remove_eapauth",         {.boolean = false}},
	[SLOT1_STATUS_CODE]                       = {"net.wo.1.statuscode",             {.integer = 0}},
	[SLOT1_LGP_DATA_DEBUG_ENABLE_PRIVACY_LOG] = {"persist.product.lge.data.service.privacy.enable",  {.boolean = 1}},
	/* 2017-02-16 protocol-iwlan@lge.com LGP_DATA_IWLAN_REAUTH_DELETE_ONLY [START] */
	[SLOT1_REAUTH_DELETE_ONLY]                = {"net.wo.1.reauth.delete",          {.boolean = false}},
	/* 2017-02-16 protocol-iwlan@lge.com LGP_DATA_IWLAN_REAUTH_DELETE_ONLY [END] */
	[SLOT1_REKEY_DELAY]                       = {"net.wo.1.rekey_delay",            {.boolean = false}},
	[SLOT1_DSCP]                              = {"net.wo.1.dscp",                   {.integer = 0}},
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [START] */
	[SLOT1_PEER_MOBIKE]                       = {"net.wo.1.peer_mobike",            {.boolean = false}},
	[SLOT1_MOBIKE_IFACE]                      = {"net.wo.1.mobike_iface",           {.str = NULL}},
	[SLOT1_IWLAN_HO_SRCIFACES]                = {"net.wo.1.ho.srcifaces",           {.str = NULL}},
	/* 2019-03-13 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [END] */
	/* LGP_DATA_IWLAN support_5gs [START] */
	[SLOT1_EPDG_SUPPORT_5GS]			={"net.wo.1.epdg_support_5gs",             {.integer = 0}},
	/* LGP_DATA_IWLAN support_5gs [END] */

};

int get_cust_setting(cust_setting_type_t type, char *value);
bool get_cust_setting_bool(cust_setting_type_t type);
int get_cust_setting_int(cust_setting_type_t type);
int set_cust_setting(cust_setting_type_t type, char *value);
int set_cust_setting_bool(cust_setting_type_t type, bool value);
int set_cust_setting_int(cust_setting_type_t type, int value);

int get_slotid(const char* conn);
int get_cust_setting_by_slotid(int slotid, cust_setting_type_t type, char *value);
bool get_cust_setting_bool_by_slotid(int slotid, cust_setting_type_t type);
int get_cust_setting_int_by_slotid(int slotid, cust_setting_type_t type);
int set_cust_setting_by_slotid(int slotid, cust_setting_type_t type, char *value);
int set_cust_setting_bool_by_slotid(int slotid, cust_setting_type_t type, bool value);
int set_cust_setting_int_by_slotid(int slotid, cust_setting_type_t type, int value);
#endif
