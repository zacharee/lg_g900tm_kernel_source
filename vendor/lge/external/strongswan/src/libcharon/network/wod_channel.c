#include "wod_channel.h"

#include <daemon.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#ifdef ANDROID
#include <cutils/sockets.h>

#include <errno.h>
#include <string.h>


#define	WOD_TCP_TIMEOUT		10

static int wod_tcp_txrx(char* txbuf, char *rxbuf, int rxbuf_size, int *rxlen)
{
	int sockfd = -1;
	int ret = -1;
	int txlen;
	struct sockaddr_in servaddr,cliaddr;
	struct timeval timeout = { .tv_sec = WOD_TCP_TIMEOUT, .tv_usec = 0 };

	*rxlen = 0;
	if (txbuf == NULL || rxbuf == NULL) {
		DBG1(DBG_IKE, "Error: txbuf(0x%08x) or rxbuf(0x%08x) is NULL", txbuf, rxbuf);
		goto end;
	}

   	sockfd = socket_local_client("wod_ipsec", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
	if (sockfd < 0) {
		DBG1(DBG_IKE, "Error: create android socket failed: %d %s", sockfd, strerror(errno));
		goto end;
	}

	if ((txlen = strlen(txbuf)) > 0)
	{
		txlen++; /* include '\0' to be delimiter */
	}
	else
	{
		DBG1(DBG_IKE, "Error: invalid txlen=%d", txlen);
		goto end;
	}

#ifdef DEBUG_LOG_DISABLE
	DBG1(DBG_IKE, "Tx to WOD - len:%d", txlen);
#else
	DBG1(DBG_IKE, "Tx to WOD - len:%d, data:[%s]", txlen, txbuf);
#endif

	if (send(sockfd, txbuf, txlen, 0) < 0) {
		DBG1(DBG_IKE, "Error: send failed");
		goto end;
	}

	if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
		DBG1(DBG_IKE, "Error: setsockopt SO_RCVTIMEO failed");
	}

	ret = recv(sockfd, rxbuf, rxbuf_size - 1, 0);
	if (ret <= 0) {
		DBG1(DBG_IKE, "Error: %s", (ret == 0)? "session closed" : "recv failed or Timeout");
		ret = -1;
		goto end;
	}
	*rxlen = ret;

#ifdef DEBUG_LOG_DISABLE
	DBG1(DBG_IKE, "RX from WOD - len:%d", *rxlen);
#else
	DBG1(DBG_IKE, "RX from WOD - len:%d, data:[%s]", *rxlen, rxbuf);
#endif

	ret = 0;

end:
	if (sockfd) {
		close(sockfd);
	}
	return ret;
}

#else

static int wod_tcp_txrx(char* txbuf, char *rxbuf, int rxbuf_size, int *rxlen)
{
	return 0;
}

#endif

/* 2016-07-02 protocol-iwlan@lge.com LGP_DATA_IWLAN_DPD_NOW [START] */
void notify_dpd_status(const char *conn_name, int status)
{
	char rxbuf[MAX_BUF_LEN] = {0, };
	char tmp[MAX_BUF_LEN] = {0, };
	int rxlen = 0;

	snprintf(tmp, MAX_BUF_LEN, "ipsecdpd=%s,%d", conn_name, status);
	wod_tcp_txrx(tmp, rxbuf, MAX_BUF_LEN, &rxlen);
}
/* 2016-07-02 protocol-iwlan@lge.com LGP_DATA_IWLAN_DPD_NOW [END] */

//changhwan.lee
void notify_error(const char *value, const char *conn_name)
{
	char rxbuf[MAX_BUF_LEN] = {0, };
	char tmp[MAX_BUF_LEN] = {0, };
	char addrs[MAX_BUF_LEN] = {0, };
	int rxlen = 0;
	int n_6, n_4;

	snprintf(tmp, MAX_BUF_LEN, "ipsecerror=%s,%s", conn_name, value);

	DBG1(DBG_IKE, "RX from charon - [%s]", tmp);

	wod_tcp_txrx(tmp, rxbuf, MAX_BUF_LEN, &rxlen);
}
//changhwan.lee

void notify_wod(wo_notify_cmd_t cmd, const char *value, conn_info_prop* prop)
{
	char rxbuf[MAX_BUF_LEN] = {0, };
	char tmp[MAX_BUF_LEN] = {0, };
	char addrs[MAX_BUF_LEN] = {0, };
	int rxlen = 0;
	int n_6, n_4;

	switch (cmd)
	{
		case N_ATTACH:
			snprintf(tmp, MAX_BUF_LEN, "ipsecattach=%s", value);
			break;
		case N_DETACH:
			snprintf(tmp, MAX_BUF_LEN, "ipsecdetach=%s", value);
			break;
		case N_SETVIF:
			snprintf(tmp, MAX_BUF_LEN, "ipsecsetvif=%s", value);
			break;
		case N_PCSCF:
			//for (n_6 = 0; prop->pcscf6[n_6][0] && n_6 < MAX_PCSCF_NUM; ++n_6)   //Fix Static Analysis by sungwoo79.park
			for (n_6 = 0; n_6 < MAX_PCSCF_NUM && prop->pcscf6[n_6][0]; ++n_6)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->pcscf6[n_6]);
			}
			//for (n_4 = 0; prop->pcscf[n_4][0] && n_4 < MAX_PCSCF_NUM; ++n_4)   //Fix Static Analysis by sungwoo79.park
			for (n_4 = 0; n_4 < MAX_PCSCF_NUM && prop->pcscf[n_4][0]; ++n_4)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->pcscf[n_4]);
			}
			snprintf(tmp, MAX_BUF_LEN, "ipsecpcscf=%s,%d,%d%s", value, n_6, n_4, addrs);
			break;
		case N_DNS:
			//for (n_6 = 0; prop->dns6[n_6][0] && n_6 < MAX_DNS_NUM; ++n_6)   //Fix Static Analysis by sungwoo79.park
			for (n_6 = 0; n_6 < MAX_DNS_NUM && prop->dns6[n_6][0]; ++n_6)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->dns6[n_6]);
			}
			//for (n_4 = 0; prop->dns[n_4][0] && n_4 < MAX_DNS_NUM; ++n_4)   //Fix Static Analysis by sungwoo79.park
			for (n_4 = 0; n_4 < MAX_DNS_NUM && prop->dns[n_4][0]; ++n_4)
			{
				snprintf(addrs, MAX_BUF_LEN, "%s,%s", addrs, prop->dns[n_4]);
			}
			snprintf(tmp, MAX_BUF_LEN, "ipsecdns=%s,%d,%d%s", value, n_6, n_4, addrs);
			break;
		default:
			break;
	}

	wod_tcp_txrx(tmp, rxbuf, MAX_BUF_LEN, &rxlen);
}

int atcmd_txrx(char* txbuf, char *rxbuf, int *rxlen)
{
	int ret, wod_rxlen;
	char inrxbuf[MAX_BUF_LEN] = {0, };

	*rxlen = 0;

	ret = wod_tcp_txrx(txbuf, inrxbuf, MAX_BUF_LEN, &wod_rxlen);
	if (ret < 0) {
		goto end;
	}

	if ((inrxbuf[0] == 0x0D) && (inrxbuf[1] == 0x0A)) {
		*rxlen = wod_rxlen - 12;
		while (*rxlen < (wod_rxlen - 4)) {
			if ((inrxbuf[*rxlen+2] == 0x0D) && (inrxbuf[*rxlen+3] == 0x0A)) {
				break;
			}
			(*rxlen)++;
		}
		memcpy(rxbuf, inrxbuf+2, *rxlen);
	}

end:
	rxbuf[*rxlen] = 0;
	return ret;
}

