#ifdef __ANDROID__
#include <cutils/properties.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cust_settings.h"
#include "settings.h"
#include "settings.h"
#include "debug.h"

#include <libpatchcodeid.h>

#define PERSIST_PROPERTY_PREFIX "persist."


static int strcicmp(char const *a, char const *b)
{
	for (;; a++, b++)
	{
		int d = tolower(*a) - tolower(*b);

		if (d != 0 || !*a)
		{
			return d;
		}
	}
}

#define strcieq(s, t) (strcicmp(s, t) == 0)

static inline bool str_to_bool(const char* v)
{
	if (!v)
	{
		return false;
	}

	if (strcieq(v, "true") || strcieq(v, "1") || strcieq(v, "yes") 
							|| strcieq(v, "y") || strcieq(v, "on"))
	{
		return true;
	}

	return false;
}

static inline const char* get_key(cust_setting_type_t type)
{
	return cust_settings[type].system_property_key;
}

static inline cust_value_t get_default(cust_setting_type_t type)
{
	return cust_settings[type].default_value;
}

int get_cust_setting(cust_setting_type_t type, char* value)
{
	const char *key = get_key(type);
	const char *default_value = get_default(type).str;
	int len = 0;

#ifdef __ANDROID__
	if (key)
	{
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);
		if ((len = __system_property_get(persist_property_name, value)))
		{
			return len;
		}
		len = property_get(key, value, default_value);
	}
	else if (default_value)
	{
#endif
		len = strlen(default_value);
		sprintf(value, default_value);
#ifdef __ANDROID__
	}
#endif

	return len;
}

bool get_cust_setting_bool(cust_setting_type_t type)
{
	const char *key = get_key(type);
	bool ret = get_default(type).boolean;

#ifdef __ANDROID__
	if (key)
	{
		char value[PROP_VALUE_MAX];
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);
		if (__system_property_get(persist_property_name, value))
			ret = str_to_bool(value);
		else
			ret = property_get_bool(key, ret);
	}
#endif

	return ret;
}

int get_cust_setting_int(cust_setting_type_t type)
{
	const char *key = get_key(type);
	int ret = get_default(type).integer;

#ifdef __ANDROID__
	if (key)
	{
		char value[PROP_VALUE_MAX];
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);
		if (__system_property_get(persist_property_name, value))
		{
			ret = atoi(value);
		}
		else if (__system_property_get(key, value))
		{
			ret = atoi(value);
		}
	}
#endif

	return ret;
}

int set_cust_setting(cust_setting_type_t type, char* value)
{
	const char *key = get_key(type);
	int ret = -1;

#ifdef __ANDROID__
	if (key)
	{
		char persist_property_name[PROP_NAME_MAX];
		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);

		ret = __system_property_set(persist_property_name, value);
	}
#endif

	return ret;
}

int set_cust_setting_bool(cust_setting_type_t type, bool value)
{
	const char *key = get_key(type);
	int ret = -1;

#ifdef __ANDROID__
	if (key)
	{
		char persist_property_name[PROP_NAME_MAX];

		snprintf(persist_property_name, PROP_NAME_MAX, "%s%s",
									PERSIST_PROPERTY_PREFIX, key);

		if (value)
		{
			ret = __system_property_set(persist_property_name, "1");
		}
		else
		{
			ret = __system_property_set(persist_property_name, "0");
		}
	}
#endif

	return ret;
}

int set_cust_setting_int(cust_setting_type_t type, int value)
{
	const char *key = get_key(type);
	int ret = -1;

#ifdef __ANDROID__
	if (key)
	{
		char p_value[PROP_VALUE_MAX];

		snprintf(p_value, PROP_VALUE_MAX, "%d", value);
		ret = __system_property_set(key, p_value);
	}
#endif

	return ret;
}


static cust_setting_type_t convert_cust_setting_to_slot1(cust_setting_type_t type)
{
	switch(type) {
		case CUST_PCSCF_IP4_VALUE: return SLOT1_CUST_PCSCF_IP4_VALUE;
		case CUST_PCSCF_IP6_VALUE: return SLOT1_CUST_PCSCF_IP6_VALUE;
		case FORCE_TSI_64: return SLOT1_FORCE_TSI_64;
		case FORCE_TSI_FULL: return SLOT1_FORCE_TSI_FULL;
		case FORCE_TSR_64: return SLOT1_FORCE_TSR_64;
		case FORCE_TSR_FULL: return SLOT1_FORCE_TSR_FULL;
		case REKEY_FULL: return SLOT1_REKEY_FULL;
		case REKEY_TSI_FULL: return SLOT1_REKEY_TSI_FULL;
		case USE_CFG_VIP: return SLOT1_USE_CFG_VIP;
		case CUST_IMEI_CP: return SLOT1_CUST_IMEI_CP;
		case IS_CERTIFICATE_USED: return SLOT1_IS_CERTIFICATE_USED;
		case IKE_PORT: return SLOT1_IKE_PORT;
		case IKE_PORT_NATT: return SLOT1_IKE_PORT_NATT;
		case IKE_RETRAN_TO: return SLOT1_IKE_RETRAN_TO;
		case IKE_RETRAN_TRIES: return SLOT1_IKE_RETRAN_TRIES;
		case IKE_RETRAN_BASE: return SLOT1_IKE_RETRAN_BASE;
		case IKE_IDI_FORMAT: return SLOT1_IKE_IDI_FORMAT;
		case IKE_HASHANDURL: return SLOT1_IKE_HASHANDURL;
		case IKE_KEEP_ALIVE_TIMER: return SLOT1_IKE_KEEP_ALIVE_TIMER;
		case HW_KEEP_ALIVE_STATE: return SLOT1_HW_KEEP_ALIVE_STATE;
		case SW_KEEP_ALIVE_STATE: return SLOT1_SW_KEEP_ALIVE_STATE;
		case ADDR_CHANGE_N_REAUTH: return SLOT1_ADDR_CHANGE_N_REAUTH;
		case REMOVE_EAPAUTH: return SLOT1_REMOVE_EAPAUTH;
		case STATUS_CODE: return SLOT1_STATUS_CODE;
		case LGP_DATA_DEBUG_ENABLE_PRIVACY_LOG:
                patch_code_id("LPCP-2249@n@c@libstrongswan@cust_settings.c@1");
                return SLOT1_LGP_DATA_DEBUG_ENABLE_PRIVACY_LOG;
		case REAUTH_DELETE_ONLY: return SLOT1_REAUTH_DELETE_ONLY;
		case REKEY_DELAY: return SLOT1_REKEY_DELAY;
		case DSCP: return SLOT1_DSCP;
		/* 2019-03-15 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [START] */
		case PEER_MOBIKE: return SLOT1_PEER_MOBIKE;
		case MOBIKE_IFACE: return SLOT1_MOBIKE_IFACE;
                case IWLAN_HO_SRCIFACES : return SLOT1_IWLAN_HO_SRCIFACES;
		/* 2019-03-15 leela.mohan@lge.com LGP_DATA_IWLAN MOBIKE [END] */
		/* LGP_DATA_IWLAN support_5gs [START] */
		case EPDG_SUPPORT_5GS: return SLOT1_EPDG_SUPPORT_5GS;
		/* LGP_DATA_IWLAN support_5gs [END] */

	}
	return SETTING_END;
}

int get_slotid(const char* conn)
{
	int len = strlen(conn);
	if (len > 4 && conn[4] == '1') {
		return 1;
	}
	return 0;
}

int get_cust_setting_by_slotid(int slotid, cust_setting_type_t type, char *value)
{
	cust_setting_type_t type_slot1 = convert_cust_setting_to_slot1(type);
	if (slotid != 0 && type_slot1 != SETTING_END) {
		return get_cust_setting(type_slot1, value);
	}
	return get_cust_setting(type, value);
}

bool get_cust_setting_bool_by_slotid(int slotid, cust_setting_type_t type)
{
	cust_setting_type_t type_slot1 = convert_cust_setting_to_slot1(type);
	if (slotid != 0 && type_slot1 != SETTING_END) {
		return get_cust_setting_bool(type_slot1);
	}
	return get_cust_setting_bool(type);
}

int get_cust_setting_int_by_slotid(int slotid, cust_setting_type_t type)
{
	cust_setting_type_t type_slot1 = convert_cust_setting_to_slot1(type);
	if (slotid != 0 && type_slot1 != SETTING_END) {
		return get_cust_setting_int(type_slot1);
	}
	return get_cust_setting_int(type);
}

int set_cust_setting_by_slotid(int slotid, cust_setting_type_t type, char *value)
{
	cust_setting_type_t type_slot1 = convert_cust_setting_to_slot1(type);
	if (slotid != 0 && type_slot1 != SETTING_END) {
		return set_cust_setting(type_slot1, value);
	}
	return set_cust_setting(type, value);
}

int set_cust_setting_bool_by_slotid(int slotid, cust_setting_type_t type, bool value)
{
	cust_setting_type_t type_slot1 = convert_cust_setting_to_slot1(type);
	if (slotid != 0 && type_slot1 != SETTING_END) {
		return set_cust_setting_bool(type_slot1, value);
	}
	return set_cust_setting_bool(type, value);
}

int set_cust_setting_int_by_slotid(int slotid, cust_setting_type_t type, int value)
{
	cust_setting_type_t type_slot1 = convert_cust_setting_to_slot1(type);
	if (slotid != 0 && type_slot1 != SETTING_END) {
		return set_cust_setting_int(type_slot1, value);
	}
	return set_cust_setting_int(type, value);
}
