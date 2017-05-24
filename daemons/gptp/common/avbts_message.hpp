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

#ifndef AVBTS_MESSAGE_HPP
#define AVBTS_MESSAGE_HPP

#include <stdint.h>
#include <avbts_osnet.hpp>
#include <ieee1588.hpp>

#include <list>
#include <algorithm>

#include <ether_port.hpp>

/** @file **/

#define PTP_CODE_STRING_LENGTH 4		/*!< PTP code string length in bytes */
#define PTP_SUBDOMAIN_NAME_LENGTH 16	/*!< PTP subdomain name lenght in bytes */

#define GPTP_VERSION 2			/*!< GPTP version */
#define PTP_NETWORK_VERSION 1	/*!< PTP Network version */
#define GPTP_TRANSPORT_SPECIFIC (0x1)	//!< Transport specific for 802.1AS

#define PTP_ETHER 1		/*!< @todo Not used */
#define PTP_DEFAULT 255	/*!< @todo Not used */

/*Exact fit. No padding*/
#pragma pack(push,1)

/**
 * @brief Common header Flags field 802.1AS-2011 tables 10-6 & 11-4
 */
typedef struct
{
	unsigned int	reserved0		: 1;
	unsigned int	twoStepFlag		: 1;
	unsigned int	reserved1		: 6;
	unsigned int	leap61			: 1;
	unsigned int	leap59			: 1;
	unsigned int	currentUtcOffsetValid	: 1;
	unsigned int	ptpTimescale		: 1;
	unsigned int	timeTraceable		: 1;
	unsigned int	frequencyTraceable	: 1;
	unsigned int	reserved2		: 2;
} header_flags_t;

/**
 * @brief Common header 802.1AS-2011 table 10-4
 */
typedef struct
{
	unsigned int	messageType		: 4;
	unsigned int	transportSpecific	: 4;
	unsigned int	versionPTP		: 4;
	unsigned int	reserved0		: 4;
	uint16_t	messageLength;
	uint8_t		domainNumber;
	uint8_t		reserved1;
	header_flags_t	flags;
	int64_t		correctionField;
	uint32_t	reserved2;
	PortIdentity	sourcePortIdentity;
	uint16_t	sequenceId;
	uint8_t		control;
	uint8_t		logMessageInterval;
} common_header_t;

/**
 * @brief Announce payload 802.1AS-2011 table 10-7
 */
typedef struct
{
	uint8_t		reserved0[10];
	int16_t		currentUtcOffset;
	uint8_t		reserved1;
	uint8_t		grandmasterPriority1;
	ClockQuality	grandmasterClockQuality;
	uint8_t		grandmasterPriority2;
	ClockIdentity	grandmasterIdentity;
	uint16_t	stepsRemoved;
	uint8_t		timeSource;
} announce_msg_t;

/**
 * @brief FollowUp payload 802.1AS-2011 table 11-9
 */
typedef struct
{
	_Timestamp	preciseOriginTimestamp;
	FollowUpTLV	tlv;
} followup_msg_t;

/**
 * @brief Pdelay Response payload 802.1AS-2011 table 11-12
 */
typedef struct
{
	_Timestamp	requestReceiptTimestamp;
	PortIdentity	requestingPortIdentity;
} pdelay_resp_msg_t;

/**
 * @brief Pdelay Response FollowUp payload 802.1AS-2011 table 11-12
 */
typedef struct
{
	_Timestamp	responseOriginTimestamp;
	PortIdentity	requestingPortIdentity;
} pdelay_fwup_msg_t;

/* back to whatever the previous packing mode was */
#pragma pack(pop)

#define PTP_COMMON_HDR_OFFSET 0		/*!< PTP common header offset */
#define PTP_COMMON_HDR_LENGTH ((int)sizeof(common_header_t))
//!< PTP common header length (bytes)

#define PTP_ANNOUNCE_OFFSET PTP_COMMON_HDR_LENGTH /*!< PTP announce offset */
#define PTP_ANNOUNCE_LENGTH ((int)sizeof(announce_msg_t))
/*!< PTP announce length in bytes */

#define PTP_SYNC_OFFSET PTP_COMMON_HDR_LENGTH /*!< PTP SYNC base offset */
#define PTP_SYNC_LENGTH ((int)sizeof(Timestamp))
/*!< PTP SYNC length in bytes */

/// PTP FOLLOWUP base offset
#define PTP_FOLLOWUP_OFFSET PTP_COMMON_HDR_LENGTH
/*!< PTP FOLLOWUP base offset */
#define PTP_FOLLOWUP_LENGTH ((int)sizeof(followup_msg_t))
/*!< PTP FOLLOWUP length in bytes */

#define PTP_PDELAY_REQ_OFFSET PTP_COMMON_HDR_LENGTH
/*!< PTP PDELAY REQUEST base offset */
#define PTP_PDELAY_REQ_LENGTH ((int)(sizeof(_Timestamp)+sizeof(PortIdentity)))
/*!< PTP PDELAY REQUEST length in bytes */

#define PTP_PDELAY_RESP_OFFSET PTP_COMMON_HDR_LENGTH
/*!< PDELAY RESPONSE base offset */
#define PTP_PDELAY_RESP_LENGTH ((int)sizeof(pdelay_resp_msg_t))
/*!< PDELAY RESPONSE length in bytes */

#define PTP_PDELAY_FWUP_OFFSET PTP_COMMON_HDR_LENGTH
/*!< PTP PDELAY FOLLOWUP base offset*/
#define PTP_PDELAY_FWUP_LENGTH ((int)sizeof(pdelay_fwup_msg_t))
/*!< PTP PDELAY FOLLOWUP length in bytes */

#define PTP_SIGNALLING_OFFSET PTP_COMMON_HDR_LENGTH
/*!< PTP signalling offset */
#define PTP_SIGNALLING_LENGTH 10                        /*!< PTP signalling length in bytes */
#define PTP_SIGNALLING_TARGET_PORT_IDENTITY(x) x        /*!< PTP signalling Tareget Port Identity */

#define TX_TIMEOUT_BASE 1000 	/*!< Timeout base in microseconds */
#define TX_TIMEOUT_ITER 6		/*!< Number of timeout iteractions for sending/receiving messages*/

/**
 * @brief Enumeration legacy message type
 */
enum LegacyMessageType {
	SYNC,
	DELAY_REQ,
	FOLLOWUP,
	DELAY_RESP,
	MANAGEMENT,
	MESSAGE_OTHER
};

/**
 * @brief Provides the PTPMessage common interface used during building of
 * PTP messages.
 */
class PTPMessageCommon {
protected:
	unsigned char versionPTP;	/*!< PTP version */
	uint16_t versionNetwork;	/*!< Network version */
	MessageType messageType;	/*!< MessageType to be built */

	PortIdentity sourcePortIdentity;	/*!< PortIdentity from source*/

	uint16_t sequenceId;		/*!< PTP message sequence ID*/
	LegacyMessageType control;	/*!< Control message type of LegacyMessageType */
	header_flags_t flags;		/*!< PTP flags field */

	uint16_t messageLength;			/*!< PTP message length */
	char logMeanMessageInterval;	/*!< LogMessageInterval (IEEE 1588-2008 table 24)*/
	long long correctionField;		/*!< Correction Field (IEEE 1588-2008 table 21) */
	unsigned char domainNumber;		/*!< PTP domain number */

	bool _gc;	/*!< Garbage collection flag */

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessageCommon() {};

 public:
	/**
	 * @brief Creates the PTPMessageCommon interface
	 * @param port EtherPort where the message interface is
	 * connected to.
	 */
	PTPMessageCommon( CommonPort *port );
	/**
	 * @brief Destroys PTPMessageCommon interface
	 */
	virtual ~PTPMessageCommon(void) {}

	/**
	 * @brief  Gets the sequenceId value within a ptp message
	 * @return Sequence ID value
	 */
	uint16_t getSequenceId(void) {
		return sequenceId;
	}
	/**
	 * @brief  Sets the sequence ID value to the PTP message.
	 * @param  seq Sequence id value to be set.
	 * @return void
	 */
	void setSequenceId(uint16_t seq) {
		sequenceId = seq;
	}

	/**
	 * @brief  Gets the MessageType field within the PTP message.
	 * @return MessageType
	 */
	MessageType getMessageType(void) {
		return messageType;
	}

	/**
	 * @brief Check if message type is event
	 * @return true if an event message
	 */
	bool isEvent( void )
	{
		return (( messageType >> 3) & 0x1 ) == 0;
	}

	/**
	 * @brief  Gets the MessageID of the PTP message.
	 * @return MessageId
	 */
	PTPMessageId getMessageId(void) {
		return PTPMessageId(messageType, sequenceId);
	}
	/**
	 * @brief  Gets the correctionField value in a Little-Endian format.
	 * @return correctionField
	 * @todo Little endian format could be removed by adding endianess discovery on
	 * compile/run time.
	 */
	long long getCorrectionField(void) {
		return correctionField;
	}
	/**
	 * @brief  Sets the correction field. It expects the host format.
	 * @param  correctionAmount
	 * @return void
	 * @todo Little endian format could be removed by adding endianess discovery on
	 * compile/run time.
	 */
	void setCorrectionField(long long correctionAmount) {
		correctionField = correctionAmount;
	}

	/**
	 * @brief  Gets PortIdentity field
	 * @param  identity [out] Source port identity
	 * @return void
	 */
	void getPortIdentity(PortIdentity * identity);
	/**
	 * @brief  Sets PortIdentity value
	 * @param  identity [in] Source port identity value to be set.
	 * @return void
	 */
	void setPortIdentity(PortIdentity * identity);

	/**
	 * @brief Gets the garbage collection status
	 * @return TRUE when it needs to be clean. FALSE otherwise.
	 */
	bool garbage() {
		return _gc;
	}

	/**
	 * @brief  Determine whether the message was sent by given communication technology, uuid, and
	 * port id fields
	 * @param  portIdentity PortIdentity value
	 * @return TRUE if sender equals to internal PTPCommon values, FALSE otherwise.
	 */
	bool isSenderEqual(PortIdentity portIdentity);

	/**
	 * @brief  Generic interface for processing PTP message
	 * @param  port IEEE1588 port
	 * @return void
	 */
	virtual void processMessage( EtherPort *port );

	/**
	 * @brief  Builds PTP common header
	 * @param  buf [out] PTP message
	 * @return void
	 */
	void buildCommonHeader(uint8_t * buf);

	/**
	 * @brief parses buffer and creates PTP message
	 * @param [in] buf		contains frame
	 * @param [in] size		frame length
	 * @param [out] remote		address buffer was received from
	 * @param [in] port		port frame was received on
	 * @return NULL on failure, otherwise pointer to message object
	 */
	friend PTPMessageCommon *buildPTPMessageCommon
	( char *buf, int size, LinkLayerAddress * remote,
	  CommonPort *port );
};

class PTPMessageEvent : public PTPMessageCommon
{
protected:
	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessageEvent() {};

	Timestamp _timestamp;	/*!< PTP message timestamp */
	unsigned _timestamp_counter_value;	//!< PTP timestamp counter
						//!< value

public:
	/**
	 * @brief Creates the PTPMessageEvent interface
	 * @param port EtherPort where the message interface is
	 * connected to.
	 */
	PTPMessageEvent( EtherPort *port ) :
		PTPMessageCommon( port ) {}

	/**
	 * @brief Destroys PTPMessageEvent
	 */
	virtual ~PTPMessageEvent(void) {}

	/**
	 * @brief  Gets the current Timestamp value from the PTP message
	 * @return Current Timestamp value
	 */
	Timestamp getTimestamp(void) {
		return _timestamp;
	}
	/**
	 * @brief  Gets the timestamp counter value set during the RX
	 *	timestamp method.
	 * @return timestamp counter value
	 */
	uint32_t getTimestampCounterValue(void)
	{
		return _timestamp_counter_value;;
	}

	/**
	 * @brief  Sets the timestamp value
	 * @param  timestamp [in] Reference to Timestamp value
	 * @return void
	 */
	void setTimestamp(Timestamp & timestamp)
	{
		_timestamp = timestamp;
	}

	/**
	 * @brief Get TX timestamp
	 * @param port used to send message
	 * @param link_speed link speed of message
	 */
	bool getTxTimestamp( EtherPort *port, uint32_t link_speed );


	friend PTPMessageCommon *buildPTPMessageCommon
	( char *buf, int size, LinkLayerAddress * remote,
	  CommonPort *port );
	friend PTPMessageCommon *buildPTPMessageEvent
	( PTPMessageEvent *msg,  CommonPort *cport, char *buf, int size );
};

/*Exact fit. No padding*/
#pragma pack(push,1)

#define PATH_TRACE_TLV_TYPE 0x8		/*!< This is the value that indicates the
									  TLV is a path trace TLV, as specified in
									  16.2.7.1 and Table 34 of IEEE Std
									  1588-2008. The value is specified there
									  as PATH_TRACE, whose value is 0x8. */

/**
 * @brief Provides the PathTraceTLV interface
 * The fields of the path TLV shall be as specified in Table 10-8 and in
 * 10.5.4.3.2 through 10.5.4.3.9 from IEEE 802.1AS. This TLV,
 * and its use, are defined in IEEE Std 1588-2008 (see 16.2 and Table 34 of IEEE Std 1588-2008).
 */
class PathTraceTLV {
 private:
	uint16_t tlvType;
	typedef std::list<ClockIdentity> IdentityList;
	IdentityList identityList;
 public:
	/**
	 * @brief Creates the PathTraceTLV interface.
	 * Sets tlvType to PATH_TRACE_TLV_TYPE using network byte order
	 */
	PathTraceTLV() {
		tlvType = PLAT_htons(PATH_TRACE_TLV_TYPE);
	}
	/**
	 * @brief  Parses ClockIdentity from message buffer
	 * @param  buffer [in] Message buffer. It should be at least ::PTP_CLOCK_IDENTITY_LENGTH bytes long.
	 * @param  size [in] Buffer size. Should be the length of the data pointed to by the buffer argument.
	 * @return void
	 */
	void parseClockIdentity(uint8_t *buffer, int size) {
		int length = PLAT_ntohs(*(uint16_t*)buffer);

		buffer += sizeof(uint16_t);
		size -= sizeof(uint16_t);

		if((unsigned)size < (unsigned)length) {
			length = size;
		}
		length /= PTP_CLOCK_IDENTITY_LENGTH;

		for(; length > 0; --length) {
			ClockIdentity add;
			add.set(buffer);
			identityList.push_back(add);
			buffer += PTP_CLOCK_IDENTITY_LENGTH;
		}
	}

	/**
	 * @brief  Appends new ClockIdentity to internal ClockIdentity list
	 * @param  id ClockIdentity to be appended
	 * @return void
	 */
	void appendClockIdentity(ClockIdentity * id) {
		identityList.push_back(*id);
	}

	/**
	 * @brief  Gets TLV value in a byte string format
	 * @param  byte_str [out] Output byte string
	 * @return void
	 */
	void toByteString(uint8_t * byte_str) {
		IdentityList::iterator iter;
		*((uint16_t *)byte_str) = tlvType;  // tlvType already in network byte order
		byte_str += sizeof(tlvType);
		*((uint16_t *)byte_str) = PLAT_htons
			((uint16_t)identityList.size()*PTP_CLOCK_IDENTITY_LENGTH);
		byte_str += sizeof(uint16_t);
		for
			(iter = identityList.begin();
			 iter != identityList.end(); ++iter) {
			iter->getIdentityString(byte_str);
			byte_str += PTP_CLOCK_IDENTITY_LENGTH;
		}
	}

	/**
	 * @brief  Looks for a specific ClockIdentity on the current TLV
	 * @param  id [in] Desired ClockIdentity
	 * @return TRUE if it has found it, FALSE otherwise.
	 */
	bool has(ClockIdentity *id) {
		return std::find
			(identityList.begin(), identityList.end(), *id) !=
			identityList.end();
	}

	/**
	 * @brief  Gets the total length of TLV.
	 * Total length of TLV is length of type field (UINT16) + length of 'length'
	 * field (UINT16) + length of
	 * identities (each PTP_CLOCK_IDENTITY_LENGTH) in the path
	 * @return Total length
	 */
	int length() {
		return (int)(2*sizeof(uint16_t) + PTP_CLOCK_IDENTITY_LENGTH*identityList.size());
	}
};

/* back to whatever the previous packing mode was */
#pragma pack(pop)

/**
 * @brief Provides the PTPMessageAnnounce interface
 * The PTPMessageAnnounce class is used to create
 * announce messages on the 802.1AS format when building
 * the ptp messages.
 */
class PTPMessageAnnounce:public PTPMessageCommon {
 private:
	ClockIdentity grandmasterIdentity;
	ClockQuality grandmasterClockQuality;

	PathTraceTLV tlv;

	uint16_t currentUtcOffset;
	unsigned char grandmasterPriority1;
	unsigned char grandmasterPriority2;
	uint16_t stepsRemoved;
	unsigned char timeSource;

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessageAnnounce(void) {}
 public:
	 /**
	  * @brief Creates the PTPMessageAnnounce interface
	  */
	 PTPMessageAnnounce( CommonPort * port );

	 /**
	  * @brief Destroys the PTPMessageAnnounce interface
	  */
	~PTPMessageAnnounce() {}

	/**
	 * @brief  Compare gramdmaster's capabilities comming on the
	 * announce messages against the current grandmaster capabilities.
	 * @param  msg [in] PTPMessageAnnounce to be compared
	 * @return TRUE if it is better. FALSE otherwise.
	 */
	bool isBetterThan(PTPMessageAnnounce * msg);

	/**
	 * @brief  Gets grandmaster's priority1 value
	 * @return Grandmaster priority1
	 */
	unsigned char getGrandmasterPriority1(void) {
		return grandmasterPriority1;
	}

	/**
	 * @brief  Gets grandmaster's priority2 value
	 * @return Grandmaster priority2
	 */
	unsigned char getGrandmasterPriority2(void) {
		return grandmasterPriority2;
	}

	/**
	 * @brief  Gets grandmaster clock quality
	 * @return Pointer to a ClockQuality object.
	 */
	ClockQuality *getGrandmasterClockQuality(void) {
		return &grandmasterClockQuality;
	}

	/**
	 * @brief  Gets the steps removed value. See IEEE 802.1AS-2011 Clause 10.3.3
	 * @return steps removed value
	 */
	uint16_t getStepsRemoved(void) {
		return stepsRemoved;
	}

	/**
	 * @brief Gets grandmaster identity value
	 * @param  identity [out] Grandmaster identity
	 * @return void
	 */
	void getGrandmasterIdentity( uint8_t *identity )
	{
		grandmasterIdentity.getIdentityString( identity );
	}

	/**
	 * @brief  Gets grandmaster's clockIdentity value
	 * @return Grandmaster ClockIdentity
	 */
	ClockIdentity getGrandmasterClockIdentity() {
		return grandmasterIdentity;
	}

	/**
	 * @brief  Processes PTP message
	 * @param  port EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Assembles PTPMessageAnnounce message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	( CommonPort *port, PortIdentity *destIdentity);

	friend PTPMessageCommon *buildPTPMessageCommon
	( char *buf, int size, LinkLayerAddress *remote,
	  CommonPort *port );
};

/**
 * @brief Provides a class for building the PTP Sync message
 */
class PTPMessageSync : public PTPMessageEvent {
 private:
	Timestamp originTimestamp;

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessageSync() {}
 public:
	/**
	 * @brief Default constructor. Creates PTPMessageSync
	 * @param port EtherPort
	 */
	PTPMessageSync( EtherPort *port );

	/**
	 * @brief Destroys PTPMessageSync interface
	 */
	~PTPMessageSync() {}

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Gets origin timestamp value
	 * @return Origin Timestamp
	 */
	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}

	/**
	 * @brief  Assembles PTPMessageSync message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	(EtherPort *port, PortIdentity *destIdentity );

	friend PTPMessageCommon *buildPTPMessageEvent
	( PTPMessageEvent *msg,  CommonPort *cport, char *buf, int size );
};

/**
 * @brief Provides a class for a class for building a PTP follow up message
 */
class PTPMessageFollowUp:public PTPMessageCommon {
private:
	Timestamp preciseOriginTimestamp;

	FollowUpTLV tlv;

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessageFollowUp() {}
public:
	/**
	 * @brief Builds the PTPMessageFollowUP object
	 */
	PTPMessageFollowUp( CommonPort *port );

	/**
	 * @brief Writes payload for transmission in network order
	 * @param buf	containing message
	 * @param port	that message is transmitted on
	 */
	void writeTxBuffer( uint8_t *buf, CommonPort *port )

	/**
	 * @brief  Assembles PTPMessageFollowUp message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	( EtherPort *port, PortIdentity *destIdentity );

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Gets the precise origin timestamp value
	 * @return preciseOriginTimestamp value
	 */
	Timestamp getPreciseOriginTimestamp(void) {
		return preciseOriginTimestamp;
	}

	/**
	 * @brief  Sets the precis origin timestamp value
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setPreciseOriginTimestamp(Timestamp & timestamp) {
		preciseOriginTimestamp = timestamp;
	}

	/**
	 * @brief  Sets the clock source time interface (802.1AS 9.2)
	 * @param  fup Follow up message
	 * @return void
	 */
	void setClockSourceTime(FollowUpTLV *fup)
	{
		tlv.setGMTimeBaseIndicator(fup->getGMTimeBaseIndicator());
		tlv.setScaledLastGmFreqChange(fup->getScaledLastGmFreqChange());
		tlv.setScaledLastGmPhaseChange(fup->getScaledLastGmPhaseChange());
	}

	friend PTPMessageCommon *buildPTPMessageCommon
	( char *buf, int size, LinkLayerAddress *remote,
	  CommonPort *port );
};

/**
 * @brief Provides a class for building the PTP Path Delay Request message
 */
class PTPMessagePathDelayReq : public PTPMessageEvent {
 private:
	Timestamp originTimestamp;

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessagePathDelayReq() {}
 public:
	/**
	 * @brief Destroys the PTPMessagePathDelayReq object
	 */
	~PTPMessagePathDelayReq() {
	}

	/**
	 * @brief Builds the PTPMessagePathDelayReq message
	 */
	PTPMessagePathDelayReq( EtherPort *port );

	/**
	 * @brief  Assembles PTPMessagePathDelayReq message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	( EtherPort *port, PortIdentity *destIdentity );

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Gets origin timestamp value
	 * @return Origin Timestamp
	 */
	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}

	friend PTPMessageCommon *buildPTPMessageEvent
	( PTPMessageEvent *msg,  CommonPort *cport, char *buf, int size );
};

/**
 * @brief Provides a class for building the PTP Path Delay Response message.
 */
class PTPMessagePathDelayResp : public PTPMessageEvent {
private:
	PortIdentity requestingPortIdentity;
	Timestamp requestReceiptTimestamp;

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessagePathDelayResp() {}
public:
	/**
	 * @brief Destroys the PTPMessagePathDelayResp object
	 */
	~PTPMessagePathDelayResp() {}
	/**
	 * @brief Builds the PTPMessagePathDelayResp object
	 */
	PTPMessagePathDelayResp( EtherPort *port );

	/**
	 * @brief  Assembles PTPMessagePathDelayResp message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	( EtherPort *port, PortIdentity *destIdentity );

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Sets the request receipt timestamp
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setRequestReceiptTimestamp(Timestamp timestamp) {
		requestReceiptTimestamp = timestamp;
	}

	/**
	 * @brief  Sets requesting port identity
	 * @param  identity [in] PortIdentity to be set
	 * @return void
	 */
	void setRequestingPortIdentity(PortIdentity * identity);
	/**
	 * @brief  Gets requesting port identity
	 * @param  identity [out] Requested PortIdentity
	 * @return void
	 */
	void getRequestingPortIdentity(PortIdentity * identity);

	/**
	 * @brief  Gets the request receipt timestamp
	 * @return requestReceiptTimestamp
	 */
	Timestamp getRequestReceiptTimestamp(void) {
		return requestReceiptTimestamp;
	}

	friend PTPMessageCommon *buildPTPMessageEvent
	( PTPMessageEvent *msg, CommonPort *cport, char *buf, int size );
};

/**
 * @brief Provides a class for building the PTP Path Delay Response follow up message.
 */
class PTPMessagePathDelayRespFollowUp:public PTPMessageCommon {
 private:
	Timestamp responseOriginTimestamp;
	PortIdentity requestingPortIdentity;

	/**
	 * @brief Default constructor used only by buildPTPMessage()
	 */
	PTPMessagePathDelayRespFollowUp() {}
public:
	/**
	 * @brief Builds the PTPMessagePathDelayRespFollowUp object
	 */
	PTPMessagePathDelayRespFollowUp( EtherPort *port );

	/**
	 * @brief Destroys the PTPMessagePathDelayRespFollowUp object
	 */
	~PTPMessagePathDelayRespFollowUp() {}

	/**
	 * @brief  Assembles PTPMessageRespFollowUp message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	( EtherPort *port, PortIdentity *destIdentity );

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Sets the response origin timestamp
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setResponseOriginTimestamp(Timestamp timestamp) {
		responseOriginTimestamp = timestamp;
	}
	/**
	 * @brief  Sets the requesting port identity
	 * @param  identity [in] PortIdentity to be set
	 * @return void
	 */
	void setRequestingPortIdentity(PortIdentity * identity);

	/**
	 * @brief  Gets the response origin timestamp
	 * @return responseOriginTimestamp
	 */
	Timestamp getResponseOriginTimestamp(void) {
		return responseOriginTimestamp;
	}
	/**
	 * @brief Gets the requesting port identity
	 * @return Pointer to requesting PortIdentity object
	 */
	PortIdentity *getRequestingPortIdentity(void) {
		return &requestingPortIdentity;
	}

	friend PTPMessageCommon *buildPTPMessageCommon
	( char *buf, int size, LinkLayerAddress *remote,
	  CommonPort *port );
};

/*Exact fit. No padding*/
#pragma pack(push,1)


/**
 * @brief Provides a Signalling Msg Interval Request TLV interface back to the previous
 * packing mode
 */
class SignallingTLV {
 private:
	uint16_t tlvType;
	uint16_t lengthField;
	uint8_t organizationId[3];
	uint8_t organizationSubType_ms;
	uint16_t organizationSubType_ls;
	uint8_t linkDelayInterval;
	uint8_t timeSyncInterval;
	uint8_t announceInterval;
	uint8_t flags;
	uint16_t reserved;
 public:
	/**
	 * @brief Builds the Signalling Msg Interval Request TLV interface
	 */
	SignallingTLV() {
		tlvType = PLAT_htons(0x3);
		lengthField = PLAT_htons(12);
		organizationId[0] = '\x00';
		organizationId[1] = '\x80';
		organizationId[2] = '\xC2';
		organizationSubType_ms = 0;
		organizationSubType_ls = PLAT_htons(2);
		linkDelayInterval = 0;
		timeSyncInterval = 0;
		announceInterval = 0;
		flags = 3;
		reserved = PLAT_htons(0);
	}

	/**
	 * @brief  Gets Msg Interval Request TLV information in a byte
	 *         string format
	 * @param  byte_str [out] Msg Interval Request TLV values
	 */
	void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}

	/**
	 * @brief  Gets the link delay interval.
	 * @return 8 bit signed value of the link delay interval.
	 */
	int8_t getLinkDelayInterval() {
		return linkDelayInterval;
	}

	/**
	 * @brief  Sets the link delay interval.
	 * @param 8 bit signed value of the link delay interval.
	 * @return void
	 */
	void setLinkDelayInterval(int8_t linkDelayInterval) {
		this->linkDelayInterval = linkDelayInterval;
	}

	/**
	 * @brief  Gets the time sync interval.
	 * @return 8 bit signed value of the time sync interval.
	 */
	int8_t getTimeSyncInterval() {
		return timeSyncInterval;
	}

	/**
	 * @brief  Sets the time sync interval.
	 * #param  8 bit signed value of the time sync interval.
	 * @return void
	 */
	void setTimeSyncInterval(int8_t timeSyncInterval) {
		this->timeSyncInterval = timeSyncInterval;
	}

	/**
	 * @brief  Gets the announce interval.
	 * @return 8 bit signed value of the announce interval.
	 */
	int8_t getAnnounceInterval() {
		return announceInterval;
	}

	/**
	 * @brief  Sets the announce interval.
	 * @param  8 bit signed value of the announce interval.
	 * @return void
	 */
	void setAnnounceInterval(int8_t announceInterval) {
		this->announceInterval = announceInterval;
	}
};

/* back to whatever the previous packing mode was */
#pragma pack(pop)

/**
 * @brief Provides a class for building a PTP signalling message
 */
class PTPMessageSignalling:public PTPMessageCommon {
private:
	int8_t targetPortIdentify;
	SignallingTLV tlv;

	PTPMessageSignalling(void) {}
public:
	static const int8_t sigMsgInterval_Initial =  126;
	static const int8_t sigMsgInterval_NoSend =  127;
	static const int8_t sigMsgInterval_NoChange =  -128;

	/**
	 * @brief Builds the PTPMessageSignalling object
	 */
	PTPMessageSignalling( EtherPort *port );

	/**
	 * @brief Destroys the PTPMessageSignalling object
	 */
	~PTPMessageSignalling();

	/**
	 * @brief Sets the signalling intervals
	 * @param  linkDelayInterval link delay interval
	 * @param  timeSyncInterval Sync interval
	 * @param  announceInterval Announce interval
	 * @return void
	 */
	void setintervals(int8_t linkDelayInterval, int8_t timeSyncInterval, int8_t announceInterval);

	/**
	 * @brief  Assembles PTPMessageSignalling message on the
	 *         EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort
	( EtherPort *port, PortIdentity *destIdentity );

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	friend PTPMessageCommon *buildPTPMessageCommon
	( char *buf, int size, LinkLayerAddress *remote,
	  CommonPort * port);
};

#endif
