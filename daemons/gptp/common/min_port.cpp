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

#include <min_port.hpp>
#include <avbts_message.hpp>
#include <avbts_clock.hpp>

#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_oscondition.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>

#include <utility>

LinkLayerAddress MediaIndependentPort::other_multicast(OTHER_MULTICAST);

MediaIndependentPort::
MediaIndependentPort(IEEE1588PortInit_t *portInit)
{
	announce_sequence_id = 0;
	sync_sequence_id = 0;
	pdelay_sequence_id = 0;
	signal_sequence_id = 0;

	clock = portInit->clock;
	ifindex = portInit->ifindex;

	automotive_profile = portInit->automotive_profile;
	testMode = portInit->testMode;
        initialLogSyncInterval = portInit->initialLogSyncInterval;
        initialLogPdelayReqInterval = portInit->initialLogPdelayReqInterval;
        operLogPdelayReqInterval = portInit->operLogPdelayReqInterval;
        operLogSyncInterval = portInit->operLogSyncInterval;

        if (automotive_profile) {
                asCapable = true;

                if (initialLogSyncInterval == LOG2_INTERVAL_INVALID)
                        initialLogSyncInterval = -5;     // 31.25 ms
                if (initialLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
                        initialLogPdelayReqInterval = 0;  // 1 second
                if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
                        operLogPdelayReqInterval = 0;      // 1 second
                if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
                        operLogSyncInterval = 0;           // 1 second
        }
        else {
                asCapable = false;

                if (initialLogSyncInterval == LOG2_INTERVAL_INVALID)
                        initialLogSyncInterval = -3;       // 125 ms
                if (initialLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
                        initialLogPdelayReqInterval = 0;   // 1 second
                if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
                        operLogPdelayReqInterval = 0;      // 1 second
                if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
                        operLogSyncInterval = 0;           // 1 second
        }

	port_state = PTP_INITIALIZING;

	pdelay_started = false;
	pdelay_halted = false;
	sync_rate_interval_timer_started = false;

        duplicate_resp_counter = 0;
        last_invalid_seqid = 0;

        log_mean_sync_interval = portInit->initialLogSyncInterval;
        log_mean_announce_interval = portInit->initialLogAnnounceInterval;
        log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;

	last_pdelay_req = NULL;

	this->clock = clock;

	port_state = PTP_INITIALIZING;

	_syntonize = portInit->syntonize;

	qualified_announce = NULL;

	_peer_rate_offset = 1.0;
	_peer_offset_init = false;

	this->timer_factory = portInit->timer_factory;
	this->thread_factory = portInit->thread_factory;

	this->condition_factory = portInit->condition_factory;
	this->lock_factory = portInit->lock_factory;

	pdelay_count = 0;
	sync_count = 0;

}

MediaIndependentPort::~MediaIndependentPort()
{
	if( qualified_announce != NULL ) delete qualified_announce;

	delete port_ready_condition;
}

bool MediaIndependentPort::serializeState( void *buf, off_t *count ) {
	bool ret = true;

	if( buf == NULL ) {
		*count = sizeof(port_state)+sizeof(_peer_rate_offset)+
			sizeof(asCapable);
		return true;
	}

	if( lock() != oslock_ok ) return false;

	if( port_state != PTP_MASTER && port_state != PTP_SLAVE ) {
		*count = 0;
		ret = false;
		goto bail;
	}

	/* asCapable */
	if( ret && *count >= (off_t) sizeof( asCapable )) {
		memcpy( buf, &asCapable, sizeof( asCapable ));
		*count -= sizeof( asCapable );
		buf = ((char *)buf) + sizeof( asCapable );
	} else if( ret == false ) {
		*count += sizeof( asCapable );
	} else {
		*count = sizeof( asCapable )-*count;
		ret = false;
	}

	/* Port State */
	if( ret && *count >= (off_t) sizeof( port_state )) {
		memcpy( buf, &port_state, sizeof( port_state ));
		*count -= sizeof( port_state );
		buf = ((char *)buf) + sizeof( port_state );
	} else if( ret == false ) {
		*count += sizeof( port_state );
	} else {
		*count = sizeof( port_state )-*count;
		ret = false;
	}

	/* Neighbor Rate Ratio */
	if( ret && *count >= (off_t) sizeof( _peer_rate_offset )) {
		memcpy( buf, &_peer_rate_offset, sizeof( _peer_rate_offset ));
		*count -= sizeof( _peer_rate_offset );
		buf = ((char *)buf) + sizeof( _peer_rate_offset );
	} else if( ret == false ) {
		*count += sizeof( _peer_rate_offset );
	} else {
		*count = sizeof( _peer_rate_offset )-*count;
		ret = false;
	}

	ret = inferior->serializeState( buf, count );

 bail:
	if( unlock() != oslock_ok ) return false;
	return ret;
}

bool MediaIndependentPort::restoreSerializedState( void *buf, off_t *count ) {
	bool ret = true;

	/* asCapable */
	if( ret && *count >= (off_t) sizeof( asCapable )) {
		memcpy( &asCapable, buf, sizeof( asCapable ));
		*count -= sizeof( asCapable );
		buf = ((char *)buf) + sizeof( asCapable );
	} else if( ret == false ) {
		*count += sizeof( asCapable );
	} else {
		*count = sizeof( asCapable )-*count;
		ret = false;
	}

	/* Port State */
	if( ret && *count >= (off_t) sizeof( port_state )) {
		memcpy( &port_state, buf, sizeof( port_state ));
		*count -= sizeof( port_state );
		buf = ((char *)buf) + sizeof( port_state );
	} else if( ret == false ) {
		*count += sizeof( port_state );
	} else {
		*count = sizeof( port_state )-*count;
		ret = false;
	}

	/* Neighbor Rate Ratio */
	if( ret && *count >= (off_t) sizeof( _peer_rate_offset )) {
		memcpy( &_peer_rate_offset, buf, sizeof( _peer_rate_offset ));
		*count -= sizeof( _peer_rate_offset );
		buf = ((char *)buf) + sizeof( _peer_rate_offset );
	} else if( ret == false ) {
		*count += sizeof( _peer_rate_offset );
	} else {
		*count = sizeof( _peer_rate_offset )-*count;
		ret = false;
	}

	ret = inferior->restoreSerializedState( buf, count );

	return ret;
}

void MediaIndependentPort::syncDone()
{
	GPTP_LOG_VERBOSE("Sync complete");

	if (automotive_profile && port_state == PTP_SLAVE)
	{
		if (avbSyncState > 0)
		{
			avbSyncState--;
			if (avbSyncState == 0)
			{
				setStationState(STATION_STATE_AVB_SYNC);
				if (testMode)
				{
					APMessageTestStatus
						*testStatusMsg = new
						APMessageTestStatus
						(this);
					if (testStatusMsg)
					{
						testStatusMsg
							->sendPort
							(this);
						delete testStatusMsg;
					}
				}
			}
		}
	}

	if (automotive_profile)
	{
		if (!sync_rate_interval_timer_started)
		{
			if (log_mean_sync_interval != operLogSyncInterval)
			{
				startSyncRateIntervalTimer();
			}
		}
	}

	if( !pdelay_started )
	{
		startPDelay();
	}
}

void MediaIndependentPort::startAnnounce() {
        if (!automotive_profile) {
                startAnnounceIntervalTimer(16000000);
        }
}

bool MediaIndependentPort::init_port( IEEE1588PortInit_t *portInit )
{
	glock = lock_factory->createLock( oslock_nonrecursive );
	if( glock == NULL )
		return false;

	if( !clock->registerPort(this, ifindex, portInit))
        {
                GPTP_LOG_ERROR( "Unable to register port, index=%d\n",
                                ifindex );
                return false;
        }

	if( !inferior->init_port( this )) return false;

        syncReceiptTimerLock = lock_factory->createLock(oslock_recursive);
        syncIntervalTimerLock = lock_factory->createLock(oslock_recursive);
        announceIntervalTimerLock = lock_factory->createLock(oslock_recursive);
        pDelayIntervalTimerLock = lock_factory->createLock(oslock_recursive);

	port_identity.setClockIdentity(clock->getClockIdentity());
	port_identity.setPortNumber(&ifindex);

	return true;
}

void IEEE1588Port::startPDelayIntervalTimer(long long unsigned int waitTime)
{
        pDelayIntervalTimerLock->lock();
        clock->deleteEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
        clock->addEventTimerLocked
		(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, waitTime);
        pDelayIntervalTimerLock->unlock();
}

void MediaIndependentPort::startPDelay() {
	long long unsigned int waitTime;
	waitTime = ((long long)
		    (pow((double)2,
			 log_min_mean_pdelay_req_interval) * 1000000000.0));

        if(!pdelayHalted()) {
                if (automotive_profile) {
                        if ( log_min_mean_pdelay_req_interval !=
			     PTPMessageSignalling::sigMsgInterval_NoSend )
			{
                                waitTime = waitTime > EVENT_TIMER_GRANULARITY
					? waitTime : EVENT_TIMER_GRANULARITY;
                                pdelay_started = true;
                                startPDelayIntervalTimer(waitTime);
                        }
                } else
		{
                        pdelay_started = true;
                        startPDelayIntervalTimer(32000000);
                }
        }
}

void MediaIndependentPort::stopPDelay() {
        haltPdelay(true);
        pdelay_started = false;
        clock->deleteEventTimerLocked( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
}

void MediaIndependentPort::startSyncRateIntervalTimer() {
        if (automotive_profile) {
                sync_rate_interval_timer_started = true;
                if ( clock->isGM() == true )
		{
                        // GM will wait up to 8  seconds for signaling rate
                        // TODO: This isn't according to spec but set because
			// it is believed that some slave devices aren't
			// signalling to reduce the rate
                        clock->addEventTimerLocked
				( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED,
				  8000000000 );
                } else
		{
                        // Slave will time out after 4 seconds
                        clock->addEventTimerLocked
				( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED,
				  4000000000 );
                }
        }
}


void MediaIndependentPort::addQualifiedAnnounce(PTPMessageAnnounce *msg) {
	if( qualified_announce != NULL ) delete qualified_announce;
	qualified_announce = msg;
}

bool MediaIndependentPort::openPort(void)
{
	bool ret;
	OSCondition *port_ready_condition =
		condition_factory->createCondition();

	ret = inferior->openPort( port_ready_condition );
	delete port_ready_condition;

	return ret;
}

bool MediaIndependentPort::recoverPort() {
	return inferior->recoverPort();
}

void MediaIndependentPort::startPDelay() {
	long long unsigned int waitTime;
	waitTime = ((long long)
		    (pow(( double ) 2,
			 log_min_mean_pdelay_req_interval) * 1000000000.0 ));

        if(!pdelayHalted()) {
                if (automotive_profile) {
                        if ( log_min_mean_pdelay_req_interval !=
			     PTPMessageSignalling::sigMsgInterval_NoSend)
			{
                                waitTime = waitTime > EVENT_TIMER_GRANULARITY
					? waitTime : EVENT_TIMER_GRANULARITY;
                                pdelay_started = true;
                                startPDelayIntervalTimer(waitTime);
                        }
                }
                else {
                        pdelay_started = true;
                        startPDelayIntervalTimer(32000000);
                }
        }
}


void MediaIndependentPort::startAnnounce() {
        if (!automotive_profile) {
                startAnnounceIntervalTimer(16000000);
        }
}

void MediaIndependentPort::setAsCapable(bool ascap)
{
	if (ascap != asCapable)
	{
		GPTP_LOG_STATUS( "AsCapable: %s", ascap == true ?
				 "Enabled" : "Disabled" );
	}

	if(!ascap)
	{
		_peer_offset_init = false;
	}

	asCapable = ascap;
}

void MediaIndependentPort::stopSyncReceiptTimer(void)
{
        syncReceiptTimerLock->lock();
        clock->deleteEventTimerLocked( this, SYNC_RECEIPT_TIMEOUT_EXPIRES );
        syncReceiptTimerLock->unlock();
}

void MediaIndependentPort::startSyncIntervalTimer(long long unsigned int waitTime)
{
        syncIntervalTimerLock->lock();
        clock->deleteEventTimerLocked( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );
        clock->addEventTimerLocked
		( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, waitTime);
        syncIntervalTimerLock->unlock();
}

void MediaIndependentPort::startSyncRateIntervalTimer()
{
        if (automotive_profile) {
                sync_rate_interval_timer_started = true;
                if (isGM) {
                        // GM will wait up to 8  seconds for signaling rate
                        // TODO: This isn't according to spec but set because
			// it is believed that some slave devices aren't
			// signalling to reduce the rate
                        clock->addEventTimerLocked
				( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED,
				  8000000000 );
                } else
		{
                        // Slave will time out after 4 seconds
                        clock->addEventTimerLocked
				( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED,
				  4000000000 );
                }
        }
}

bool MediaIndependentPort::processStateChangeEvent()
{
	PTPMessageAnnounce *EBest = NULL;
	MinPortListIterator curr = clock->getPortListBegin();
	MinPortListIterator end = clock->getPortListEnd();
	uint8_t EBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
	uint8_t LastEBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
	bool changed_external_master;

	/* Find EBest for all ports */
	for ( ;curr < end; ++curr)
	{
		PTPMessageAnnounce *annc;
		if
			((*curr)->port_state == PTP_DISABLED ||
			 (*curr)->port_state == PTP_FAULTY )
		{
			continue;
		}
		annc = (*curr)->calculateERBest();

		if ( EBest == NULL )
		{
			EBest = annc;
		} else if( annc != NULL )
		{
			if( annc->isBetterThan(EBest) )
			{
				EBest = (*curr)->calculateERBest();
			}
		}
	}

	if( EBest == NULL )
		return false;

	/* Check if we've changed */
	clock->getLastEBestIdentity().getIdentityString
		( LastEBestClockIdentity );
	EBest->getGrandmasterIdentity( EBestClockIdentity );

	if( memcmp( EBestClockIdentity, LastEBestClockIdentity,
		    PTP_CLOCK_IDENTITY_LENGTH ) != 0 )
	{
		ClockIdentity newGM;
		changed_external_master = true;
		newGM.set((uint8_t *) EBestClockIdentity );
		clock->setLastEBestIdentity( newGM );
	} else
	{
		changed_external_master = false;
	}

	if( clock->isBetterThan( EBest )) {
		// We're Grandmaster
		// set grandmaster info to me
		ClockIdentity clock_identity;
		unsigned char priority1;
		unsigned char priority2;
		ClockQuality clock_quality;

		clock_identity = getClock()->getClockIdentity();
		getClock()->setGrandmasterClockIdentity( clock_identity );
		priority1 = getClock()->getPriority1();
		getClock()->setGrandmasterPriority1( priority1 );
		priority2 = getClock()->getPriority2();
		getClock()->setGrandmasterPriority2( priority2 );
		clock_quality = getClock()->getClockQuality();
		getClock()->setGrandmasterClockQuality( clock_quality );

		getClock()->setGM();
	} else {
		getClock()->clearGM();
	}

	curr = clock->getPortListBegin();
	for ( ;curr < end; ++curr)
	{
		if((*curr)->port_state == PTP_DISABLED ||
		   (*curr)->port_state == PTP_FAULTY)
		{
			continue;
		}

		if( clock->isGM() ) {
			(*curr)->recommendState(PTP_MASTER,
						changed_external_master );
		} else {
			if( EBest == (*curr)->calculateERBest() )
			{
				// The "best" Announce was
				// recieved on this port
				ClockIdentity clock_identity;
				unsigned char priority1;
				unsigned char priority2;
				ClockQuality *clock_quality;

				(*curr)->recommendState
					( PTP_SLAVE, changed_external_master );
				clock_identity =
					EBest->getGrandmasterClockIdentity();
				getClock()->setGrandmasterClockIdentity
					(clock_identity);
				priority1 = EBest->getGrandmasterPriority1();
				getClock()->setGrandmasterPriority1
					( priority1 );
				priority2 = EBest->getGrandmasterPriority2();
				getClock()->setGrandmasterPriority2
					( priority2 );
				clock_quality =
					EBest->getGrandmasterClockQuality();
				getClock()->setGrandmasterClockQuality
					(*clock_quality);
			} else {
				/* Otherwise we are the master because we have
				   sync'd to a better clock */
				(*curr)->recommendState
					(PTP_MASTER, changed_external_master);
			}
		}
	}

	return true;
}

bool MediaIndependentPort::processSyncAnncReceiptTimeout( void )
{
	if( clock->getPriority1() == SLAVE_ONLY_PRIO ) {
		// Restart timer
		if( e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ) {
			clock->addEventTimer
				( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
				  (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
				   (unsigned long long)
				   (pow((double)2,getAnnounceInterval())*
				    1000000000.0 )));
		} else {
			clock->addEventTimer
				(this, SYNC_RECEIPT_TIMEOUT_EXPIRES,
				 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER*
				  (unsigned long long)
				  (pow((double)2,getSyncInterval())*
				   1000000000.0)));
		}
		goto receipt_timeout_bail;
	}

	if (port_state == PTP_INITIALIZING ||
	    port_state == PTP_UNCALIBRATED ||
	    port_state == PTP_SLAVE ||
	    port_state == PTP_PRE_MASTER )
	{
		// We're Grandmaster, set grandmaster info to me
		ClockIdentity clock_identity;
		unsigned char priority1;
		unsigned char priority2;
		ClockQuality clock_quality;

		Timestamp system_time;
		Timestamp device_time;

		GPTP_LOG_STATUS(
			"*** %s Timeout Expired - Becoming Master",
			e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ? "Announce" :
			"Sync" );

		clock_identity = getClock()->getClockIdentity();
		getClock()->setGrandmasterClockIdentity
			( clock_identity );
		priority1 = getClock()->getPriority1();
		getClock()->setGrandmasterPriority1( priority1 );
		priority2 = getClock()->getPriority2();
		getClock()->setGrandmasterPriority2( priority2 );
		clock_quality = getClock()->getClockQuality();
		getClock()->setGrandmasterClockQuality
			( clock_quality );

		getClock()->setGrandmaster();

		port_state = PTP_MASTER;

		inferior->getDeviceTime( system_time, device_time );
		clock->calcLocalSystemClockRateDifference
			( device_time, system_time );

		delete qualified_announce;
		qualified_announce = NULL;

		// Add timers for Announce and Sync,
		// this is as close to immediately as we get
		clock->addEventTimer
			( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
		if( clock->getPriority1() != SLAVE_ONLY_PRIO ) {
			clock->addEventTimerLocked
				( this, SYNC_INTERVAL_TIMEOUT_EXPIRES,
				  16000000 );
		}
		startAnnounce();
	}
}

bool processSyncIntervalTimeout( void )
{
	Timestamp system_time;
	Timestamp device_time;
	unsigned long long interval;
	long elapsed_time = 0;
	bool grandmaster = getClock()->getGrandmaster();

	GPTP_LOG_DEBUG("SYNC_INTERVAL_TIMEOUT_EXPIRES occured");
	if( asCapable && (clock->isGM() ||
			  getClock()->checkLastSyncInfo( NULL )))
	{
		inferior->processSync
			(getNextSyncSequenceId(), grandmaster, &elapsed_time);
	}

	/* If accelerated_sync is non-zero then start 16 ms sync
	   timer, subtract 1, for last one start PDelay also */

	syncDone();
	if( _accelerated_sync_count == 0 )
	{
		startAnnounce();
		--_accelerated_sync_count;
	}

	interval = (uint64_t) (pow((double)2,getSyncInterval())*1000000000.0);
	interval -= elapsed_time*1000ULL;
	interval =
		interval < EVENT_TIMER_GRANULARITY ? EVENT_TIMER_GRANULARITY :
		interval;
	clock->addEventTimer
		( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, interval );

}


bool MediaIndependentPort::processEvent(Event e)
{
	switch (e)
	{
	case POWERUP:
	case INITIALIZE:
		GPTP_LOG_DEBUG("Received POWERUP/INITIALIZE event");
		lock();
		inferior->lock();

		{
			unsigned long long interval;
			Event e = NULL_EVENT;

                        if ( !automotive_profile )
			{
                                if ( port_state != PTP_SLAVE &&
				     port_state != PTP_MASTER )
				{
                                        GPTP_LOG_STATUS("Starting PDelay");
                                        startPDelay();
                                }
                        }
                        else
			{
                                startPDelay();
                        }

			if( clock->getPriority1() == 255 ||
			    port_state == PTP_SLAVE )
			{
				becomeSlave( true );
			} else if( port_state == PTP_MASTER )
			{
				becomeMaster( true );
			} else
			{
                                e = ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES;

                                interval = (unsigned long long)
                                        (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
                                         pow((double)2,getAnnounceInterval())*
					 1000000000.0);
			}

			port_ready_condition->wait_prelock();

			inferior->openPort( port_ready_condition );
			port_ready_condition->wait();

			if (e != NULL_EVENT)
				clock->addEventTimer( this, e, interval );
		}

		if (automotive_profile)
		{
			setStationState(STATION_STATE_ETHERNET_READY);
			if (testMode)
			{
                                APMessageTestStatus *testStatusMsg =
					new APMessageTestStatus(this);
                                if (testStatusMsg)
				{
                                        testStatusMsg->sendPort(this);
                                        delete testStatusMsg;
                                }
                        }

                        if (!isGM)
			{
				uint8 msg_interval = PTPMessageSignalling::
					sigMsgInterval_NoSend;
                                // Send an initial signalling message
                                PTPMessageSignalling *sigMsg =
					new PTPMessageSignalling(this);
                                if (sigMsg)
				{
                                        sigMsg->setintervals
						( msg_interval,
						  log_mean_sync_interval,
						  msg_interval );
                                        sigMsg->sendPort( inferior, NULL );
                                        delete sigMsg;
                                }

                                startSyncReceiptTimer((unsigned long long)
                                         (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
                                          ((double)
					   pow((double)2, getSyncInterval()) *
                                           1000000000.0)));
                        }
                }

		inferior->unlock();
		unlock();

		break;
	case STATE_CHANGE_EVENT:
		clock->lock();
		lock();
		inferior->lock();

		if( !automotive_profile &&
		    clock->getPriority != SLAVE_ONLY_PRIO )
			processStateChangeEvent();

		inferior->unlock();
		unlock();
		clock->unlock();

		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		clock->lock();
		lock();
		inferior->lock();

		if (e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES)
		{
			incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts();
		} else
		{
			incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts();
		}

		if( !automotive_profile )
		{
			processSyncAnncReceiptTimeout();
		} else if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES)
		{
			GPTP_LOG_EXCEPTION("SYNC receipt timeout");

			startSyncReceiptTimer
				((unsigned long long)
				 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
				  ((double) pow((double)2,
						getSyncInterval()) *
				   1000000000.0)));
		}

		inferior->unlock();
		unlock();
		clock->unlock();

		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		XPTPD_INFOL(PDELAY_DEBUG,"PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");

		lock();
		inferior->lock();

		{
			long elapsed_time = 0;
			unsigned long long interval;

			inferior->processPDelay( getNextPDelaySequenceId(), &elapsed_time );

			interval = (uint64_t) (PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER*
						(pow((double)2,getPDelayInterval())*1000000000.0));
			interval -= elapsed_time*1000ULL;
			interval = interval < EVENT_TIMER_GRANULARITY ? EVENT_TIMER_GRANULARITY : interval;
			clock->addEventTimer( this, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, interval );
			interval = (uint64_t) (pow((double)2,getPDelayInterval())*1000000000.0);
			interval -= elapsed_time*1000ULL;
			interval = interval < EVENT_TIMER_GRANULARITY ? EVENT_TIMER_GRANULARITY : interval;
			XPTPD_INFO("Re-add PDelay Event Timer");
			clock->addEventTimer(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, interval);
		}

		inferior->unlock();
		unlock();

		break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		clock->lock();
		lock();
		inferior->lock();

		{
		}

		inferior->unlock();
		unlock();
		clock->unlock();
		break;
	case ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES:
		XPTPD_INFOL
			(ANNOUNCE_DEBUG,"Announce interval expired on port %s "
			 "(AsCapable=%s)", inferior->getNetLabel()->toString(),
			 asCapable ? "true" : "false" );

		clock->lock();
		lock();

		if (asCapable) {
			// Send an announce message
			PTPMessageAnnounce *annc =
				new PTPMessageAnnounce(this);
			PortIdentity dest_id;
			PortIdentity gmId;
			ClockIdentity clock_id = clock->getClockIdentity();
			gmId.setClockIdentity(clock_id);
			getPortIdentity(dest_id);
			annc->setPortIdentity(&dest_id);
			annc->sendPort(inferior);
			XPTPD_INFOL
				(ANNOUNCE_DEBUG,"Sent announce message on port %s",
				 inferior->getNetLabel()->toString() );
			delete annc;
		}
		clock->addEventTimer
			(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
			 (unsigned)
			 (pow ((double)2, getAnnounceInterval()) * 1000000000.0));

		unlock();
		clock->unlock();

		break;
	case FAULT_DETECTED:
		XPTPD_INFO("Received FAULT_DETECTED event");
		break;
	case PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES:
		lock();
		pdelay_count = 0;
		unsetAsCapable();
		unlock();
		break;
	default:
		if( !inferior->processEvent( e )) {
			XPTPD_INFO
				("Unhandled event type in "
				 "MediaIndependentPort::processEvent(), %d", e );
			return false;
		}
		break;
	}

	return true;
}

PTPMessageAnnounce *MediaIndependentPort::calculateERBest(void)
{
	return qualified_announce;
}


bool MediaIndependentPort::adjustPhaseError
( int64_t master_local_offset, FrequencyRatio master_local_freq_offset )
{
	long double phase_error;
	if( _new_syntonization_set_point ) {
		bool result;
		if( !inferior->suspendTransmissionAll() ) goto bail;
		result = inferior->adjustClockPhase( -master_local_offset );
		if( !inferior->resumeTransmissionAll() || !result ) goto bail;
		_new_syntonization_set_point = false;
		master_local_offset = 0;
	}

	// Adjust for frequency offset
	phase_error = (long double) -master_local_offset;
	_ppm += (float) (INTEGRAL*phase_error +
					 PROPORTIONAL*((master_local_freq_offset-1.0)*1000000));
	if( _ppm < LOWER_FREQ_LIMIT ) _ppm = LOWER_FREQ_LIMIT;
	if( _ppm > UPPER_FREQ_LIMIT ) _ppm = UPPER_FREQ_LIMIT;
	if( !inferior->adjustClockRate( _ppm )) {
		XPTPD_ERROR( "Failed to adjust clock rate" );
		return false;
	}

	return true;

 bail:
	return false;
}

void MediaIndependentPort::becomeMaster( bool annc ) {
	port_state = PTP_MASTER;
	// Start announce receipt timeout timer
	// Start sync receipt timeout timer
	clock->deleteEventTimer( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES );
	stopSyncReceiptTimeout();
	if( annc ) {
		startAnnounce();
	}
	clock->addEventTimer( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
	XPTPD_WDEBUG("Switching to Master" );

	return;
}

void MediaIndependentPort::becomeSlave( bool restart_syntonization ) {
	port_state = PTP_SLAVE;
	clock->deleteEventTimer( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->deleteEventTimer( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );
	//startSyncReceiptTimeout();
	clock->addEventTimer
		(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
		  (unsigned long long)
		  (pow((double)2,getAnnounceInterval())*1000000000.0)));
	XPTPD_WDEBUG("Switching to Slave" );
	if( restart_syntonization ) newSyntonizationSetPoint();

	return;
}

void MediaIndependentPort::recommendState
( PortState state, bool changed_external_master )
{
	bool reset_sync = false;
	switch (state) {
	case PTP_MASTER:
		if (port_state != PTP_MASTER) {
			port_state = PTP_MASTER;
			// Start announce receipt timeout timer
			// Start sync receipt timeout timer
			becomeMaster( true );
			reset_sync = true;
		}
		break;
	case PTP_SLAVE:
		if (port_state != PTP_SLAVE) {
			becomeSlave( true );
			reset_sync = true;
		} else {
			if( changed_external_master ) {
				XPTPD_WDEBUG("Changed master!\n" );
				newSyntonizationSetPoint();
				reset_sync = true;
			}
		}
		break;
	default:
		XPTPD_INFO
		    ("Invalid state change requested by call to "
			 "1588Port::recommendState()");
		break;
	}
	if( reset_sync ) sync_count = 0;
	return;
}

bool MediaIndependentPort::stop() {
	bool ret;
	glock->lock();
	ret = clock->stop_port(this);
	glock->unlock();
	return ret;
}
