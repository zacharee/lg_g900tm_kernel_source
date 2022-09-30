/*
 * Copyright (C) 2005-2007 Martin Willi
 * Copyright (C) 2005 Jan Hutter
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

#include "retransmit_job.h"

#include <daemon.h>
/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
#include <hardware_legacy/power.h>
/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */

typedef struct private_retransmit_job_t private_retransmit_job_t;

/**
 * Private data of an retransmit_job_t Object.
 */
struct private_retransmit_job_t {
	/**
	 * Public retransmit_job_t interface.
	 */
	retransmit_job_t public;

	/**
	 * Message ID of the request to resend.
	 */
	uint32_t message_id;

	/**
	 * ID of the IKE_SA which the message belongs to.
	 */
	ike_sa_id_t *ike_sa_id;
	/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
	bool wakelock_aqured;
	/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
};

METHOD(job_t, destroy, void,
	private_retransmit_job_t *this)
{
	this->ike_sa_id->destroy(this->ike_sa_id);
	/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
	if (this->wakelock_aqured) {
        this->wakelock_aqured = FALSE;
		u_int32_t release_wake_lock_ret = release_wake_lock(RETRANSMIT_DPD_WAKELOCK);
		DBG2(DBG_IKE, "[LGE][IWLAN] wakelock released: %s, ret=%d", RETRANSMIT_DPD_WAKELOCK, release_wake_lock_ret);
	}
	/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
	free(this);
}

METHOD(job_t, execute, job_requeue_t,
	private_retransmit_job_t *this)
{
	ike_sa_t *ike_sa;

	ike_sa = charon->ike_sa_manager->checkout(charon->ike_sa_manager,
											  this->ike_sa_id);
	/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
	if (this->wakelock_aqured) {
        this->wakelock_aqured = FALSE;
		u_int32_t release_wake_lock_ret = release_wake_lock(RETRANSMIT_DPD_WAKELOCK);
		DBG2(DBG_IKE, "[LGE][IWLAN] wakelock released: %s, ret=%d", RETRANSMIT_DPD_WAKELOCK, release_wake_lock_ret);
	}
	/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
	if (ike_sa)
	{
		if (ike_sa->retransmit(ike_sa, this->message_id) == DESTROY_ME)
		{
			/* retransmitted to many times, giving up */
			charon->ike_sa_manager->checkin_and_destroy(charon->ike_sa_manager,
														ike_sa);
		}
		else
		{
			charon->ike_sa_manager->checkin(charon->ike_sa_manager, ike_sa);
		}
	}
	return JOB_REQUEUE_NONE;
}

METHOD(job_t, get_priority, job_priority_t,
	private_retransmit_job_t *this)
{
	return JOB_PRIO_HIGH;
}

/*
 * Described in header.
 */
/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
// original
// retransmit_job_t *retransmit_job_create(uint32_t message_id,ike_sa_id_t *ike_sa_id)
retransmit_job_t *retransmit_job_create(uint32_t message_id,ike_sa_id_t *ike_sa_id, bool aqure_wakelock)
/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
{
	private_retransmit_job_t *this;

	INIT(this,
		.public = {
			.job_interface = {
				.execute = _execute,
				.get_priority = _get_priority,
				.destroy = _destroy,
			},
		},
		.message_id = message_id,
		.ike_sa_id = ike_sa_id->clone(ike_sa_id),
		/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START] */
		.wakelock_aqured = aqure_wakelock,
		/* 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END] */
	);

	return &this->public;
}
