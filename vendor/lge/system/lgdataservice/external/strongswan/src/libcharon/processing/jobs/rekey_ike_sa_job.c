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

#include "rekey_ike_sa_job.h"

#include <daemon.h>
#include <time.h>
#include <utils/cust_settings.h>

typedef struct private_rekey_ike_sa_job_t private_rekey_ike_sa_job_t;

/**
 * Private data of an rekey_ike_sa_job_t object.
 */
struct private_rekey_ike_sa_job_t {
	/**
	 * Public rekey_ike_sa_job_t interface.
	 */
	rekey_ike_sa_job_t public;

	/**
	 * ID of the IKE_SA to rekey
	 */
	ike_sa_id_t *ike_sa_id;

	/**
	 * force reauthentication of the peer (full IKE_SA setup)
	 */
	bool reauth;
};

METHOD(job_t, destroy, void,
	private_rekey_ike_sa_job_t *this)
{
	this->ike_sa_id->destroy(this->ike_sa_id);
	free(this);
}

/**
 * Check if we should delay a reauth, and by how many seconds
 */
static uint32_t get_retry_delay(ike_sa_t *ike_sa)
{
	enumerator_t *enumerator;
	child_sa_t *child_sa;
	uint32_t retry = 0;

	/* avoid reauth collisions for certain IKE_SA/CHILD_SA states */
	if (ike_sa->get_state(ike_sa) != IKE_ESTABLISHED)
	{
		retry = RETRY_INTERVAL - (random() % RETRY_JITTER);
		DBG1(DBG_IKE, "unable to reauthenticate in %N state, delaying for %us",
			 ike_sa_state_names, ike_sa->get_state(ike_sa), retry);
	}
	else
	{
		enumerator = ike_sa->create_child_sa_enumerator(ike_sa);
		while (enumerator->enumerate(enumerator, &child_sa))
		{
			if (child_sa->get_state(child_sa) != CHILD_INSTALLED &&
				child_sa->get_state(child_sa) != CHILD_REKEYED)
			{
				retry = RETRY_INTERVAL - (random() % RETRY_JITTER);
				DBG1(DBG_IKE, "unable to reauthenticate in CHILD_SA %N state, "
					 "delaying for %us", child_sa_state_names,
					 child_sa->get_state(child_sa), retry);
				break;
			}
		}
		enumerator->destroy(enumerator);
	}
	return retry;
}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
METHOD(job_t, checkout, ike_sa_t *,
	private_rekey_ike_sa_job_t *this)
{
	ike_sa_t *ike_sa = NULL;
	int j = 0;
	struct timespec tsa, tsr;

	do {
		ike_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager, this->ike_sa_id);
		if (ike_sa) {
			break;
		}
		j++;
		DBG1(DBG_IKE, "IKE_SA to rekey not found, loop: %d", j);
		tsa.tv_sec = 0;
		tsa.tv_nsec = 0;
		tsr.tv_sec = 0;
		tsr.tv_nsec = 500000000;
		clock_nanosleep(CLOCK_REALTIME_ALARM, 0, &tsr, &tsa);
	} while (j < 10);

	return ike_sa;
}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
METHOD(job_t, execute, job_requeue_t,
	private_rekey_ike_sa_job_t *this)
{
	ike_sa_t *ike_sa= NULL;
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
	status_t status = SUCCESS;
	uint32_t retry = 0;
	int i = 0;
	struct timespec tsa, tsr;
	int slotid = 0;
	ike_sa_t *ike_sa_current_thread = NULL;

	ike_sa_current_thread = charon->ike_sa_manager->get_sa_by_ikeid_rekey(charon->ike_sa_manager, this->ike_sa_id);
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
	ike_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager,
							this->ike_sa_id);
	}
	if (ike_sa == NULL)
	{
		DBG2(DBG_JOB, "IKE_SA to rekey not found");
	}
	else
	{
		if (this->reauth)
		{
			retry = get_retry_delay(ike_sa);
			if (!retry)
			{
				status = ike_sa->reauth(ike_sa);
			}
		}
		else
		{
			status = ike_sa->rekey(ike_sa);
		}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
		if (get_cust_setting_bool_by_slotid(slotid, REKEY_DELAY))
		{
			for (i = 1; (i < 60) && (status == NEED_MORE); i++)
			{
				charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);

				DBG1(DBG_IKE, "delaying rekey_ike_sa initiation,loop: %d", i);
				tsa.tv_sec = 0;
				tsa.tv_nsec = 0;
				tsr.tv_sec = 0;
				tsr.tv_nsec = 500000000;
				clock_nanosleep(CLOCK_REALTIME_ALARM, 0, &tsr, &tsa);

				ike_sa = checkout(this);
				if (ike_sa == NULL)
				{
					DBG1(DBG_JOB, "IKE_SA with ike_sa_id %d not found for rekeying",  this->ike_sa_id);
					return JOB_REQUEUE_NONE;
				}

				if (this->reauth)
				{
					if (!retry)
					{
						status = ike_sa->reauth(ike_sa);
					} else {
						break;
					}
				}
				else
				{
					status = ike_sa->rekey(ike_sa);
				}
			}
		}
/* 2016-02-26 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
		if (status == DESTROY_ME)
		{
			charon->ike_sa_manager->checkin_and_destroy(charon->ike_sa_manager,
														ike_sa);
		}
		else
		{
			charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
		}
	}
	if (retry)
	{
		return JOB_RESCHEDULE(retry);
	}
	return JOB_REQUEUE_NONE;
}

METHOD(job_t, get_priority, job_priority_t,
	private_rekey_ike_sa_job_t *this)
{
	return JOB_PRIO_MEDIUM;
}

/*
 * Described in header
 */
rekey_ike_sa_job_t *rekey_ike_sa_job_create(ike_sa_id_t *ike_sa_id, bool reauth)
{
	private_rekey_ike_sa_job_t *this;

	INIT(this,
		.public = {
			.job_interface = {
				.execute = _execute,
				.get_priority = _get_priority,
				.destroy = _destroy,
			},
		},
		.ike_sa_id = ike_sa_id->clone(ike_sa_id),
		.reauth = reauth,
	);

	return &(this->public);
}
