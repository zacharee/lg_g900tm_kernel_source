/*
 * Copyright (C) 2006 Martin Willi
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

#include "rekey_child_sa_job.h"

#include <daemon.h>
#include <time.h>
#include <utils/cust_settings.h>

typedef struct private_rekey_child_sa_job_t private_rekey_child_sa_job_t;

/**
 * Private data of an rekey_child_sa_job_t object.
 */
struct private_rekey_child_sa_job_t {

	/**
	 * Public rekey_child_sa_job_t interface.
	 */
	rekey_child_sa_job_t public;

	/**
	 * protocol of the CHILD_SA (ESP/AH)
	 */
	protocol_id_t protocol;

	/**
	 * inbound SPI of the CHILD_SA
	 */
	uint32_t spi;

	/**
	 * SA destination address
	 */
	host_t *dst;
};

METHOD(job_t, destroy, void,
	private_rekey_child_sa_job_t *this)
{
	this->dst->destroy(this->dst);
	free(this);
}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
METHOD(job_t, checkout, ike_sa_t*,
	private_rekey_child_sa_job_t *this)
{
	ike_sa_t *ike_sa = NULL;
	int j = 0;
	struct timespec tsa, tsr;

	do {
		ike_sa = charon->child_sa_manager->checkout(charon->child_sa_manager,
										this->protocol, this->spi, this->dst, NULL);
		if (ike_sa) {
			break;
		}
		j++;
		//TODO need print more info.
		DBG1(DBG_JOB, "CHILD_SA not found for rekeying, loop: %d", j);
		tsa.tv_sec = 0;
		tsa.tv_nsec = 0;
		tsr.tv_sec = 0;
		tsr.tv_nsec = 500000000;
		clock_nanosleep(CLOCK_REALTIME_ALARM, 0, &tsr, &tsa);
	} while (j < 10);

	return ike_sa;
}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */

METHOD(job_t, execute, job_requeue_t,
	private_rekey_child_sa_job_t *this)
{
	ike_sa_t *ike_sa = NULL;
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
	status_t status = SUCCESS;
	int i = 0;
	struct timespec tsa, tsr;
	int slotid = 0;
	ike_sa_t *ike_sa_current_thread = NULL;

	ike_sa_current_thread = charon->child_sa_manager->get_sa(charon->child_sa_manager,
														this->protocol, this->spi, this->dst);
	if (ike_sa_current_thread != NULL)
	{
		slotid = get_slotid(ike_sa_current_thread->get_name(ike_sa_current_thread));
	}

	if (get_cust_setting_bool_by_slotid(slotid, REKEY_DELAY))
	{
		ike_sa = checkout(this);
	}
	else
	{
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
	ike_sa = charon->child_sa_manager->checkout(charon->child_sa_manager,
									this->protocol, this->spi, this->dst, NULL);
	}
	if (ike_sa == NULL)
	{
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
		if (get_cust_setting_bool_by_slotid(slotid, REKEY_DELAY))
		{
			DBG1(DBG_JOB, "CHILD_SA to rekey not found");
		}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
		else
		{
		DBG1(DBG_JOB, "CHILD_SA %N/0x%08x/%H not found for rekey",
			 protocol_id_names, this->protocol, htonl(this->spi), this->dst);
		}
	}
	else
	{
		if (ike_sa->get_state(ike_sa) != IKE_PASSIVE)
		{
			/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
			if (get_cust_setting_bool_by_slotid(slotid, REKEY_DELAY))
			{
				status = ike_sa->rekey_child_sa(ike_sa, this->protocol, this->spi);
				for (i = 1; (i < 60) && (status == NEED_MORE); i++)
				{
					charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
					DBG1(DBG_IKE, "delaying rekey_child_sa_job initiation,loop: %d", i);

					tsa.tv_sec = 0;
					tsa.tv_nsec = 0;
					tsr.tv_sec = 0;
					tsr.tv_nsec = 500000000;
					clock_nanosleep(CLOCK_REALTIME_ALARM, 0, &tsr, &tsa);

					ike_sa = checkout(this);
					if (ike_sa == NULL)
					{
						//TODO need print more info
						DBG1(DBG_JOB, "CHILD_SA not found for rekeying");
						return JOB_REQUEUE_NONE;
					}
					status = ike_sa->rekey_child_sa(ike_sa, this->protocol, this->spi);
				}
			}
			else
			{
			/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
			ike_sa->rekey_child_sa(ike_sa, this->protocol, this->spi);
			}
		}
		charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
	}
	return JOB_REQUEUE_NONE;
}

METHOD(job_t, get_priority, job_priority_t,
	private_rekey_child_sa_job_t *this)
{
	return JOB_PRIO_MEDIUM;
}

/*
 * Described in header
 */
rekey_child_sa_job_t *rekey_child_sa_job_create(protocol_id_t protocol,
												uint32_t spi, host_t *dst)
{
	private_rekey_child_sa_job_t *this;

	INIT(this,
		.public = {
			.job_interface = {
				.execute = _execute,
				.get_priority = _get_priority,
				.destroy = _destroy,
			},
		},
		.protocol = protocol,
		.spi = spi,
		.dst = dst->clone(dst),
	);

	return &this->public;
}
