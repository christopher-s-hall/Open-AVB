/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <ieee1588.hpp>

#include <mdport.hpp>
#include <avbts_message.hpp>
#include <avbts_clock.hpp>

#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_oscondition.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>

#include <utility>

LinkLayerAddress MediaDependentEtherPort::pdelay_multicast(PDELAY_MULTICAST);

LinkLayerAddress
MediaDependentEtherPort::test_status_multicast(TEST_STATUS_MULTICAST);

MediaDependentEtherPort::MediaDependentEtherPort
(IEEE1588PortInit_t *portInit) : MediaDependentPort( portInit )
{
	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;
}

bool MediaDependentEtherPort::_init_port
( MediaIndependentPort *port, IEEE1588PortInit_t *portInit )
{
	pdelay_rx_lock = lock_factory->createLock(oslock_recursive);
	if( pdelay_rx_lock == NULL )
		return false;

	port_tx_lock = lock_factory->createLock(oslock_recursive);
	if( port_tx_lock == NULL )
		return false;

	return true;
}

void *MediaDependentEtherPort::listenPort()
{
        port_ready_condition->signal();
        struct phy_delay get_delay = { 0, 0, 0, 0 };
        if(port->_hw_timestamper)
                port->_hw_timestamper->get_phy_delay(&get_delay);

        while (1) {
                PTPMessageCommon *msg;
                uint8_t buf[128];
                LinkLayerAddress remote;
                net_result rrecv;
                size_t length = sizeof(buf);

                if ((rrecv = getNetIFace->nrecv
		     ( &remote, buf, length,&get_delay )) == net_succeed )
		{
                        GPTP_LOG_VERBOSE("Processing network buffer");
			msg = buildPTPMessage((char *)buf, (int)length,
					      &remote, this);
                        if (msg != NULL)
			{
                                GPTP_LOG_VERBOSE("Processing message");
                                msg->processMessage(this);
                                if (msg->garbage())
				{
                                        delete msg;
                                }
                        } else
			{
                                GPTP_LOG_ERROR("Discarding invalid message");
                        }
		} else if (rrecv == net_fatal)
		{
                        GPTP_LOG_ERROR("read from network interface failed");
                        this->processEvent(FAULT_DETECTED);
                        break;
                }
        }

        return NULL;
}


bool MediaDependentEtherPort::listenPort( OSCondition *port_ready_condition )
{
	bool ret = true;
	port_ready_condition->signal(); /* Check return value? */
	OSTimer *timer = timer_factory->createTimer();

	while (1) {
		PTPMessageCommon *msg;
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);
		bool event = false;
		bool ts_good = false;
		Timestamp rx_timestamp;

		if ((rrecv = net_iface->nrecv(&remote, buf, length)) == net_succeed) {
			XPTPD_INFO("Processing network buffer");

			getClock()->timerq_lock();
			getClock()->lock();
			getPort()->lock();
			lock();
			timestamper_lock();

			msg = buildPTPMessage
				((char *)buf, (int)length, &remote, this,
				 &event );

			if( event ) {
				ts_good = getRxTimestampRetry
					( msg, rx_timestamp, RX_TIMEOUT_ITER, RX_TIMEOUT_BASE );
			}


			if ( msg != NULL && ts_good == event ) {
				XPTPD_INFO("Processing message");
				msg->setTimestamp( rx_timestamp );
				msg->processMessage((MediaDependentPort *) this );
				if (msg->garbage()) {
					delete msg;
				}
			} else {
				XPTPD_ERROR("Discarding invalid message");
			}

			timestamper_unlock();
			unlock();
			getPort()->unlock();
			getClock()->unlock();
			getClock()->timerq_unlock();
		} else if (rrecv == net_fatal) {
			XPTPD_ERROR("read from network interface failed");
			this->processEvent(FAULT_DETECTED);
			ret = false;
			break;
		}
	}

	delete timer;
	return ret;
}

MediaDependentEtherPort::~MediaDependentEtherPort()
{
}

net_result MediaDependentEtherPort::port_send
( uint16_t etherType, uint8_t *buf, size_t size, MulticastType mcast_type,
  bool timestamp )
{
	LinkLayerAddress dest;

	if (mcast_type != MCAST_NONE) {
		if (mcast_type == MCAST_PDELAY) {
			dest = pdelay_multicast;
		} else {
			dest = other_multicast;
		}
	} else {
		return net_fatal;
	}

	return net_iface->send( &dest, (uint8_t *) buf, size, timestamp);
}

net_result MediaDependentEtherPort::sendGeneralPort
( uint16_t etherType, uint8_t *buf, size_t len, MulticastType mcast_type )
{
	return port_send( buf, len, mcast_type, false );
}

net_result MediaDependentEtherPort::sendEventPort
( uint16_t etherType, uint8_t *buf, size_t size, MulticastType mcast_type )
{
	return port_send( buf, size, mcast_type, true );
}

bool MediaDependentEtherPort::processPDelay( uint16_t seq_id, long *wait_time ) {
	bool ts_good;
	*wait_time = 0;
	if( timestamper_lock() == oslock_fail ) return false;

	{
		Timestamp req_timestamp;

		PTPMessagePathDelayReq *pdelay_req =
			new PTPMessagePathDelayReq(this);
		PortIdentity dest_id;
		port->getPortIdentity(dest_id);
		pdelay_req->setPortIdentity(&dest_id);
		pdelay_req->setSequenceId( seq_id );

		{
			Timestamp pending =
				PDELAY_PENDING_TIMESTAMP;
			pdelay_req->setTimestamp(pending);
		}

		if (last_pdelay_req != NULL) {
			delete last_pdelay_req;
		}
		setLastPDelayReq(pdelay_req);

		getTxLock();
		XPTPD_INFO("Preparing to send PDelay Request");
		pdelay_req->sendPort( this );
		XPTPD_INFO("Sent PDelay Request");

		*wait_time = TX_TIMEOUT_BASE;
		ts_good = getTxTimestampRetry
			( pdelay_req, req_timestamp,
			  TX_TIMEOUT_ITER, wait_time );

		putTxLock();

		if ( ts_good ) {
			pdelay_req->setTimestamp(req_timestamp);
			XPTPD_INFO("Got PDelay Timestamp");
		} else {
			Timestamp failed = INVALID_TIMESTAMP;
			pdelay_req->setTimestamp(failed);
			XPTPD_INFO("Pdelay timestamp failed");
		}

		if ( !ts_good ) {
			XPTPD_ERROR(
						"Error (TX) timestamping PDelay request, error=%s\n",
						!ts_good ? "true" : "false" );
		}
	}

	if( timestamper_unlock() == oslock_fail ) return false;

	return ts_good;
}

bool MediaDependentEtherPort::processSync
( uint16_t seq_id, bool grandmaster, long *wait_time )
{
	/* Set offset from master to zero, update device vs
	   system time offset */
	*wait_time = 0;
	Timestamp system_time;
	Timestamp device_time;
	bool ts_good;
	Timestamp sync_timestamp;

	bool ret = true;
	PortIdentity dest_id;
	PTPMessageSync *sync;
	PTPMessageFollowUp *follow_up;
	clock_offset_t offset;

	if( timestamper_lock() == oslock_fail ) return false;

	getDeviceTime( system_time, device_time );

	offset.ls_freqoffset = getLocalSystemRateOffset();
	XPTPD_INFOL( SYNC_DEBUG, "local-system frequency offset: %Lf",
				 offset.ls_freqoffset );

	// Only do this update if we are port 0
	// Prevents multiple updates on a multiport grandmaster
	if( getPort()->getPortNumber() == 1 ) {
		memset( &offset, 0, sizeof(offset));
		offset.master_time = TIMESTAMP_TO_NS(device_time);
		offset.ml_freqoffset = 1.0;
		offset.ls_phoffset = TIMESTAMP_TO_NS(system_time - device_time);
		offset.sync_count = getPort()->getSyncCount();
		offset.pdelay_count = getPort()->getPdelayCount();
		getPort()->getClock()->setMasterOffset( &offset );
	}

	sync = new PTPMessageSync( this );
	// Send a sync message and then a followup to broadcast
	port->getPortIdentity(dest_id);
	sync->setSequenceId( seq_id );
	sync->setPortIdentity(&dest_id);

	getTxLock();

	if( sync->sendPort( this ) != net_succeed ) {
		ret = false;
		goto bail_syncsnd;
	}
	XPTPD_INFO("Sent SYNC message");

	*wait_time = TX_TIMEOUT_BASE;
	ts_good = getTxTimestampRetry
		( sync, sync_timestamp,
		  TX_TIMEOUT_ITER, wait_time );

	if ( !ts_good ) {
		XPTPD_ERROR
			( "Error (TX) timestamping Sync" );
		goto bail_syncsnd;
	}

	uint32_t device_sync_time_offset;

	device_sync_time_offset = TIMESTAMP_TO_NS( device_time - sync_timestamp );

	// Calculate sync timestamp in terms of system clock
	system_time - (uint64_t)
		(((FrequencyRatio)device_sync_time_offset) /
			offset.ls_freqoffset);

	follow_up = new PTPMessageFollowUp(this);
	port->getPortIdentity(dest_id);
	follow_up->setPortIdentity(&dest_id);
	follow_up->setSequenceId(sync->getSequenceId());
	if( !grandmaster ) {
		uint64_t correction_field;
		unsigned residence_time;
		follow_up->setPreciseOriginTimestamp
			(getClock()->getLastSyncOriginTime());
		// Calculate residence time
		if( getClock()->getLastSyncReceiveDeviceId() ==
			getTimestampDeviceId() ) {
			// Same timestamp source
			residence_time =
				TIMESTAMP_TO_NS
				( sync_timestamp -
				  (getClock()->getLastSyncReceiveDeviceTime()));
			residence_time = (unsigned) (((FrequencyRatio)residence_time)*
				getClock()->getLastSyncCumulativeOffset());
			follow_up->setRateOffset
				( getClock()->getLastSyncCumulativeOffset() );
		} else {
			// Different timestamp source
				residence_time =
				TIMESTAMP_TO_NS
					(system_time -
					 (getClock()->getLastSyncReceiveSystemTime()));
			residence_time = (unsigned) (((FrequencyRatio)residence_time)*
				getClock()->getMasterSystemFrequencyOffset());
			follow_up->setRateOffset
				( getClock()->getMasterSystemFrequencyOffset()/
				  offset.ls_freqoffset );
		}
		// Add to correction field
		correction_field =  getClock()->getLastSyncCorrection();
		correction_field += ((uint64_t)residence_time) << 16;
		follow_up->setCorrectionField( correction_field );
	} else {
		follow_up->setPreciseOriginTimestamp( system_time );
	}
	if( follow_up->sendPort( this ) != net_succeed ) {
		ret = false;
		goto bail_fwupsnd;
	}

 bail_fwupsnd:
	delete follow_up;
 bail_syncsnd:
	delete sync;
	putTxLock();

	if( timestamper_unlock() == oslock_fail ) {
		ret = false;
	}

	return ret;
}

bool MediaDependentEtherPort::adjustClockRate( double freq_offset ) {
	return !_hw_timestamper ? false :
		_hw_timestamper->HWTimestamper_adjclockrate( (float) freq_offset );
}
bool MediaDependentEtherPort::adjustClockPhase( int64_t phase_adjust ) {
	return !_hw_timestamper ? false :
		_hw_timestamper->HWTimestamper_adjclockphase( phase_adjust );
}

bool MediaDependentEtherPort::processEvent(Event e)
{
	switch (e) {
	case PDELAY_DEFERRED_PROCESSING:
		getClock()->lock();
		lock();
		timestamper_lock();

		pdelay_rx_lock->lock();
		if (last_pdelay_resp_fwup == NULL) {
			XPTPD_ERROR("PDelay Response Followup is NULL!");
			abort();
		}
		last_pdelay_resp_fwup->processMessage(this);
		if (last_pdelay_resp_fwup->garbage()) {
			delete last_pdelay_resp_fwup;
			this->setLastPDelayRespFollowUp(NULL);
		}
		pdelay_rx_lock->unlock();

		timestamper_unlock();
		unlock();
		getClock()->unlock();
		break;
	default:
		XPTPD_INFO
		    ( "Unhandled event type in "
			  "MediaDependentEtherPort::processEvent(), %d", e );
		return false;
	}
	return true;
}

bool MediaDependentEtherPort::getTxTimestampRetry
(PTPMessageCommon *msg, Timestamp & timestamp, int count, long *timeout )
{
	int ts_good;
	OSTimer *timer = timer_factory->createTimer();
	long wait_time = 0;

	XPTPD_INFO( "getTxTimestampRetry::count=%d", count );
	ts_good =
		getTxTimestamp
		( msg, timestamp, false );
	XPTPD_INFO( "ts_good=%d", count == 0 );
	while (ts_good != 0 && count >= 0) {
		XPTPD_INFO
			("Error (TX) Timestamp (Retrying-%d), error=%d", count, ts_good);
		timer->sleep(*timeout);
		wait_time += *timeout;
		ts_good = getTxTimestamp
			( msg, timestamp, count == 0 );
		*timeout *= 2;
		--count;
	}

	delete timer;
	*timeout = wait_time;
	return ts_good == 0 ? true : false;
}

bool MediaDependentEtherPort::getRxTimestampRetry
(PTPMessageCommon *msg, Timestamp & timestamp, int count, long timeout )
{
	int ts_good;
	OSTimer *timer = timer_factory->createTimer();

	XPTPD_INFO( "getRxTimestampRetry::count=%d", count );
	ts_good =
		getRxTimestamp
		( msg, timestamp, false);
	XPTPD_INFO( "ts_good=%d", count == 0 );
	while (ts_good != 0 && count >= 0) {
		XPTPD_INFO
			("Error (RX) Timestamp (Retrying-%d), error=%d", count, ts_good);
		timer->sleep(timeout);
		ts_good = getRxTimestamp
			( msg, timestamp, count == 0 );
		timeout *= 2;
		--count;
	}

	delete timer;
	return ts_good == 0 ? true : false;
}

int MediaDependentEtherPort::getTxTimestamp
(PTPMessageCommon * msg, Timestamp & timestamp, bool last)
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getTxTimestamp
		( &identity, msg->getSequenceId(), timestamp, last );
}

int MediaDependentEtherPort::getRxTimestamp
( PTPMessageCommon * msg, Timestamp & timestamp, bool last)
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	if( msg != NULL ) {
		return getRxTimestamp
			( &identity, msg->getSequenceId(), timestamp, last );
	} else {
		return getRxTimestamp
			( NULL, 0, timestamp, last );
	}
}

int MediaDependentEtherPort::getTxTimestamp
(PortIdentity * sourcePortIdentity, uint16_t sequenceId, Timestamp & timestamp,
 bool last)
{
	EthernetTimestamper *timestamper_l =
		dynamic_cast<EthernetTimestamper *>(_hw_timestamper);
	if (timestamper_l) {
		return timestamper_l->HWTimestamper_txtimestamp
		    (sourcePortIdentity, sequenceId, timestamp, last);
	}
	timestamp = getClock()->getSystemTime();
	return 0;
}

int MediaDependentEtherPort::getRxTimestamp
(PortIdentity * sourcePortIdentity, uint16_t sequenceId,
 Timestamp & timestamp, bool last)
{
	EthernetTimestamper *timestamper_l =
		dynamic_cast<EthernetTimestamper *>(_hw_timestamper);
	if (timestamper_l) {
		return timestamper_l->HWTimestamper_rxtimestamp
		    (sourcePortIdentity, sequenceId, timestamp, last);
	}
	timestamp = getClock()->getSystemTime();
	return 0;
}

void *MediaDependentEtherPort::watchNetLink(void)
{
	// Clean-up should be done by network interface code
	// Runs until interface is shutdown, error occurs, or unimplemented
        net_iface->watchNetLink(this);

	// Raise event indicating exit
	port->processEvent( LINKWATCH_EXIT );

        return NULL;
}
