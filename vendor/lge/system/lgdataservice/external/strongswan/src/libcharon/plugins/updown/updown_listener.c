/*
 * Copyright (C) 2013 Tobias Brunner
 * Copyright (C) 2008 Martin Willi
 * Copyright (C) 2016 Andreas Steffen
 * HSR Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>

#include "updown_listener.h"

#include <utils/process.h>
#include <daemon.h>
#include <config/child_cfg.h>
#ifdef ANDROID
#include <network/wod_channel.h>
#include <cutils/properties.h>
#include <cutils/android_filesystem_config.h> /* for AID_VPN */
#endif

typedef struct private_updown_listener_t private_updown_listener_t;

/**
 * Private data of an updown_listener_t object.
 */
struct private_updown_listener_t {

	/**
	 * Public updown_listener_t interface.
	 */
	updown_listener_t public;

	/**
	 * List of cached interface names
	 */
	linked_list_t *iface_cache;

	/**
	 * DNS attribute handler
	 */
	updown_handler_t *handler;
#ifdef ANDROID
	conn_info_prop prop;
#endif
};

typedef struct cache_entry_t cache_entry_t;

/**
 * Cache line in the interface name cache.
 */
struct cache_entry_t {
	/** requid of the CHILD_SA */
	uint32_t reqid;
	/** cached interface name */
	char *iface;
};

/**
 * Insert an interface name to the cache
 */
static void cache_iface(private_updown_listener_t *this, uint32_t reqid,
						char *iface)
{
	cache_entry_t *entry = malloc_thing(cache_entry_t);

	entry->reqid = reqid;
	entry->iface = strdup(iface);

	this->iface_cache->insert_first(this->iface_cache, entry);
}

/**
 * Remove a cached interface name and return it.
 */
static char* uncache_iface(private_updown_listener_t *this, uint32_t reqid)
{
	enumerator_t *enumerator;
	cache_entry_t *entry;
	char *iface = NULL;

	enumerator = this->iface_cache->create_enumerator(this->iface_cache);
	while (enumerator->enumerate(enumerator, &entry))
	{
		if (entry->reqid == reqid)
		{
			this->iface_cache->remove_at(this->iface_cache, enumerator);
			iface = entry->iface;
			free(entry);
			break;
		}
	}
	enumerator->destroy(enumerator);
	return iface;
}

/**
 * Allocate and push a format string to the environment
 */
static bool push_env(char *envp[], u_int count, char *fmt, ...)
{
	int i = 0;
	char *str;
	va_list args;

	while (envp[i])
	{
		if (++i + 1 >= count)
		{
			return FALSE;
		}
	}
	va_start(args, fmt);
	if (vasprintf(&str, fmt, args) >= 0)
	{
		envp[i] = str;
	}
	va_end(args);
	return envp[i] != NULL;
}

/**
 * Free all allocated environment strings
 */
static void free_env(char *envp[])
{
	int i;

	for (i = 0; envp[i]; i++)
	{
		free(envp[i]);
	}
}

/**
 * Push variables for handled DNS attributes
 */
static void push_dns_env(private_updown_listener_t *this, ike_sa_t *ike_sa,
						 char *envp[], u_int count)
{
	enumerator_t *enumerator;
	host_t *host;
	int v4 = 0, v6 = 0;

	if (this->handler)
	{


#ifdef ANDROID
        memset(this->prop.dns, 0, sizeof(this->prop.dns));
        memset(this->prop.dns6, 0, sizeof(this->prop.dns6));

        enumerator = ike_sa->create_dns_enumerator(ike_sa);
#else
        enumerator = this->handler->create_dns_enumerator(this->handler,
												ike_sa->get_unique_id(ike_sa));
#endif

        while (enumerator->enumerate(enumerator, &host))
        {
            switch (host->get_family(host))
            {
                case AF_INET:
#ifdef ANDROID
                    if (v4 < MAX_DNS_NUM) {
                        snprintf(this->prop.dns[v4], IP_ADDR_LEN, "%H", host);
                    }
#endif
					push_env(envp, count, "PLUTO_DNS4_%d=%H", ++v4, host);
					break;
                case AF_INET6:
#ifdef ANDROID
                    if (v6 < MAX_DNS_NUM) {
                        snprintf(this->prop.dns6[v6], IP6_ADDR_LEN, "%H", host);
                    }
#endif
					push_env(envp, count, "PLUTO_DNS6_%d=%H", ++v6, host);
					break;
				default:
					continue;
			}
		}
		enumerator->destroy(enumerator);
	}
}

/**
 * Push variables for local/remote virtual IPs
 */
static void push_vip_env(private_updown_listener_t *this, ike_sa_t *ike_sa,
						 char *envp[], u_int count, bool local)
{
	enumerator_t *enumerator;
	host_t *host;
	int v4 = 0, v6 = 0;
	bool first = TRUE;

#ifdef ANDROID
    if (local) {
        memset(this->prop.ip, 0, sizeof(this->prop.ip));
        memset(this->prop.ip6, 0, sizeof(this->prop.ip6));
	}
#endif
	enumerator = ike_sa->create_virtual_ip_enumerator(ike_sa, local);
	while (enumerator->enumerate(enumerator, &host))
	{
		if (first)
		{	/* legacy variable for first VIP */
			first = FALSE;
			push_env(envp, count, "PLUTO_%s_SOURCEIP=%H",
					 local ? "MY" : "PEER", host);
		}
		switch (host->get_family(host))
		{
			case AF_INET:
#ifdef ANDROID
				if ( local && v4 < MAX_VIP_NUM) {
					snprintf(this->prop.ip[v4], IP_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_SOURCEIP4_%d=%H",
						 local ? "MY" : "PEER", ++v4, host);
				break;
			case AF_INET6:
#ifdef ANDROID
				if ( local && v6 < MAX_VIP_NUM) {
					snprintf(this->prop.ip6[v6], IP6_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_SOURCEIP6_%d=%H",
						 local ? "MY" : "PEER", ++v6, host);
				break;
			default:
				continue;
		}
	}
	enumerator->destroy(enumerator);
}

/**
 * Create variables for local virtual IPs
 */
static void push_pcscf_env(private_updown_listener_t *this, ike_sa_t *ike_sa,
						 char *envp[], u_int count)
{
	enumerator_t *enumerator;
	host_t *host;
	int v4 = 0, v6 = 0;
	bool first = TRUE;

#ifdef ANDROID
	memset(this->prop.pcscf, 0, sizeof(this->prop.pcscf));
	memset(this->prop.pcscf6, 0, sizeof(this->prop.pcscf6));
#endif
	enumerator = ike_sa->create_pcscf_enumerator(ike_sa);
	while (enumerator->enumerate(enumerator, &host))
	{

		if (first)
		{	/* legacy variable for first VIP */
			first = FALSE;
			push_env(envp, count, "PLUTO_%s_PCSCF=%H",
					 "MY", host);
		}

		switch (host->get_family(host))
		{
			case AF_INET:
#ifdef ANDROID
				if (v4 < MAX_PCSCF_NUM) {
					snprintf(this->prop.pcscf[v4], IP_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_PCSCF4_%d=%H",
						 "MY", ++v4, host);
				break;

			case AF_INET6:
#ifdef ANDROID
				if (v6 < MAX_PCSCF_NUM) {
					snprintf(this->prop.pcscf6[v6], IP6_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_PCSCF6_%d=%H",
						 "MY", ++v6, host);
				break;

			default:
				continue;
		}
	}
	enumerator->destroy(enumerator);
}

/**
 * Create variables for local netmask
 */
static void push_netmask_env(private_updown_listener_t *this, ike_sa_t *ike_sa,
						 char *envp[], u_int count)
{
	enumerator_t *enumerator;
	host_t *host;
	int v4 = 0, v6 = 0;
	bool first = TRUE;

#ifdef ANDROID
	memset(this->prop.netmask, 0, sizeof(this->prop.netmask));
	memset(this->prop.netmask6, 0, sizeof(this->prop.netmask6));
#endif
	enumerator = ike_sa->create_intnetmask_enumerator(ike_sa);
	while (enumerator->enumerate(enumerator, &host))
	{
		if (first)
		{	/* legacy variable for first VIP */
			first = FALSE;
			push_env(envp, count, "PLUTO_%s_NETMASK=%H",
					 "MY", host);
		}

		switch (host->get_family(host))
		{
			case AF_INET:
#ifdef ANDROID
				if (v4 < MAX_NETMASK_NUM) {
					snprintf(this->prop.netmask[v4], IP_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_NETMASK4_%d=%H",
						 "MY", ++v4, host);
				break;

			case AF_INET6:
#ifdef ANDROID
				if (v6 < MAX_NETMASK_NUM) {
					snprintf(this->prop.netmask6[v6], IP6_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_NETMASK6_%d=%H",
						 "MY", ++v6, host);
				break;
			default:
				continue;
		}
	}
	enumerator->destroy(enumerator);
}

/**
 * Create variables for local subnet
 */
static void push_subnet_env(private_updown_listener_t *this, ike_sa_t *ike_sa,
						 char *envp[], u_int count)
{
	enumerator_t *enumerator;
	host_t *host;
	int v4 = 0, v6 = 0;
	bool first = TRUE;

#ifdef ANDROID
	memset(this->prop.subnet, 0, sizeof(this->prop.subnet));
	memset(this->prop.subnet6, 0, sizeof(this->prop.subnet6));
#endif
	enumerator = ike_sa->create_intsubnet_enumerator(ike_sa);
	while (enumerator->enumerate(enumerator, &host))
	{
		if (first)
		{	/* legacy variable for first VIP */
			first = FALSE;
			push_env(envp, count, "PLUTO_%s_SUBNET=%H",
					 "MY", host);
		}

		switch (host->get_family(host))
		{
			case AF_INET:
#ifdef ANDROID
				if (v4 < MAX_SUBNET_NUM) {
					snprintf(this->prop.subnet[v4], IP_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_SUBNET4_%d=%H",
						 "MY", ++v4, host);
				break;

			case AF_INET6:
#ifdef ANDROID
				if (v6 < MAX_SUBNET_NUM) {
					snprintf(this->prop.subnet6[v6], IP6_ADDR_LEN, "%H", host);
				}
#endif
				push_env(envp, count, "PLUTO_%s_SUBNET6_%d=%H",
						 "MY", ++v6, host);

			default:
				continue;
		}
	}
	enumerator->destroy(enumerator);
}

#define	PORT_BUF_LEN	12
/**
 * Determine proper values for port env variable
 */
static char* get_port(traffic_selector_t *me, traffic_selector_t *other,
					  char *port_buf, bool local)
{
	uint16_t port, to, from;

	switch (max(me->get_protocol(me), other->get_protocol(other)))
	{
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
		{
			port = max(me->get_from_port(me), other->get_from_port(other));
			snprintf(port_buf, PORT_BUF_LEN, "%u",
					 local ? traffic_selector_icmp_type(port)
						   : traffic_selector_icmp_code(port));
			return port_buf;
		}
	}
	if (local)
	{
		from = me->get_from_port(me);
		to   = me->get_to_port(me);
	}
	else
	{
		from = other->get_from_port(other);
		to   = other->get_to_port(other);
	}
	if (from == to || (from == 0 && to == 65535))
	{
		snprintf(port_buf, PORT_BUF_LEN, "%u", from);
	}
	else
	{
		snprintf(port_buf, PORT_BUF_LEN, "%u:%u", from, to);
	}
	return port_buf;
}

#ifdef ANDROID
void update_property(conn_info_prop *prop, char *prefix, char *conn, char *ifname, int up, int is_ipv6, uint8_t psid, uint8_t* snssai_info, uint8_t* plmn_id)
{
	char key[PROPERTY_KEY_MAX], value[PROPERTY_VALUE_MAX], key_prefix[PROPERTY_KEY_MAX];
	int i,ret = 0;

        DBG1(DBG_CHD, "prefix: %s, conn: %s, if: %s, up:%d, ipv6: %d, psid: %d", prefix, conn, ifname, up, is_ipv6, psid);
	if (snprintf(key_prefix, PROPERTY_KEY_MAX-15, "%s.%s", prefix, ifname) >= PROPERTY_KEY_MAX-15) {
		//	net.wo.apn_xxx.netmask6_1, 10 + 1
		DBG1(DBG_CHD, "key length is too larger than %d, '%s' and '%s'", PROPERTY_KEY_MAX-15, prefix, conn);
//		return ;
	}

	if (up) {
		sprintf(key, "%s.apn", key_prefix);
		if (property_set(key, conn) != 0) {
			DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, ifname);
			return ;
		}
                //support_5gs [s]
                //if (get_cust_setting_int_by_slotid(slotid, EPDG_SUPPORT_5GS) == 1) {
                memset(key, 0, PROPERTY_KEY_MAX);
                memset(value,0,PROPERTY_VALUE_MAX);
                if (psid > 0 && psid < 256) {
                    snprintf(key, sizeof(key), "%s.pduinfo", key_prefix);
                //psid,snssailen,sst,sd0,sd1,sd2,msst,msd0,msd1,msd2,plmnidexist,plmnid0,plmnid1,plmnid2
                    snprintf(value, sizeof(value),"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",psid, snssai_info[0],snssai_info[1],snssai_info[2],
                                                                         snssai_info[3], snssai_info[4], snssai_info[5],
                                                                         snssai_info[6], snssai_info[7], snssai_info[8],
                                                                         plmn_id[0], plmn_id[1], plmn_id[2], plmn_id[3]);
                    if (property_set(key,value) != 0) {
                        DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key,value);
                    } else {
                        DBG1(DBG_CHD, "property_set success with ('%s', '%s')", key,value);
                    }
                } else {
                    snprintf(key, sizeof(key), "%s.pduinfo", key_prefix);
                    if (property_set(key,"na")) {
                        DBG1(DBG_CHD, "property_set fail with ('%s', '%d')", key,psid);
                    }
                }
                memset(key, 0, PROPERTY_KEY_MAX);
                memset(value,0,PROPERTY_VALUE_MAX);
                //}
                //support_5gs [e]
		if (is_ipv6) {
			//for (i = 0; prop->dns6[i][0] && i < MAX_DNS_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_DNS_NUM && prop->dns6[i][0]; i++) {
				sprintf(key, "%s.dns6_%d", key_prefix, i+1);
				if (property_set(key, prop->dns6[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->dns6[i]);
					return ;
				}
			}

			//for (i = 0; prop->pcscf6[i][0] && i < MAX_PCSCF_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_PCSCF_NUM && prop->pcscf6[i][0]; i++) {
				sprintf(key, "%s.pcscf6_%d", key_prefix, i+1);
				if (property_set(key, prop->pcscf6[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->pcscf6[i]);
					return ;
				}
			}

			//for (i = 0; prop->netmask6[i][0] && i < MAX_NETMASK_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_NETMASK_NUM && prop->netmask6[i][0]; i++) {
				sprintf(key, "%s.netmask6_%d", key_prefix, i+1);
				if (property_set(key, prop->netmask6[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->netmask6[i]);
					return ;
				}
			}

			//for (i = 0; prop->subnet6[i][0] && i < MAX_SUBNET_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_SUBNET_NUM && prop->subnet6[i][0]; i++) {
				sprintf(key, "%s.subnet6_%d", key_prefix, i+1);
				if (property_set(key, prop->subnet6[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->subnet6[i]);
					return ;
				}
			}
			//for (i = 0; prop->ip6[i][0] && i < MAX_VIP_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_VIP_NUM && prop->ip6[i][0]; i++) {
				sprintf(key, "%s.ip6_%d", key_prefix, i+1);
				if (property_set(key, prop->ip6[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->ip6[i]);
					return ;
				}
			}
		} else {
			//for (i = 0; prop->dns[i][0] && i < MAX_DNS_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_DNS_NUM && prop->dns[i][0]; i++) {
				sprintf(key, "%s.dns_%d", key_prefix, i+1);
				if (property_set(key, prop->dns[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->dns[i]);
					return ;
				}
			}

			//for (i = 0; prop->pcscf[i][0] && i < MAX_PCSCF_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_PCSCF_NUM && prop->pcscf[i][0]; i++) {
				sprintf(key, "%s.pcscf_%d", key_prefix, i+1);
				if (property_set(key, prop->pcscf[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s'), ret=%d", key, prop->pcscf[i]);
					return ;
				}
			}

			//for (i = 0; prop->netmask[i][0] && i < MAX_NETMASK_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_NETMASK_NUM && prop->netmask[i][0]; i++) {
				sprintf(key, "%s.netmask_%d", key_prefix, i+1);
				if (property_set(key, prop->netmask[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->netmask[i]);
					return ;
				}
			}

			//for (i = 0; prop->subnet[i][0] && i < MAX_SUBNET_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_SUBNET_NUM && prop->subnet[i][0]; i++) {
				sprintf(key, "%s.subnet_%d", key_prefix, i+1);
				if (property_set(key, prop->subnet[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->subnet[i]);
					return ;
				}
			}
			//for (i = 0; prop->ip[i][0] && i < MAX_VIP_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_VIP_NUM && prop->ip[i][0]; i++) {
				sprintf(key, "%s.ip_%d", key_prefix, i+1);
				if (property_set(key, prop->ip[i]) != 0) {
					DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, prop->ip[i]);
					return ;
				}
			}

		}
	} else {
		sprintf(key, "%s.if", key_prefix);
		if (property_set(key, "") != 0) {
			DBG1(DBG_CHD, "property_set fail with ('%s', '')", key);
			return ;
		}
		if (is_ipv6) {
			//for (i = 0; prop->dns6[i][0] && i < MAX_DNS_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_DNS_NUM && prop->dns6[i][0]; i++) {
				sprintf(key, "%s.dns6_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}

			//for (i = 0; prop->pcscf6[i][0] && i < MAX_PCSCF_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_PCSCF_NUM && prop->pcscf6[i][0]; i++) {
				sprintf(key, "%s.pcscf6_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
			//for (i = 0; prop->netmask6[i][0] && i < MAX_NETMASK_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_NETMASK_NUM && prop->netmask6[i][0]; i++) {
				sprintf(key, "%s.netmask6_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
			//for (i = 0; prop->subnet6[i][0] && i < MAX_SUBNET_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_SUBNET_NUM && prop->subnet6[i][0]; i++) {
				sprintf(key, "%s.subnet6_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
			//for (i = 0; prop->ip6[i][0] && i < MAX_VIP_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_VIP_NUM && prop->ip6[i][0]; i++) {
				sprintf(key, "%s.ip6_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
		} else {
			//for (i = 0; prop->dns[i][0] && i < MAX_DNS_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_DNS_NUM && prop->dns[i][0]; i++) {
				sprintf(key, "%s.dns_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}

			//for (i = 0; prop->pcscf[i][0] && i < MAX_PCSCF_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_PCSCF_NUM && prop->pcscf[i][0]; i++) {
				sprintf(key, "%s.pcscf_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
			//for (i = 0; prop->netmask[i][0] && i < MAX_NETMASK_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_NETMASK_NUM && prop->netmask[i][0]; i++) {
				sprintf(key, "%s.netmask_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
			//for (i = 0; prop->subnet[i][0] && i < MAX_SUBNET_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_SUBNET_NUM && prop->subnet[i][0]; i++) {
				sprintf(key, "%s.subnet_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
			//for (i = 0; prop->ip[i][0] && i < MAX_VIP_NUM; i++) {   //Fix Static Analysis by sungwoo79.park
			for (i = 0; i < MAX_VIP_NUM && prop->ip[i][0]; i++) {
				sprintf(key, "%s.ip_%d", key_prefix, i+1);
				if (property_set(key, "") != 0) {
					return ;
				}
			}
		}
	}
	DBG1(DBG_CHD, "prefix: %s, conn: %s, if: %s, up:%d, ipv6: %d, ... Done", prefix, conn, ifname, up, is_ipv6);
}
#endif

/**
 * Invoke the updown script once for given traffic selectors
 */
static void invoke_once(private_updown_listener_t *this, ike_sa_t *ike_sa,
						child_sa_t *child_sa, child_cfg_t *config, bool up,
						traffic_selector_t *my_ts, traffic_selector_t *other_ts)
{
#ifdef ANDROID
    ike_cfg_t* ike_cfg;
#endif
	host_t *me, *other, *host;
	char *iface;
	uint8_t mask;
	mark_t mark;
	bool is_host, is_ipv6;
	int out;
	FILE *shell;
	process_t *process;
	char port_buf[PORT_BUF_LEN];
	char *envp[128] = {};
        //support_5gs [S]
        uint8_t snssai_info[10] = {0,};
        uint8_t plmnid[4] = {0,};
        //support_5gs [E]
#ifdef ANDROID
    ike_cfg = ike_sa->get_ike_cfg(ike_sa);
#endif
	me = ike_sa->get_my_host(ike_sa);
	other = ike_sa->get_other_host(ike_sa);

	push_env(envp, countof(envp), "PATH=%s", getenv("PATH"));
	push_env(envp, countof(envp), "PLUTO_VERSION=1.1");
	is_host = my_ts->is_host(my_ts, me);
	if (is_host)
	{
		is_ipv6 = me->get_family(me) == AF_INET6;
	}
	else
	{
		is_ipv6 = my_ts->get_type(my_ts) == TS_IPV6_ADDR_RANGE;
	}
	push_env(envp, countof(envp), "PLUTO_VERB=%s%s%s",
			 up ? "up" : "down",
			 is_host ? "-host" : "-client",
			 is_ipv6 ? "-v6" : "");
	push_env(envp, countof(envp), "PLUTO_CONNECTION=%s",
			 config->get_name(config));
	if (up)
	{
#ifdef ANDROID
        iface = ike_cfg->get_vif(ike_cfg);
#endif
		if (iface || charon->kernel->get_interface(charon->kernel, me, &iface))
        //orig --> if (charon->kernel->get_interface(charon->kernel, me, &iface))
		{
			cache_iface(this, child_sa->get_reqid(child_sa), iface);
		}
		else
		{
			iface = NULL;
		}
	}
	else
	{
		iface = uncache_iface(this, child_sa->get_reqid(child_sa));
	}
	push_env(envp, countof(envp), "PLUTO_INTERFACE=%s",
			 iface ? iface : "unknown");
	push_env(envp, countof(envp), "PLUTO_REQID=%u",
			 child_sa->get_reqid(child_sa));
	push_env(envp, countof(envp), "PLUTO_PROTO=%s",
			 child_sa->get_protocol(child_sa) == PROTO_ESP ? "esp" : "ah");
	push_env(envp, countof(envp), "PLUTO_UNIQUEID=%u",
			 ike_sa->get_unique_id(ike_sa));
	push_env(envp, countof(envp), "PLUTO_ME=%H", me);
	push_env(envp, countof(envp), "PLUTO_MY_ID=%Y", ike_sa->get_my_id(ike_sa));
	if (!my_ts->to_subnet(my_ts, &host, &mask))
	{
		DBG1(DBG_CHD, "updown approximates local TS %R "
					  "by next larger subnet", my_ts);
	}
	push_env(envp, countof(envp), "PLUTO_MY_CLIENT=%+H/%u", host, mask);
	host->destroy(host);
	push_env(envp, countof(envp), "PLUTO_MY_PORT=%s",
			 get_port(my_ts, other_ts, port_buf, TRUE));
	push_env(envp, countof(envp), "PLUTO_MY_PROTOCOL=%u",
			 my_ts->get_protocol(my_ts));
	push_env(envp, countof(envp), "PLUTO_PEER=%H", other);
	push_env(envp, countof(envp), "PLUTO_PEER_ID=%Y",
			 ike_sa->get_other_id(ike_sa));
	if (!other_ts->to_subnet(other_ts, &host, &mask))
	{
		DBG1(DBG_CHD, "updown approximates remote TS %R "
					  "by next larger subnet", other_ts);
	}
	push_env(envp, countof(envp), "PLUTO_PEER_CLIENT=%+H/%u", host, mask);
	host->destroy(host);
	push_env(envp, countof(envp), "PLUTO_PEER_PORT=%s",
			 get_port(my_ts, other_ts, port_buf, FALSE));
	push_env(envp, countof(envp), "PLUTO_PEER_PROTOCOL=%u",
			 other_ts->get_protocol(other_ts));
	if (ike_sa->has_condition(ike_sa, COND_EAP_AUTHENTICATED) ||
		ike_sa->has_condition(ike_sa, COND_XAUTH_AUTHENTICATED))
	{
		push_env(envp, countof(envp), "PLUTO_XAUTH_ID=%Y",
				 ike_sa->get_other_eap_id(ike_sa));
	}
	push_vip_env(this, ike_sa, envp, countof(envp), TRUE);
	push_vip_env(this, ike_sa, envp, countof(envp), FALSE);
	mark = child_sa->get_mark(child_sa, TRUE);
	if (mark.value)
	{
		push_env(envp, countof(envp), "PLUTO_MARK_IN=%u/0x%08x",
				 mark.value, mark.mask);
	}
	mark = child_sa->get_mark(child_sa, FALSE);
	if (mark.value)
	{
		push_env(envp, countof(envp), "PLUTO_MARK_OUT=%u/0x%08x",
				 mark.value, mark.mask);
	}
	if (ike_sa->has_condition(ike_sa, COND_NAT_ANY))
	{
		push_env(envp, countof(envp), "PLUTO_UDP_ENC=%u",
				 other->get_port(other));
	}
	if (child_sa->get_ipcomp(child_sa) != IPCOMP_NONE)
	{
		push_env(envp, countof(envp), "PLUTO_IPCOMP=1");
	}
	push_dns_env(this, ike_sa, envp, countof(envp));
	if (config->has_option(config, OPT_HOSTACCESS))
	{
		push_env(envp, countof(envp), "PLUTO_HOST_ACCESS=1");
	}
    push_pcscf_env(this, ike_sa, envp, countof(envp));
	push_netmask_env(this, ike_sa, envp, countof(envp));
    push_subnet_env(this, ike_sa, envp, countof(envp));
#ifdef ANDROID
    DBG1(DBG_CHD, "running updown script");
        //support_5gs [S]
        ike_sa->get_s_nssai(ike_sa, snssai_info);
        ike_sa->get_plmn_id(ike_sa, plmnid);
        update_property(&this->prop, "net.wo", config->get_name(config), iface, up, is_ipv6, ike_sa->get_psid(ike_sa),snssai_info,plmnid);
        //support_5gs [E]
	if (iface)
	{
		char key[PROPERTY_KEY_MAX], value[PROPERTY_VALUE_MAX];
		if (up) {
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.%s.reqid", iface);
			snprintf(value, PROPERTY_VALUE_MAX, "%u", child_sa->get_reqid(child_sa));
			if (property_set(key, value) != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, value);
			}
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.%s.me", iface);
			snprintf(value, PROPERTY_VALUE_MAX, "%H", me);
			if (property_set(key, value) != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, value);
			}
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.%s.peer", iface);
			snprintf(value, PROPERTY_VALUE_MAX, "%H", other);
			if (property_set(key, value) != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, value);
			}
		} else {
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.%s.reqid", iface);
			if (property_set(key, "") != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', \"\")", key);
			}
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.%s.me", iface);
			if (property_set(key, "") != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', \"\")", key);
			}
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.%s.peer", iface);
			if (property_set(key, "") != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', \"\")", key);
			}
		}
	}
	/* block this codes. because The apn_ims format was used in 5.0. after epdg6.0 we are using slot0_IMS.
	if ((strncmp(config->get_name(config), "apn_ims", 7) == 0) || (strncmp(config->get_name(config), "apn_IMS", 7) == 0))
	{
		char key[PROPERTY_KEY_MAX], value[PROPERTY_VALUE_MAX];
		if (up) {
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.epdg.ims.reqid");
			snprintf(value, PROPERTY_VALUE_MAX, "%u", child_sa->get_reqid(child_sa));
			if (property_set(key, value) != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, value);
			}
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.epdg.ims.me");
			snprintf(value, PROPERTY_VALUE_MAX, "%H", me);
			if (property_set(key, value) != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, value);
			}
			snprintf(key, PROPERTY_KEY_MAX, "net.wo.epdg.ims.peer");
			snprintf(value, PROPERTY_VALUE_MAX, "%H", other);
			if (property_set(key, value) != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', '%s')", key, value);
			}
		} else {
			if (property_set("net.wo.epdg.ims.reqid", "") != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', \"\")", key);
			}
			if (property_set("net.wo.epdg.ims.me", "") != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', \"\")", key);
			}
			if (property_set("net.wo.epdg.ims.peer", "") != 0) {
				DBG1(DBG_CHD, "property_set fail with ('%s', \"\")", key);
			}
		}
	}
	*/
	DBG1(DBG_CHD, "ike_sa: COND_REAUTHENTICATING(%d), COND_STALE(%d)",
			ike_sa->has_condition(ike_sa, COND_REAUTHENTICATING),
			ike_sa->has_condition(ike_sa, COND_STALE));
	/* for MOBIKE, but not SQC done. */

#else
	process = process_start_shell(envp, NULL, &out, NULL, "2>&1 %s",
								  config->get_updown(config));
	if (process)
	{
		shell = fdopen(out, "r");
		if (shell)
		{
			while (TRUE)
			{
				char resp[128];

				if (fgets(resp, sizeof(resp), shell) == NULL)
				{
					if (ferror(shell))
					{
						DBG1(DBG_CHD, "error reading from updown script");
					}
					break;
				}
				else
				{
					char *e = resp + strlen(resp);
					if (e > resp && e[-1] == '\n')
					{
						e[-1] = '\0';
					}
					DBG1(DBG_CHD, "updown: %s", resp);
				}
			}
			fclose(shell);
		}
		else
		{
			close(out);
		}
		process->wait(process, NULL);
	}
#endif
	free(iface);
	free_env(envp);
}

/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
/**
 * Notify Wod after Invoke once.
 */
static void notify_wod_after_invoke_once(private_updown_listener_t *this,
		child_cfg_t *config, bool up)
{
	//if (!up && !ike_sa->has_condition(ike_sa, COND_REAUTHENTICATING)) {
	if (!up)
	{	/* Config name: apn_xxx */
		notify_wod(N_DETACH, config->get_name(config), NULL);
	}
	else
	{
		if (this->prop.pcscf6[0][0] || this->prop.pcscf[0][0])
		{
			notify_wod(N_PCSCF,config->get_name(config), &this->prop);
		}
		if (this->prop.dns6[0][0] || this->prop.dns[0][0])
		{
			notify_wod(N_DNS, config->get_name(config), &this->prop);
		}
		notify_wod(N_ATTACH, config->get_name(config), NULL);
	}
}
/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */

METHOD(listener_t, child_updown, bool,
	private_updown_listener_t *this, ike_sa_t *ike_sa, child_sa_t *child_sa,
	bool up)
{
	traffic_selector_t *my_ts, *other_ts;
	enumerator_t *enumerator;
	child_cfg_t *config;

	config = child_sa->get_config(child_sa);
	if (config->get_updown(config))
	{
		enumerator = child_sa->create_policy_enumerator(child_sa);
		while (enumerator->enumerate(enumerator, &my_ts, &other_ts))
		{
			invoke_once(this, ike_sa, child_sa, config, up, my_ts, other_ts);
		}
		/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
		notify_wod_after_invoke_once(this, config, up);
		/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
		enumerator->destroy(enumerator);
	}
	return TRUE;
}

METHOD(updown_listener_t, destroy, void,
	private_updown_listener_t *this)
{
	this->iface_cache->destroy(this->iface_cache);
	free(this);
}

/**
 * See header
 */
updown_listener_t *updown_listener_create(updown_handler_t *handler)
{
	private_updown_listener_t *this;

	INIT(this,
		.public = {
			.listener = {
				.child_updown = _child_updown,
			},
			.destroy = _destroy,
		},
		.iface_cache = linked_list_create(),
		.handler = handler,
	);

	return &this->public;
}
