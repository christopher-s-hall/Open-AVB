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

#ifndef MD_ETHPORT_HPP
#define MD_ETHPORT_HPP

#include <ieee1588.hpp>
#include <avbts_message.hpp>
#include <mdport.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <ethtimestamper.hpp>

#include <stdint.h>

#include <map>
#include <list>

#define TIMEOUT_BASE 1000	/*!< Timeout base in microseconds */
#define TIMEOUT_ITER 6		/*!< Number of timeout iteractions for sending/receiving messages*/

#define TX_TIMEOUT_BASE TIMEOUT_BASE
#define TX_TIMEOUT_ITER TIMEOUT_ITER

#define RX_TIMEOUT_BASE TIMEOUT_BASE
#define RX_TIMEOUT_ITER TIMEOUT_ITER

#define PDELAY_MULTICAST GPTP_MULTICAST			/*!< PDELAY Multicast value */
#define TEST_STATUS_MULTICAST 0x011BC50AC000ULL	/*!< AVnu Automotive profile test status msg Multicast value */

class MediaDependentEtherPort : public MediaDependentPort {
	PTPMessagePathDelayReq *last_pdelay_req;
	PTPMessagePathDelayResp *last_pdelay_resp;
	PTPMessagePathDelayRespFollowUp *last_pdelay_resp_fwup;
	PTPMessageSync *last_sync;

	// Automotive Profile AVB SYNC state indicator.
	// > 0 will inditate valid AVB SYNC state
	uint32_t avbSyncState;

	OSThread *listening_thread;

	OSLock *pdelay_rx_lock;
	OSLock *port_tx_lock;

	net_result
	port_send( uint8_t * buf, size_t size, MulticastType mcast_type,
		   bool timestamp );

	/* PDelay Req/Resp threshold */
	static const unsigned int DUPLICATE_RESP_THRESH = 3;
	unsigned int duplicate_resp_counter;
	uint16_t last_invalid_seqid;

	uint32_t linkUpCount;
	uint32_t linkDownCount;

 public:
	static const LinkLayerAddress pdelay_multicast;
	static const LinkLayerAddress test_status_multicast;

	bool serializeState( void *buf, off_t *count );
	bool restoreSerializedState( void *buf, off_t *count );

	OSTimerFactory *getTimerFactory() const {
		return timer_factory;
	}

	~MediaDependentEtherPort();
	MediaDependentEtherPort
	( EthernetTimestamper *timestamper, InterfaceLabel *net_label,
	  OSConditionFactory *condition_factory,
	  OSThreadFactory *thread_factory, OSTimerFactory *timer_factory,
	  OSLockFactory *lock_factory );
	bool init_port();

	bool listenPort( OSCondition *port_ready_condition );
	bool _openPort( OSCondition *port_ready_condition );
	bool _init_port( MediaIndependentPort *port );

	/**
	 * @brief Sends event message
	 * @param  etherType
	 * @param  buf	Buffer containing PTP message
	 * @param  len	Buffer length
	 * @param  mcast_type suggested multicast based on message type
	 * @return net_succeed on success
	 */
	net_result sendEventPort
	( uint16_t etherType, uint8_t * buf, size_t len,
	  MulticastType mcast_type );

	/**
	 * @brief Sends general (e.g. Announce, Signalling) 802.1AS message
	 * @param  etherType
	 * @param  buf	Buffer containing PTP message
	 * @param  len	Buffer length
	 * @param  mcast_type suggested multicast based on message type
	 * @return net_succeed on success
	 */
	net_result sendGeneralPort
	( uint16_t etherType, uint8_t * buf, size_t len,
	  MulticastType mcast_type );

	bool processEvent(Event e);
	bool processSync( uint16_t seq, bool grandmaster, long *elapsed_time );
	bool processPDelay( uint16_t seq, long *elapsed_time );

	virtual bool adjustClockRate( double freq_offset );
	virtual bool adjustClockPhase( int64_t phase_adjust );

	void *watchNetLink(void);

	uint16_t getParentLastSyncSequenceNumber(void) const ;
	bool setParentLastSyncSequenceNumber(uint16_t num);

	void setLastSync(PTPMessageSync * msg) {
		last_sync = msg;
	}
	PTPMessageSync *getLastSync(void) const {
		return last_sync;
	}


	unsigned getTimestampDeviceIdx() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->getDeviceIdx() : 0;
	}

	/**
	 * @brief  Gets link up count
	 * @return Link up  count
	 */
	uint32_t getLinkUpCount() {
		return linkUpCount;
	}

	/**
	 * @brief  Gets link down count
	 * @return Link down count
	 */
	uint32_t getLinkDownCount() {
		return linkDownCount;
	}

	bool suspendTransmission() {
		return getTxLock();
	}
	bool resumeTransmission() {
		return putTxLock();
	}

	bool getPDelayRxLock() {
		return pdelay_rx_lock->lock() == oslock_ok ? true : false;
	}

	bool tryPDelayRxLock() {
		return pdelay_rx_lock->trylock() == oslock_ok ? true : false;
	}

	bool putPDelayRxLock() {
		return pdelay_rx_lock->unlock() == oslock_ok ? true : false;
	}

	bool getTxLock() {
		return port_tx_lock->lock() == oslock_ok ? true : false;
	}
	bool putTxLock() {
		return port_tx_lock->unlock() == oslock_ok ? true : false;
	}

	void setLastPDelayReq(PTPMessagePathDelayReq * msg) {
		last_pdelay_req = msg;
	}
	PTPMessagePathDelayReq *getLastPDelayReq(void) const {
		return last_pdelay_req;
	}

	void setLastPDelayResp(PTPMessagePathDelayResp * msg) {
		last_pdelay_resp = msg;
	}
	PTPMessagePathDelayResp *getLastPDelayResp(void) const {
		return last_pdelay_resp;
	}

	void setLastPDelayRespFollowUp(PTPMessagePathDelayRespFollowUp * msg) {
		last_pdelay_resp_fwup = msg;
	}

	PTPMessagePathDelayRespFollowUp *
	getLastPDelayRespFollowUp(void) const {
		return last_pdelay_resp_fwup;
	}

	/**
	 * @brief  Gets RX timestamp based on port identity
	 * @param  sourcePortIdentity [in] Source port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] RX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the rx lock.
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and
	 * GPTP_EC_EAGAIN to try again.
	 */
	int getRxTimestamp
	(PortIdentity * sourcePortIdentity, uint16_t sequenceId,
	 Timestamp & timestamp, unsigned &counter_value, bool last);

	/**
	 * @brief  Gets TX timestamp based on port identity
	 * @param  sourcePortIdentity [in] Source port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] TX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the TX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and
	 * GPTP_EC_EAGAIN to try again.
	 */
	int getTxTimestamp
	(PortIdentity * sourcePortIdentity, uint16_t sequenceId,
	 Timestamp & timestamp, unsigned &counter_value, bool last);

	/**
	 * @brief  Gets TX timestamp based on PTP message
	 * @param  msg PTPMessageCommon message
	 * @param  timestamp [out] TX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the TX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and
	 * GPTP_EC_EAGAIN to try again.
	 */
	int getTxTimestamp
	(PTPMessageCommon * msg, Timestamp & timestamp,
	 unsigned &counter_value, bool last);

	/**
	 * @brief  Gets RX timestamp based on PTP message
	 * @param  msg PTPMessageCommon message
	 * @param  timestamp [out] RX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the RX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and
	 * GPTP_EC_EAGAIN to try again.
	 */
	int getRxTimestamp
	(PTPMessageCommon * msg, Timestamp & timestamp,
	 unsigned &counter_value, bool last);

	/**
	 * @brief Initializes the hwtimestamper
	 */
	void timestamper_init(void);

	/**
	 * @brief Resets the hwtimestamper
	 */
	void timestamper_reset(void);

	/**
	 * @brief  Sets the duplicate pdelay_resp counter.
	 * @param  cnt Value to be set
	 */
	void setDuplicateRespCounter(unsigned int cnt)
	{
		duplicate_resp_counter = cnt;
	}

	/**
	 * @brief  Gets the current pdelay_resp duplicate messages counter
	 * @return Counter value
	 */
	unsigned int getDuplicateRespCounter(void)
	{
		return duplicate_resp_counter;
	}

	/**
	 * @brief  Increment the duplicate PDelayResp message counter
	 * @return True if it equals the threshold, False otherwise
	 */
	bool incrementDuplicateRespCounter(void)
	{
		return ++duplicate_resp_counter == DUPLICATE_RESP_THRESH;
	}

};

#endif/*MD_ETHPORT_HPP*/
