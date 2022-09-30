#ifndef __WOD_CHANNEL_H__
#define __WOD_CHANNEL_H__

#define	MAX_VIP_NUM		2
#define	MAX_DNS_NUM		6
#define	MAX_PCSCF_NUM	6
#define	MAX_NETMASK_NUM	2
#define MAX_SUBNET_NUM	2
#define	IP_ADDR_LEN		16	//	xxx.xxx.xxx.xxx
#define	IP6_ADDR_LEN	40	//	xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
#define MAX_BUF_LEN      1024

typedef enum wo_notify_cmd_t {
	N_ATTACH,
	N_DETACH,
	N_SETVIF,
	N_PCSCF,
	N_DNS
} wo_notify_cmd_t;

typedef struct conn_info {
	char ip[MAX_VIP_NUM][IP_ADDR_LEN];
	char ip6[MAX_VIP_NUM][IP6_ADDR_LEN];
	char dns[MAX_DNS_NUM][IP_ADDR_LEN];
	char dns6[MAX_DNS_NUM][IP6_ADDR_LEN];
	char pcscf[MAX_PCSCF_NUM][IP_ADDR_LEN];
	char pcscf6[MAX_PCSCF_NUM][IP6_ADDR_LEN];
	char netmask[MAX_NETMASK_NUM][IP_ADDR_LEN];
	char netmask6[MAX_NETMASK_NUM][IP6_ADDR_LEN];
	char subnet[MAX_SUBNET_NUM][IP_ADDR_LEN];
	char subnet6[MAX_SUBNET_NUM][IP6_ADDR_LEN];
} conn_info_prop;

/* 2016-07-02 protocol-iwlan@lge.com LGP_DATA_IWLAN_DPD_NOW [START] */
void notify_dpd_status(const char *conn_name, int status);
/* 2016-07-02 protocol-iwlan@lge.com LGP_DATA_IWLAN_DPD_NOW [END] */
void notify_error(const char *value, const char *conn_name);
void notify_wod(wo_notify_cmd_t cmd, const char *value, conn_info_prop* prop);
int atcmd_txrx(char* txbuf, char *rxbuf, int *rxlen);

#endif // __WOD_CHANNEL_H__

