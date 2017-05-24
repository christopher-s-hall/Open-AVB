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
#include <avbts_message.hpp>
#include <ether_port.hpp>
#include <avbts_ostimer.hpp>
#include <ether_tstamper.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>

PTPMessageCommon::PTPMessageCommon( CommonPort *port )
{
	// Fill in fields using port/clock dataset as a template
	versionPTP = GPTP_VERSION;
	versionNetwork = PTP_NETWORK_VERSION;
	domainNumber = port->getClock()->getDomain();
	// Set flags as necessary
	memset( &flags, 0, sizeof( flags ));
	flags.ptpTimescale = 0x1; // Reserved "TRUE"
	correctionField = 0;
	_gc = false;

	return;
}

/* Determine whether the message was sent by given communication technology,
   uuid, and port id fields */
bool PTPMessageCommon::isSenderEqual(PortIdentity portIdentity)
{
	return portIdentity == sourcePortIdentity;
}

PTPMessageCommon *buildPTPMessageEvent
( PTPMessageEvent *msg,  CommonPort *cport, char *buf, int size )
{
	int iter = 5;
	long req = 4000;	// = 1 ms
	int ts_good;

	PTPMessageId messageId;
	Timestamp timestamp;
	unsigned counter_value = 0;
	OSTimer *timer;

	EtherPort *port = dynamic_cast<decltype(port)>(cport);
	if( port == NULL )
	{
		GPTP_LOG_ERROR( "Received event message on port that doesn't "
				"support it" );
		return NULL;
	}

	messageId.setMessageType(msg->messageType);
	messageId.setSequenceId(msg->sequenceId);

	GPTP_LOG_VERBOSE("Timestamping event packet");
	timer = port->getTimerFactory()->createTimer();
	ts_good = port->getRxTimestamp( &msg->sourcePortIdentity, messageId,
					timestamp, counter_value, false );
	while (ts_good != GPTP_EC_SUCCESS && iter-- != 0) {
		// Waits at least 1 time slice regardless of size of 'req'
		timer->sleep(req);
		if (ts_good != GPTP_EC_EAGAIN)
			GPTP_LOG_ERROR(
				"Error (RX) timestamping RX event packet "
				"(Retrying), error=%d", ts_good );
		ts_good = port->getRxTimestamp
			( &msg->sourcePortIdentity, messageId, timestamp,
			  counter_value, iter == 0 );
		req *= 2;
	}
	delete timer;

	if (ts_good != GPTP_EC_SUCCESS)
	{
		char err_msg[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
		port->getExtendedError(err_msg);
		GPTP_LOG_ERROR
			("*** Received an event packet but cannot retrieve "
			 "timestamp, discarding. messageType=%u,error=%d\n%s",
			 msg->messageType, ts_good, msg);
		goto abort;
	}

	msg->_timestamp = timestamp;
	msg->_timestamp_counter_value = counter_value;

	switch (msg->messageType) {
	default:
		GPTP_LOG_EXCEPTION( "Received unsupported event message "
				    "type, %d", (int)msg->messageType );
		port->incCounter_ieee8021AsPortStatRxPTPPacketDiscard();

		goto abort;

	case SYNC_MESSAGE:
		GPTP_LOG_DEBUG("*** Received Sync message" );
		GPTP_LOG_VERBOSE("Sync RX timestamp = %s",
				 timestamp.toString().c_str());

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH) {
			GPTP_LOG_ERROR("Received wrong length sync message" );
			goto abort;
		}
		{
			auto sync_msg = new PTPMessageSync();
			*((PTPMessageEvent *) sync_msg) = *msg;

			msg = sync_msg;
		}
		break;

	case PATH_DELAY_REQ_MESSAGE:
		GPTP_LOG_DEBUG("*** Received PDelay Request message");

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH
		    && /* For Broadcom compatibility */ size != 46) {
			goto abort;
		}
		{
			auto pdelay_req_msg =
				new PTPMessagePathDelayReq();
			*((PTPMessageEvent *) pdelay_req_msg) = *msg;

			msg = pdelay_req_msg;
		}
		break;

	case PATH_DELAY_RESP_MESSAGE:
		GPTP_LOG_DEBUG
			( "*** Received PDelay Response message, "
			  "Timestamp %s, seqID %u",
			  timestamp.toString().c_str(), msg->sequenceId );

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH) {
			goto abort;
		}
		{
			pdelay_resp_msg_t *fields = (decltype(fields))
				(buf + PTP_PDELAY_RESP_OFFSET);
			auto pdelay_resp_msg =
				new PTPMessagePathDelayResp();
			*((PTPMessageEvent *) pdelay_resp_msg) = *msg;

			pdelay_resp_msg->requestReceiptTimestamp =
				fields->requestReceiptTimestamp;
			pdelay_resp_msg->requestReceiptTimestamp.ntoh();

			pdelay_resp_msg->requestingPortIdentity =
				fields->requestingPortIdentity;
			pdelay_resp_msg->
				requestingPortIdentity.ntoh();

			msg = pdelay_resp_msg;
		}
		break;
	}

	msg->_gc = false;
	return msg;

abort:
	return NULL;
}

PTPMessageCommon *buildPTPMessageCommon
( char *buf, int size, LinkLayerAddress *remote,
  CommonPort *port )
{
	PTPMessageCommon *msg = NULL;

	common_header_t *header = (common_header_t *) buf;

#if PTP_DEBUG
	{
		int i;
		GPTP_LOG_VERBOSE("Packet Dump:\n");
		for (i = 0; i < size; ++i) {
			GPTP_LOG_VERBOSE("%hhx\t", buf[i]);
			if (i % 8 == 7)
				GPTP_LOG_VERBOSE("\n");
		}
		if (i % 8 != 0)
			GPTP_LOG_VERBOSE("\n");
	}
#endif

	msg = new PTPMessageCommon();
	msg->messageType = (MessageType) header->messageType;
	msg->sequenceId = PLAT_ntohs( header->sequenceId );
	msg->sourcePortIdentity = header->sourcePortIdentity;
	msg->sourcePortIdentity.ntoh();
	msg->versionPTP = header->versionPTP;
	msg->messageLength = PLAT_ntohs( header->messageLength );
	msg->domainNumber = header->domainNumber;
	msg->correctionField = PLAT_ntohll( header->correctionField );
	msg->control = (LegacyMessageType) header->control;

	GPTP_LOG_VERBOSE("Captured Sequence Id: %u", msg->sequenceId);

	if ( GPTP_TRANSPORT_SPECIFIC != header->transportSpecific ) {
		GPTP_LOG_EXCEPTION
			( "*** Received message with unsupported "
			  "transportSpecific type=%d",
			  header->transportSpecific );
		goto abort;
	}

	if( msg->isEvent() )
	{
		auto event_msg = new PTPMessageEvent();
		*((PTPMessageCommon *) event_msg) = *msg;

		return buildPTPMessageEvent( event_msg, port, buf, size );
	}

	switch (msg->messageType) {
	case FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received Follow Up message");

		// Be sure buffer is the correction size
		if (size < (int)(PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH))
		{
			GPTP_LOG_ERROR( "Received wrong length followup "
					"message" );
			goto abort;
		}
		{
			followup_msg_t *fields = (decltype(fields))
				(buf + PTP_FOLLOWUP_OFFSET);
			auto followup_msg = new PTPMessageFollowUp();
			*((PTPMessageCommon *) followup_msg) = *msg;

			followup_msg->preciseOriginTimestamp =
				fields->preciseOriginTimestamp;
			followup_msg->preciseOriginTimestamp.ntoh();
			followup_msg->tlv = fields->tlv;

			msg = followup_msg;
		}

		break;
	case PATH_DELAY_FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Response FollowUp message");

		// Be sure buffer is the correction size
//     if( size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_FWUP_LENGTH ) {
//       goto abort;
//     }
		{
			pdelay_fwup_msg_t *fields = (decltype(fields))
				(buf + PTP_PDELAY_FWUP_OFFSET);
			auto pdelay_fwup_msg =
			    new PTPMessagePathDelayRespFollowUp();
			*((PTPMessageCommon *) pdelay_fwup_msg) = *msg;
			// Copy in v2 PDelay Response specific fields
			pdelay_fwup_msg->responseOriginTimestamp =
				fields->responseOriginTimestamp;
			pdelay_fwup_msg->responseOriginTimestamp.ntoh();
			pdelay_fwup_msg->requestingPortIdentity =
				fields->requestingPortIdentity;
			pdelay_fwup_msg->
				requestingPortIdentity.ntoh();

			msg = pdelay_fwup_msg;
		}
		break;
	case ANNOUNCE_MESSAGE:

		GPTP_LOG_VERBOSE("*** Received Announce message");

		{
			announce_msg_t *fields = (decltype(fields))
				(buf + PTP_ANNOUNCE_OFFSET);
			auto annc = new PTPMessageAnnounce();
			*((PTPMessageCommon *) annc) = *msg;
			int tlv_length = size -
				(PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH);

			annc->currentUtcOffset =
				PLAT_ntohs( fields->currentUtcOffset );
			annc->grandmasterPriority1 =
				fields->grandmasterPriority1;
			annc->grandmasterClockQuality =
				fields->grandmasterClockQuality;
			annc->grandmasterClockQuality.ntoh();
			annc->grandmasterPriority2 =
				fields->grandmasterPriority2;
			annc->grandmasterIdentity =
				fields->grandmasterIdentity;
			annc->stepsRemoved =
				PLAT_ntohs( fields->stepsRemoved );
			annc->timeSource = fields->timeSource;

			// Parse TLV if it exists
			buf += PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH;
			if( tlv_length > (int) (2*sizeof(uint16_t)) && PLAT_ntohs(*((uint16_t *)buf)) == PATH_TRACE_TLV_TYPE)  {
				buf += sizeof(uint16_t);
				tlv_length -= sizeof(uint16_t);
				annc->tlv.parseClockIdentity((uint8_t *)buf, tlv_length);
			}

			msg = annc;
		}
		break;

	case SIGNALLING_MESSAGE:
		{
			auto signallingMsg = new PTPMessageSignalling();
			*((PTPMessageCommon *) signallingMsg) = *msg;
			memcpy(&(signallingMsg->targetPortIdentify),
			       buf + PTP_SIGNALLING_TARGET_PORT_IDENTITY(PTP_SIGNALLING_OFFSET),
			       sizeof(signallingMsg->targetPortIdentify));

			memcpy( &(signallingMsg->tlv), buf + PTP_SIGNALLING_OFFSET + PTP_SIGNALLING_LENGTH, sizeof(signallingMsg->tlv) );

			msg = signallingMsg;
		}
		break;

	default:

		GPTP_LOG_EXCEPTION("Received unsupported message type, %d",
		            (int)msg->messageType);
		port->incCounter_ieee8021AsPortStatRxPTPPacketDiscard();

		goto abort;
	}

	msg->_gc = false;

	return msg;

abort:

	return NULL;
}

bool PTPMessageEvent::getTxTimestamp( EtherPort *port, uint32_t link_speed )
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	int ts_good;
	Timestamp tx_timestamp;
	uint32_t unused;
	unsigned req = TX_TIMEOUT_BASE;
	int iter = TX_TIMEOUT_ITER;

	ts_good = port->getTxTimestamp
		( this, tx_timestamp, unused, false );
	while( ts_good != GPTP_EC_SUCCESS && iter-- != 0 )
	{
		timer->sleep(req);
		if (ts_good != GPTP_EC_EAGAIN && iter < 1)
			GPTP_LOG_ERROR(
				"Error (TX) timestamping PDelay request "
				"(Retrying-%d), error=%d", iter, ts_good);
		ts_good = port->getTxTimestamp
			( this, tx_timestamp, unused , iter == 0 );
		req *= 2;
	}

	if( ts_good == GPTP_EC_SUCCESS )
	{
		Timestamp phy_compensation = port->getTxPhyDelay( link_speed );
		GPTP_LOG_DEBUG( "TX PHY compensation: %s sec",
				phy_compensation.toString().c_str() );
		phy_compensation.setVersion( tx_timestamp.getVersion() );
		_timestamp = tx_timestamp + phy_compensation;
	} else
	{
		char msg[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
		port->getExtendedError(msg);
		GPTP_LOG_ERROR(
			"Error (TX) timestamping PDelay request, error=%d\t%s",
			ts_good, msg);
		_timestamp = INVALID_TIMESTAMP;
	}

	delete timer;
	return ts_good == GPTP_EC_SUCCESS;
}

void PTPMessageCommon::processMessage( EtherPort *port )
{
	_gc = true;
	return;
}

void PTPMessageCommon::buildCommonHeader(uint8_t * buf)
{
	common_header_t *hdr = (decltype(hdr)) buf;

	memset( hdr, 0, sizeof( *hdr ));
	hdr->messageType = messageType;
	hdr->transportSpecific = GPTP_TRANSPORT_SPECIFIC;
	hdr->versionPTP = versionPTP;
	hdr->messageLength = PLAT_htons(messageLength);
	hdr->domainNumber = domainNumber;
	hdr->flags = flags;
	hdr->correctionField = PLAT_htonll(correctionField);
	hdr->sourcePortIdentity = sourcePortIdentity;
	hdr->sourcePortIdentity.hton();

	GPTP_LOG_VERBOSE("Sending Sequence Id: %u", sequenceId);
	hdr->sequenceId = PLAT_htons( sequenceId );
	hdr->control = control;
	hdr->logMessageInterval = logMeanMessageInterval;

	return;
}

void PTPMessageCommon::getPortIdentity(PortIdentity * identity)
{
	*identity = sourcePortIdentity;
}

void PTPMessageCommon::setPortIdentity(PortIdentity * identity)
{
	sourcePortIdentity = *identity;
}

bool PTPMessageAnnounce::isBetterThan(PTPMessageAnnounce * msg)
{
	unsigned char this1[14];
	unsigned char that1[14];
	uint16_t tmp;

	this1[0] = grandmasterPriority1;
	that1[0] = msg->getGrandmasterPriority1();

	this1[1] = grandmasterClockQuality.cq_class;
	that1[1] = msg->getGrandmasterClockQuality()->cq_class;

	this1[2] = grandmasterClockQuality.clockAccuracy;
	that1[2] = msg->getGrandmasterClockQuality()->clockAccuracy;

	tmp = grandmasterClockQuality.offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(this1 + 3, &tmp, sizeof(tmp));
	tmp = msg->getGrandmasterClockQuality()->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(that1 + 3, &tmp, sizeof(tmp));

	this1[5] = grandmasterPriority2;
	that1[5] = msg->getGrandmasterPriority2();

	this->getGrandmasterIdentity(this1 + 6);
	msg->getGrandmasterIdentity(that1 + 6);

#if 0
	GPTP_LOG_VERBOSE("Us: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_VERBOSE("%hhx", this1[i]);
	GPTP_LOG_VERBOSE("\n");
	GPTP_LOG_VERBOSE("Them: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_VERBOSE("%hhx", that1[i]);
	GPTP_LOG_VERBOSE("\n");
#endif

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}


PTPMessageSync::PTPMessageSync( EtherPort *port ) :
	PTPMessageEvent( port )
{
	messageType = SYNC_MESSAGE;	// This is an event message
	sequenceId = port->getNextSyncSequenceId();
	control = SYNC;

	flags.twoStepFlag = 0x1;
	originTimestamp = port->getClock()->getTime();

	logMeanMessageInterval = port->getSyncInterval();
	return;
}

bool PTPMessageSync::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	uint32_t link_speed;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH;
	buildCommonHeader(buf_ptr);

	port->sendEventPort
		( PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER,
		  destIdentity, &link_speed );
	port->incCounter_ieee8021AsPortStatTxSyncCount();

	return getTxTimestamp( port, link_speed );
}

PTPMessageAnnounce::PTPMessageAnnounce( CommonPort *port ) :
	PTPMessageCommon( port )
{
	messageType = ANNOUNCE_MESSAGE;	// This is an event message
	sequenceId = port->getNextAnnounceSequenceId();
	ClockIdentity id;
	control = MESSAGE_OTHER;

	id = port->getClock()->getClockIdentity();
	tlv.appendClockIdentity(&id);

	currentUtcOffset = port->getClock()->getCurrentUtcOffset();
	grandmasterPriority1 = port->getClock()->getPriority1();
	grandmasterPriority2 = port->getClock()->getPriority2();
	grandmasterClockQuality = port->getClock()->getClockQuality();
	stepsRemoved = 0;
	timeSource = port->getClock()->getTimeSource();
	grandmasterIdentity = port->getClock()->getGrandmasterClockIdentity();

	logMeanMessageInterval = port->getAnnounceInterval();
	return;
}

bool PTPMessageAnnounce::sendPort
( CommonPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	announce_msg_t *msg = (decltype(msg))
		(buf_ptr + PTP_ANNOUNCE_OFFSET);

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength =
		PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH + tlv.length();
	buildCommonHeader(buf_ptr);
	msg->currentUtcOffset = PLAT_htons( currentUtcOffset );
	msg->grandmasterPriority1 = grandmasterPriority1;
	msg->grandmasterClockQuality = grandmasterClockQuality;
	msg->grandmasterClockQuality.hton();
	msg->grandmasterPriority2 = grandmasterPriority2;
	msg->grandmasterIdentity = grandmasterIdentity;
	msg->stepsRemoved = PLAT_htons( stepsRemoved );
	msg->timeSource = timeSource;
	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);
	port->incCounter_ieee8021AsPortStatTxAnnounce();

	return true;
}

void PTPMessageAnnounce::processMessage( EtherPort *port )
{
	ClockIdentity my_clock_identity;

	port->incCounter_ieee8021AsPortStatRxAnnounce();

	// Delete announce receipt timeout
	port->getClock()->deleteEventTimerLocked
		(port, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES);

	if( stepsRemoved >= 255 ) goto bail;

	// Reject Announce message from myself
	my_clock_identity = port->getClock()->getClockIdentity();
	if( sourcePortIdentity.getClockIdentity() == my_clock_identity ) {
		goto bail;
	}

	if(tlv.has(&my_clock_identity)) {
		goto bail;
	}

	// Add message to the list
	port->setQualifiedAnnounce( this );

	port->getClock()->addEventTimerLocked(port, STATE_CHANGE_EVENT, 16000000);
 bail:
	port->getClock()->addEventTimerLocked
		(port, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (unsigned long long)
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER *
		  (pow
		   ((double)2,
			port->getAnnounceInterval()) *
		   1000000000.0)));
}

void PTPMessageSync::processMessage( EtherPort *port )
{
	if (port->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxSyncCount();

#if CHECK_ASSIST_BIT
	if( flags.twoStepFlag )
	{
#endif
		PTPMessageSync *old_sync = port->getLastSync();

		if (old_sync != NULL) {
			delete old_sync;
		}
		port->setLastSync(this);
		_gc = false;
		goto done;
#if CHECK_ASSIST_BIT
	} else {
		GPTP_LOG_ERROR("PTP assist flag is not set, discarding invalid sync");
		_gc = true;
		goto done;
	}
#endif

 done:
	return;
}

PTPMessageFollowUp::PTPMessageFollowUp( CommonPort *port ) :
	PTPMessageCommon( port )
{
	messageType = FOLLOWUP_MESSAGE;	/* This is an event message */
	control = FOLLOWUP;

	logMeanMessageInterval = port->getSyncInterval();

	return;
}

void PTPMessageFollowUp::writeTxBuffer( uint8_t *buf, CommonPort *port )
{
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset;
	followup_msg_t *msg = (decltype(msg))
		(buf_ptr + PTP_COMMON_HDR_LENGTH);

	messageLength =
	    PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(tlv);
	buildCommonHeader(buf_ptr);
	msg->preciseOriginTimestamp = preciseOriginTimestamp;
	msg->preciseOriginTimestamp.hton();
	msg->tlv = tlv;
	msg->tlv.setGMTimeBaseIndicator
		( PLAT_htonl( tlv.getGMTimeBaseIndicator() ));
}

bool PTPMessageFollowUp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];

	memset(buf_t, 0, 256);
	writeTxBuffer( buf_t, port );

	GPTP_LOG_VERBOSE( "Follow-Up Time: %s",
			  preciseOriginTimestamp.toString().c_str() );

#ifdef DEBUG
	GPTP_LOG_VERBOSE("Follow-up Dump:");
	for (int i = 0; i < messageLength; ++i) {
		GPTP_LOG_VERBOSE("%d:%02x ", i, (unsigned char)buf_t[i]);
	}
#endif

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);

	port->incCounter_ieee8021AsPortStatTxFollowUpCount();

	return true;
}

void PTPMessageFollowUp::processMessage( EtherPort *port )
{
	uint64_t delay;
	Timestamp sync_arrival;
	Timestamp system_time(0, 0, 0);
	Timestamp device_time(0, 0, 0);

	signed long long local_system_offset;
	signed long long scalar_offset;

	FrequencyRatio local_clock_adjustment;
	FrequencyRatio local_system_freq_offset;
	FrequencyRatio master_local_freq_offset;
	int64_t correction;
	int32_t scaledLastGmFreqChange = 0;
	scaledNs scaledLastGmPhaseChange;

	GPTP_LOG_DEBUG("Processing a follow-up message");

	// Expire any SYNC_RECEIPT timers that exist
	port->stopSyncReceiptTimer();

	if (port->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxFollowUpCount();

	PortIdentity sync_id;
	PTPMessageSync *sync = port->getLastSync();
	if (sync == NULL) {
		GPTP_LOG_ERROR("Received Follow Up but there is no sync message");
		return;
	}
	sync->getPortIdentity(&sync_id);

	if ( sync->getSequenceId() != sequenceId ||
	     sync_id != sourcePortIdentity )
	{
		unsigned int cnt = 0;

		if( !port->incWrongSeqIDCounter(&cnt) )
		{
			port->becomeMaster( true );
			port->setWrongSeqIDCounter(0);
		}
		GPTP_LOG_ERROR
		    ("Received Follow Up %d times but cannot find corresponding Sync", cnt);
		goto done;
	}

	if (sync->getTimestamp().getVersion() != port->getTimestampVersion())
	{
		GPTP_LOG_ERROR("Received Follow Up but timestamp version indicates Sync is out of date");
		goto done;
	}

	sync_arrival = sync->getTimestamp();

	if( !port->getLinkDelay(&delay) ) {
		goto done;
	}

	master_local_freq_offset  =  tlv.getRateOffset();
	master_local_freq_offset /= 1ULL << 41;
	master_local_freq_offset += 1.0;
	master_local_freq_offset /= port->getPeerRateOffset();

	correctionField /= 1 << 16;
	correction = (int64_t)((delay * master_local_freq_offset) + correctionField );

	if( correction > 0 )
	  TIMESTAMP_ADD_NS( preciseOriginTimestamp, correction );
	else TIMESTAMP_SUB_NS( preciseOriginTimestamp, -correction );

	local_clock_adjustment =
	  port->getClock()->
	  calcMasterLocalClockRateDifference
	  ( preciseOriginTimestamp, sync_arrival );

	if( local_clock_adjustment == NEGATIVE_TIME_JUMP )
	{
		GPTP_LOG_VERBOSE("Received Follow Up but preciseOrigintimestamp indicates negative time jump");
		goto done;
	}

	scalar_offset  = TIMESTAMP_TO_NS( sync_arrival );
	scalar_offset -= TIMESTAMP_TO_NS( preciseOriginTimestamp );

	GPTP_LOG_VERBOSE
		("Followup Correction Field: %Ld, Link Delay: %lu", correctionField,
		 delay);
	GPTP_LOG_VERBOSE
		("FollowUp Scalar = %lld", scalar_offset);


	/* Otherwise synchronize clock with approximate time from Sync message */
	uint32_t local_clock, nominal_clock_rate;
	uint32_t device_sync_time_offset;

	port->getDeviceTime(system_time, device_time, local_clock,
			    nominal_clock_rate);
	GPTP_LOG_VERBOSE
		( "Device Time = %llu,System Time = %llu",
		  TIMESTAMP_TO_NS(device_time), TIMESTAMP_TO_NS(system_time));

	/* Adjust local_clock to correspond to sync_arrival */
	device_sync_time_offset =
		(uint32_t) (TIMESTAMP_TO_NS(device_time) - TIMESTAMP_TO_NS(sync_arrival));

	GPTP_LOG_VERBOSE( "ptp_message::FollowUp::processMessage "
			  "System time: %s Device Time: %s",
			  system_time.toString().c_str(),
			  device_time.toString().c_str() );

	/*Update information on local status structure.*/
	scaledLastGmFreqChange = (int32_t)((1.0/local_clock_adjustment -1.0) * (1ULL << 41));
	scaledLastGmPhaseChange.setLSB( tlv.getRateOffset() );
	port->getFUPStatus()->setScaledLastGmFreqChange
		( scaledLastGmFreqChange );
	port->getFUPStatus()->setScaledLastGmPhaseChange
		( scaledLastGmPhaseChange );

	if( port->getPortState() == PTP_SLAVE )
	{
		/* The sync_count counts the number of sync messages received
		   that influence the time on the device. Since adjustments are only
		   made in the PTP_SLAVE state, increment it here */
		port->incSyncCount();

		/* Do not call calcLocalSystemClockRateDifference it updates state
		   global to the clock object and if we are master then the network
		   is transitioning to us not being master but the master process
		   is still running locally */
		local_system_freq_offset =
			port->getClock()
			->calcLocalSystemClockRateDifference
			( device_time, system_time );
		TIMESTAMP_SUB_NS
			( system_time, (uint64_t)
			  (((FrequencyRatio) device_sync_time_offset)/
			   local_system_freq_offset) );
		local_system_offset =
			TIMESTAMP_TO_NS(system_time) - TIMESTAMP_TO_NS(sync_arrival);

		port->getClock()->setMasterOffset
			( port, scalar_offset, sync_arrival, local_clock_adjustment,
			  local_system_offset, system_time, local_system_freq_offset,
			  port->getSyncCount(), port->getPdelayCount(),
			  port->getPortState(), port->getAsCapable() );
		port->syncDone();
		// Restart the SYNC_RECEIPT timer
		port->startSyncReceiptTimer((unsigned long long)
			 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
			  ((double) pow((double)2, port->getSyncInterval()) *
			   1000000000.0)));
	}

	uint16_t lastGmTimeBaseIndicator;
	lastGmTimeBaseIndicator = port->getLastGmTimeBaseIndicator();
	if ((lastGmTimeBaseIndicator > 0) && (tlv.getGmTimeBaseIndicator() != lastGmTimeBaseIndicator)) {
		GPTP_LOG_EXCEPTION("Sync discontinuity");
	}
	port->setLastGmTimeBaseIndicator(tlv.getGmTimeBaseIndicator());

done:
	_gc = true;
	port->setLastSync(NULL);
	delete sync;

	return;
}

PTPMessagePathDelayReq::PTPMessagePathDelayReq( EtherPort *port ) :
	PTPMessageEvent( port )
{
	logMeanMessageInterval = 0;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_REQ_MESSAGE;
	sequenceId = port->getNextPDelaySequenceId();
	return;
}

void PTPMessagePathDelayReq::processMessage( EtherPort *port )
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PortIdentity resp_fwup_id;
	PortIdentity requestingPortIdentity_p;
	PTPMessagePathDelayResp *resp;
	PortIdentity resp_id;
	PTPMessagePathDelayRespFollowUp *resp_fwup;

	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		goto done;
	}

	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		goto done;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayRequest();

	/* Generate and send message */
	resp = new PTPMessagePathDelayResp(port);
	port->getPortIdentity(resp_id);
	resp->setPortIdentity(&resp_id);
	resp->setSequenceId(sequenceId);

	GPTP_LOG_DEBUG("Process PDelay Request SeqId: %u\t", sequenceId);

#ifdef DEBUG
	for (int n = 0; n < PTP_CLOCK_IDENTITY_LENGTH; ++n) {
		GPTP_LOG_VERBOSE("%c", resp_id.clockIdentity[n]);
	}
#endif

	this->getPortIdentity(&requestingPortIdentity_p);
	resp->setRequestingPortIdentity(&requestingPortIdentity_p);
	resp->setRequestReceiptTimestamp(_timestamp);

	port->getTxLock();
	resp->sendPort(port, &sourcePortIdentity);
	GPTP_LOG_DEBUG("*** Sent PDelay Response message");
	port->putTxLock();

	if( resp->getTimestamp().getVersion() != _timestamp.getVersion() )
	{
		GPTP_LOG_ERROR( "TX timestamp version mismatch: %u/%u",
				resp->getTimestamp().getVersion(),
				_timestamp.getVersion() );
	}

	resp_fwup = new PTPMessagePathDelayRespFollowUp(port);
	port->getPortIdentity(resp_fwup_id);
	resp_fwup->setPortIdentity(&resp_fwup_id);
	resp_fwup->setSequenceId(sequenceId);
	resp_fwup->setRequestingPortIdentity(&sourcePortIdentity);
	resp_fwup->setResponseOriginTimestamp(resp->getTimestamp());

	GPTP_LOG_VERBOSE( "Response Depart: %s",
			  resp->getTimestamp().toString().c_str() );
	GPTP_LOG_VERBOSE( "Request Arrival: %s",
			  _timestamp.toString().c_str() );

	resp_fwup->setCorrectionField(0);
	resp_fwup->sendPort(port, &sourcePortIdentity);

	GPTP_LOG_DEBUG("*** Sent PDelay Response FollowUp message");

	delete resp;
	delete resp_fwup;

done:
	delete timer;
	_gc = true;
	return;
}

bool PTPMessagePathDelayReq::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint32_t link_speed;

	if(port->pdelayHalted())
		return false;

	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	memset(buf_t, 0, 256);
	/* Create packet in buf */
	/* Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	port->sendEventPort
		( PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY,
		  destIdentity, &link_speed );
	port->incCounter_ieee8021AsPortStatTxPdelayRequest();

	return getTxTimestamp( port, link_speed );
}

PTPMessagePathDelayResp::PTPMessagePathDelayResp
( EtherPort *port ) : PTPMessageEvent( port )
{
    /*TODO: Why 0x7F?*/
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_RESP_MESSAGE;
	versionPTP = GPTP_VERSION;
	flags.twoStepFlag = 0x1;

	return;
}

void PTPMessagePathDelayResp::processMessage( EtherPort *port )
{
	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayResponse();

	if (port->tryPDelayRxLock() != true) {
		GPTP_LOG_ERROR("Failed to get PDelay RX Lock");
		return;
	}

	PortIdentity resp_id;
	PortIdentity oldresp_id;
	uint16_t resp_port_number;
	uint16_t oldresp_port_number;

	PTPMessagePathDelayResp *old_pdelay_resp = port->getLastPDelayResp();
	if( old_pdelay_resp == NULL ) {
		goto bypass_verify_duplicate;
	}

	old_pdelay_resp->getPortIdentity(&oldresp_id);
	oldresp_id.getPortNumber(&oldresp_port_number);
	getPortIdentity(&resp_id);
	resp_id.getPortNumber(&resp_port_number);

	/* In the case where we have multiple PDelay responses for the same
	 * PDelay request, and they come from different sources, it is necessary
	 * to verify if this happens 3 times (sequentially). If it does, PDelayRequests
	 * are halted for 5 minutes
	 */
	if( getSequenceId() == old_pdelay_resp->getSequenceId() )
	{
		/*If the duplicates are in sequence and from different sources*/
		if( (resp_port_number != oldresp_port_number ) && (
					(port->getLastInvalidSeqID() + 1 ) == getSequenceId() ||
					port->getDuplicateRespCounter() == 0 ) ){
			GPTP_LOG_ERROR("Two responses for same Request. seqID %d. First Response Port# %hu. Second Port# %hu. Counter %d",
				getSequenceId(), oldresp_port_number, resp_port_number, port->getDuplicateRespCounter());

			if( port->incrementDuplicateRespCounter() ) {
				GPTP_LOG_ERROR("Remote misbehaving. Stopping PDelay Requests for 5 minutes.");
				port->stopPDelay();
				port->getClock()->addEventTimerLocked
					(port, PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES, (int64_t)(300 * 1000000000.0));
			}
		}
		else {
			port->setDuplicateRespCounter(0);
		}
		port->setLastInvalidSeqID(getSequenceId());
	}
	else
	{
		port->setDuplicateRespCounter(0);
	}

bypass_verify_duplicate:
	port->setLastPDelayResp(this);

	if (old_pdelay_resp != NULL) {
		delete old_pdelay_resp;
	}

	port->putPDelayRxLock();
	_gc = false;

	return;
}

bool PTPMessagePathDelayResp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	pdelay_resp_msg_t *msg = (decltype(msg))
		(buf_ptr + PTP_COMMON_HDR_LENGTH);
	uint32_t link_speed;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH;
	buildCommonHeader(buf_ptr);

	msg->requestReceiptTimestamp = requestReceiptTimestamp;
	msg->requestReceiptTimestamp.hton();
	msg->requestingPortIdentity = requestingPortIdentity;
	msg->requestingPortIdentity.hton();

	GPTP_LOG_VERBOSE( "PDelay Resp Timestamp: %s",
			  requestReceiptTimestamp.toString().c_str() );

	port->sendEventPort
		( PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY,
		  destIdentity, &link_speed );
	port->incCounter_ieee8021AsPortStatTxPdelayResponse();

	return getTxTimestamp( port, link_speed );
}

void PTPMessagePathDelayResp::setRequestingPortIdentity
(PortIdentity * identity)
{
	requestingPortIdentity = *identity;
}

void PTPMessagePathDelayResp::getRequestingPortIdentity
(PortIdentity * identity)
{
	*identity = requestingPortIdentity;
}

PTPMessagePathDelayRespFollowUp::PTPMessagePathDelayRespFollowUp
( EtherPort *port ) : PTPMessageCommon( port )
{
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_FOLLOWUP_MESSAGE;
	versionPTP = GPTP_VERSION;

	return;
}

#define US_PER_SEC 1000000
void PTPMessagePathDelayRespFollowUp::processMessage
( EtherPort *port )
{
	Timestamp remote_resp_tx_timestamp(0, 0, 0);
	Timestamp request_tx_timestamp(0, 0, 0);
	Timestamp remote_req_rx_timestamp(0, 0, 0);
	Timestamp response_rx_timestamp(0, 0, 0);

	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayResponseFollowUp();

	if (port->tryPDelayRxLock() != true)
		return;

	PTPMessagePathDelayReq *req = port->getLastPDelayReq();
	PTPMessagePathDelayResp *resp = port->getLastPDelayResp();

	PortIdentity req_id;
	PortIdentity resp_id;
	PortIdentity fup_sourcePortIdentity;
	PortIdentity resp_sourcePortIdentity;
	ClockIdentity req_clkId;
	ClockIdentity resp_clkId;

	uint16_t resp_port_number;
	uint16_t req_port_number;

	int64_t link_delay;
	Timestamp transaction;
	Timestamp turnaround;

	if (req == NULL) {
		/* Shouldn't happen */
		GPTP_LOG_ERROR
		    (">>> Received PDelay followup but no REQUEST exists");
		goto abort;
	}

	if (resp == NULL) {
		/* Probably shouldn't happen either */
		GPTP_LOG_ERROR
		    (">>> Received PDelay followup but no RESPONSE exists");

		goto abort;
	}

	req->getPortIdentity(&req_id);
	resp->getRequestingPortIdentity(&resp_id);
	req_clkId = req_id.getClockIdentity();
	resp_clkId = resp_id.getClockIdentity();
	resp_id.getPortNumber(&resp_port_number);
	requestingPortIdentity.getPortNumber(&req_port_number);
	resp->getPortIdentity(&resp_sourcePortIdentity);
	getPortIdentity(&fup_sourcePortIdentity);

	if( req->getSequenceId() != sequenceId ) {
		GPTP_LOG_ERROR
			(">>> Received PDelay FUP has different seqID than the PDelay request (%d/%d)",
			 sequenceId, req->getSequenceId() );
		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if (resp->getSequenceId() != sequenceId) {
		GPTP_LOG_ERROR
			("Received PDelay Response Follow Up but cannot find "
			 "corresponding response");
		GPTP_LOG_ERROR("%hu, %hu, %hu, %hu", resp->getSequenceId(),
				sequenceId, resp_port_number, req_port_number);

		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if (req_clkId != resp_clkId ) {
		GPTP_LOG_ERROR
			("ClockID Resp/Req differs. PDelay Response ClockID: %s PDelay Request ClockID: %s",
			 req_clkId.getIdentityString().c_str(), resp_clkId.getIdentityString().c_str() );
		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if ( resp_port_number != req_port_number ) {
		GPTP_LOG_ERROR
			("Request port number (%hu) is different from Response port number (%hu)",
				resp_port_number, req_port_number);

		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if ( fup_sourcePortIdentity != resp_sourcePortIdentity ) {
		GPTP_LOG_ERROR("Source port identity from PDelay Response/FUP differ");

		goto abort;
	}

	port->getClock()->deleteEventTimerLocked
		(port, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES);

	GPTP_LOG_VERBOSE("Request Sequence Id: %u", req->getSequenceId());
	GPTP_LOG_VERBOSE("Response Sequence Id: %u", resp->getSequenceId());
	GPTP_LOG_VERBOSE("Follow-Up Sequence Id: %u", req->getSequenceId());

	/* Assume that we are a two step clock, otherwise originTimestamp
	   may be used */
	request_tx_timestamp = req->getTimestamp();

	if( request_tx_timestamp == INVALID_TIMESTAMP )
	{
		/* Stop processing the packet */
		goto abort;
	}
	if (request_tx_timestamp == PDELAY_PENDING_TIMESTAMP )
	{
		// Defer processing
		if(
			port->getLastPDelayRespFollowUp() != NULL &&
			port->getLastPDelayRespFollowUp() != this )
		{
			delete port->getLastPDelayRespFollowUp();
		}
		port->setLastPDelayRespFollowUp(this);
		port->getClock()->addEventTimerLocked
			(port, PDELAY_DEFERRED_PROCESSING, 1000000);
		goto defer;
	}
	remote_req_rx_timestamp = resp->getRequestReceiptTimestamp();
	response_rx_timestamp = resp->getTimestamp();
	remote_resp_tx_timestamp = responseOriginTimestamp;

	if( request_tx_timestamp.getVersion() !=
	    response_rx_timestamp.getVersion() )
	{
		GPTP_LOG_ERROR
			( "RX timestamp version mismatch %d/%d",
			  request_tx_timestamp.getVersion(),
			  response_rx_timestamp.getVersion() );
		goto abort;
	}

	port->incPdelayCount();

	transaction = response_rx_timestamp - request_tx_timestamp;
	turnaround = remote_resp_tx_timestamp - remote_req_rx_timestamp;

	// Adjust turn-around time for peer to local clock rate difference
	// TODO: Are these .998 and 1.002 specifically defined in the standard?
	// Should we create a define for them ?
	if
		( port->getPeerRateOffset() > .998 &&
		  port->getPeerRateOffset() < 1.002 )
	{
		link_delay = turnaround.getNsScalar() *
			port->getPeerRateOffset();
	}

	GPTP_LOG_VERBOSE( "Adjusted Peer turn around is %lu", link_delay );

	/* Subtract turn-around time from link delay after rate adjustment */
	link_delay = transaction.getNsScalar() - link_delay;
	link_delay /= 2;
	GPTP_LOG_DEBUG( "Link delay: %ld ns", link_delay );

	{
		uint64_t mine_elapsed;
		uint64_t theirs_elapsed;
		Timestamp prev_peer_ts_mine;
		Timestamp prev_peer_ts_theirs;
		FrequencyRatio rate_offset;
		if( port->getPeerOffset( prev_peer_ts_mine, prev_peer_ts_theirs )) {
			FrequencyRatio upper_ratio_limit, lower_ratio_limit;
			upper_ratio_limit = PPM_OFFSET_TO_RATIO(UPPER_LIMIT_PPM);
			lower_ratio_limit = PPM_OFFSET_TO_RATIO(LOWER_LIMIT_PPM);

			mine_elapsed =  TIMESTAMP_TO_NS(request_tx_timestamp)-TIMESTAMP_TO_NS(prev_peer_ts_mine);
			theirs_elapsed = TIMESTAMP_TO_NS(remote_req_rx_timestamp)-TIMESTAMP_TO_NS(prev_peer_ts_theirs);
			theirs_elapsed -= port->getLinkDelay();
			theirs_elapsed += link_delay < 0 ? 0 : link_delay;
			rate_offset =  ((FrequencyRatio) mine_elapsed)/theirs_elapsed;

			if( rate_offset < upper_ratio_limit && rate_offset > lower_ratio_limit ) {
				port->setPeerRateOffset(rate_offset);
			}
		}
	}
	if( !port->setLinkDelay( link_delay ) ) {
		if (!port->getAutomotiveProfile()) {
			GPTP_LOG_ERROR("Link delay %ld beyond neighborPropDelayThresh; not AsCapable", link_delay);
			port->setAsCapable( false );
		}
	} else {
		if (!port->getAutomotiveProfile()) {
			port->setAsCapable( true );
		}
	}
	port->setPeerOffset( request_tx_timestamp, remote_req_rx_timestamp );

 abort:
	delete req;
	port->setLastPDelayReq(NULL);
	delete resp;
	port->setLastPDelayResp(NULL);

	_gc = true;

 defer:
	port->putPDelayRxLock();

	return;
}

bool PTPMessagePathDelayRespFollowUp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	pdelay_fwup_msg_t *msg = (decltype(msg))
		(buf_ptr + PTP_COMMON_HDR_LENGTH);

	memset(buf_t, 0, 256);
	/* Create packet in buf
	   Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH;
	buildCommonHeader(buf_ptr);

	msg->requestingPortIdentity = requestingPortIdentity;
	msg->requestingPortIdentity.hton();
	msg->responseOriginTimestamp = responseOriginTimestamp;
	msg->responseOriginTimestamp.hton();

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY, destIdentity);
	port->incCounter_ieee8021AsPortStatTxPdelayResponseFollowUp();

	return true;
}

void PTPMessagePathDelayRespFollowUp::setRequestingPortIdentity
(PortIdentity * identity)
{
	requestingPortIdentity = *identity;
}


PTPMessageSignalling::PTPMessageSignalling
( EtherPort *port ) : PTPMessageCommon( port )
{
	messageType = SIGNALLING_MESSAGE;
	sequenceId = port->getNextSignalSequenceId();

	targetPortIdentify = (int8_t)0xff;

	control = MESSAGE_OTHER;

	logMeanMessageInterval = 0x7F;    // 802.1AS 2011 10.5.2.2.11 logMessageInterval (Integer8)
}

 PTPMessageSignalling::~PTPMessageSignalling(void)
{
}

void PTPMessageSignalling::setintervals(int8_t linkDelayInterval, int8_t timeSyncInterval, int8_t announceInterval)
{
	tlv.setLinkDelayInterval(linkDelayInterval);
	tlv.setTimeSyncInterval(timeSyncInterval);
	tlv.setAnnounceInterval(announceInterval);
}

bool PTPMessageSignalling::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_SIGNALLING_LENGTH + sizeof(tlv);
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);

	memcpy(buf_ptr + PTP_SIGNALLING_TARGET_PORT_IDENTITY(PTP_SIGNALLING_OFFSET),
	       &targetPortIdentify, sizeof(targetPortIdentify));

	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_SIGNALLING_LENGTH);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);

	return true;
}

void PTPMessageSignalling::processMessage( EtherPort *port )
{
	long long unsigned int waitTime;

	GPTP_LOG_STATUS("Signalling Link Delay Interval: %d", tlv.getLinkDelayInterval());
	GPTP_LOG_STATUS("Signalling Sync Interval: %d", tlv.getTimeSyncInterval());
	GPTP_LOG_STATUS("Signalling Announce Interval: %d", tlv.getAnnounceInterval());

	char linkDelayInterval = tlv.getLinkDelayInterval();
	char timeSyncInterval = tlv.getTimeSyncInterval();
	char announceInterval = tlv.getAnnounceInterval();

	if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		port->setInitPDelayInterval();

		waitTime = ((long long) (pow((double)2, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		// TODO: No send functionality needs to be implemented.
		GPTP_LOG_WARNING("Signal received to stop sending pDelay messages: Not implemented");
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
	}
	else {
		port->setPDelayInterval(linkDelayInterval);

		waitTime = ((long long) (pow((double)2, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
	}

	if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		port->resetInitSyncInterval();

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startSyncIntervalTimer(waitTime);
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		// TODO: No send functionality needs to be implemented.
		GPTP_LOG_WARNING("Signal received to stop sending Sync messages: Not implemented");
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
	}
	else {
		port->setSyncInterval(timeSyncInterval);

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startSyncIntervalTimer(waitTime);
	}

	if (!port->getAutomotiveProfile()) {
		if (announceInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
			// TODO: Needs implementation
			GPTP_LOG_WARNING("Signal received to set Announce message to initial interval: Not implemented");
		}
		else if (announceInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
			// TODO: No send functionality needs to be implemented.
			GPTP_LOG_WARNING("Signal received to stop sending Announce messages: Not implemented");
		}
		else if (announceInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
			// Nothing to do
		}
		else {
			port->setAnnounceInterval(announceInterval);

			waitTime = ((long long) (pow((double)2, port->getAnnounceInterval()) *  1000000000.0));
			waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
			port->startAnnounceIntervalTimer(waitTime);
		}
	}
}
