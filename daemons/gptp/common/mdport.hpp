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

#ifndef MDPORT_HPP
#define MDPORT_HPP

#include <min_port.hpp>
#include <avbts_oscondition.hpp>
#include <avbts_osnet.hpp>
#include <timestamper.hpp>
#include <avbts_clock.hpp>
#include <stdint.h>

typedef struct {
	MediaDependentPort *port;
	OSCondition *ready;
} ListenPortWrapperArg;

typedef struct {
	MediaDependentPort *port;
	OSCondition *ready;
} WatchNetLinkWrapperArg;

/**
 * @brief Structure for IEE1588Port Counters
 */
typedef struct {
	int32_t ieee8021AsPortStatRxSyncCount;
	int32_t ieee8021AsPortStatRxFollowUpCount;
	int32_t ieee8021AsPortStatRxPdelayRequest;
	int32_t ieee8021AsPortStatRxPdelayResponse;
	int32_t ieee8021AsPortStatRxPdelayResponseFollowUp;
	int32_t ieee8021AsPortStatRxAnnounce;
	int32_t ieee8021AsPortStatRxPTPPacketDiscard;
	int32_t ieee8021AsPortStatRxSyncReceiptTimeouts;
	int32_t ieee8021AsPortStatAnnounceReceiptTimeouts;
	int32_t ieee8021AsPortStatPdelayLostResponsesExceeded;
	int32_t ieee8021AsPortStatTxSyncCount;
	int32_t ieee8021AsPortStatTxFollowUpCount;
	int32_t ieee8021AsPortStatTxPdelayRequest;
	int32_t ieee8021AsPortStatTxPdelayResponse;
	int32_t ieee8021AsPortStatTxPdelayResponseFollowUp;
	int32_t ieee8021AsPortStatTxAnnounce;
} IEEE1588EthPortCounters_t;

class MediaDependentPort {
private:
	/* Signed value allows this to be negative result because of inaccurate
	timestamp */
	int64_t one_way_delay;
	int64_t neighbor_prop_delay_thresh;

	OSLock *glock;

	MediaIndependentPort *port;

	InterfaceLabel *net_label;
	OSThreadFactory *thread_factory;
	OSConditionFactory *condition_factory;
	OSLockFactory *lock_factory;

	LinkLayerAddress local_addr;
	LinkLayerAddress peer_addr;
	bool unicast_min;		// Unicast for media independent msgs

	struct phy_delay phy_delay;

	/* Sync/FollowUp threshold*/
	static const unsigned int DEFAULT_SYNC_RECEIPT_THRESH = 5;
	unsigned int sync_receipt_thresh;
	unsigned int wrongSeqIDCounter;

	IEEE1588EthPortCounters_t counters;

protected:
	static const int64_t ONE_WAY_DELAY_DEFAULT = 3600000000000;
	static const int64_t INVALID_LINKDELAY = 3600000000000;
	static const int64_t NEIGHBOR_PROP_DELAY_THRESH = 800;

	Timestamper *_hw_timestamper;

	OSTimerFactory *timer_factory;

	OSThread *listening_thread;
	OSThread *link_thread;

	OSNetworkInterface *net_iface;

	MediaDependentPort(IEEE1588PortInit_t *portInit);

public:
	virtual ~MediaDependentPort() = 0;

	/**
	 * @brief Common port initialization
	 * @param port		Associated media independent port
	 * @param portInit	port initialization parameters
	 */
	virtual bool init_port
	( MediaIndependentPort *port, IEEE1588PortInit_t *portInit );

	/**
	 * @brief Finish port initialization in derived class
	 * @param port		Associated media independent port
	 * @param portInit	port initialization parameters
	 */
	virtual bool _init_port
	( MediaIndependentPort *port, IEEE1588PortInit_t *portInit ) = 0;

	virtual bool stop() {
		return false;
	}

	/**
	 * @brief this function starts media dependent listener and link watch
	 * @return true on success
	 */
	bool startPort();

	/**
	 * @brief this function listens for incoming frames
	 * @return true on success
	 */
	virtual bool listenPort( OSCondition *ready ) = 0;
	virtual bool recoverPort() { return false; }

	/**
	 * @brief this function listens changes in link state
	 * @return true on success, undefined if unimplemented
	 */
	virtual tristate_t watchNetLink( OSCondition *ready )
	{
		return undefined;
	}

	/**
	 * @brief  Serializes (i.e. copy over buf pointer) the information from
	 * the variables (in that order):
	 *  - Link Delay
	 * @param  buf [out] Buffer where to put the results.
	 * @param  count [inout] Length of buffer. It contains maximum length
	 * to be written when the function is called, and the value is
	 * decremented by the same amount the buf size increases.
	 * @return TRUE if it has successfully written to buf all the values
	 * or if buf is NULL.
	 * FALSE if count should be updated with the right size.
	 */
	bool serializeState( void *buf, off_t *count ) { return true; }

	/**
	 * @brief  Restores the serialized state from the buffer. Copies the
	 * information from buffer
	 * to the variables (in that order):
	 *  - Link delay
	 * @param  buf Buffer containing the serialized state.
	 * @param  count Buffer lenght. It is decremented by the same size of
	 * the variables that are being copied.
	 * @return TRUE if everything was copied successfully, FALSE otherwise.
	 */
	bool restoreSerializedState( void *buf, off_t *count ) { return true; }

	/**
	 * @brief  Gets the local_addr
	 * @return LinkLayerAddress
	 */
	LinkLayerAddress *getLocalAddr(void) {
		return &local_addr;
	}

	/**
	 * @brief  Gets the peer address
	 * @return peer address
	 */
	LinkLayerAddress *getPeerAddr( void ) {
		return &peer_addr;
	}

	/**
	 * @brief Sets unicast messaging for media independent messages
	 */
	void setUnicastMinMessage( void )
	{
		unicast_min = true;
	}

	/**
	 * @brief  Gets the peer address
	 * @return peer address
	 */
	void setPeerAddr( LinkLayerAddress *addr ) {
		peer_addr = *addr;
	}

	// XXX Is this needed?
	void getLinkLayerAddress( LinkLayerAddress *addr ) {
		net_iface->getLinkLayerAddress( addr );
	}

	/**
	 * @brief Sends general (e.g. Announce, Signalling) 802.1AS message
	 * @param  etherType
	 * @param  buf	Buffer containing PTP message
	 * @param  len	Buffer length
	 * @param  mcast_type suggested multicast based on message type
	 * @return net_succeed on success
	 */
	net_result sendGeneralPort
	( uint16_t etherType, uint8_t * buf, int len,
	  MulticastType mcast_type )
	{
		LinkLayerAddress *dest;

		if( unicast_min )
			dest = peer_addr;
		else
			dest = 
			
	}
	virtual sendGeneralPort
	( uint16_t etherType, uint8_t * buf, int len,
	  LinkLayerAddress *dest ) = 0;
	

        /**
         * @brief  Get the PTP message offset within buffer, leaving room for
	 *		other headers as needed
         * @return PTP message offset
         */
	unsigned getPayloadOffset() const
	{
		return net_iface->getPayloadOffset();
	}

	/**
	 * @brief Process sync transaction
	 * @param  seq		PTP sequence
	 * @param  grandmaster	TRUE if we're the GM
	 * @param  elapsed_time	(output) elapsed time in ns
	 */
	virtual bool
	processSync( uint16_t seq, bool grandmaster, long *elapsed_time ) = 0;

	/**
	 * @brief Process pdelay transaction (optional)
	 * @param  seq		PTP sequence
	 * @param  grandmaster	TRUE if we're the GM
	 * @param  elapsed_time	(output) elapsed time in ns
	 */
	virtual bool
	processPDelay( uint16_t seq, long *elapsed_time );

	bool getDeviceTime
	(Timestamp &system_time, Timestamp &device_time ) const;

	virtual bool adjustClockRate( double freq_offset ) { return false; }
	virtual bool adjustClockPhase( int64_t phase_adjust ) { return false; }

	/* Suspend transmission on all ports associated with this port's
	   timestamp clock called from MediaIndependentPort */
	bool suspendTransmissionAll() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->suspendTransmission() : false;
	}
	bool resumeTransmissionAll() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->suspendTransmission() : false;
	}

	/* Callback (typically from timestamp module) to suspend tranmission on
	   this port */
	virtual bool suspendTransmission() { return false; }
	virtual bool resumeTransmission() { return false; }

	IEEE1588Clock *getClock() const {
		return port->getClock();
	}

	MediaIndependentPort *getPort() const {
		return port;
	}

	OSTimerFactory *getTimerFactory() {
		return timer_factory;
	}

	virtual FrequencyRatio getLocalSystemRateOffset() {
		return _hw_timestamper ?
			_hw_timestamper->getLocalSystemRatio() : 1.0;
	}

	virtual InterfaceLabel *getNetLabel() const {
		return net_label;
	}

	unsigned getTimestampDeviceIdx() {
		return _hw_timestamper != NULL ?
			_hw_timestamper->getDeviceIdx() : 0;
	}
	int getTimestampVersion() const {
		if( _hw_timestamper != NULL )
			return _hw_timestamper->getVersion();
		return 0;
	}
	virtual bool processEvent(Event e) { return false; }

	uint64_t getLinkDelay(void) const {
		return one_way_delay > 0LL ? one_way_delay : 0LL;
	}

	/**
	 * @brief  Sets link delay information.
	 * Signed value allows this to be negative result because
	 * of inaccurate timestamps.
	 * @param  delay Link delay
	 * @return True if one_way_delay is <= neighbor propogation delay
	 * threshold, False otherwise
	 */
	bool setLinkDelay(int64_t delay);

	/**
	 * @brief  Sets the neighbor propagation delay threshold
	 * @param  delay Delay in nanoseconds
	 * @return void
	 */
	void setNeighPropDelayThresh(int64_t delay) {
		neighbor_prop_delay_thresh = delay;
	}


	/**
	 * @brief  Sets the internal variable sync_receipt_thresh
	 * @param  th Threshold to be set
	 * @return void
	 */
	void setSyncReceiptThresh(unsigned int th)
	{
		sync_receipt_thresh = th;
	}

	/**
	 * @brief  Gets the internal variabl sync_receipt_thresh, which counts
	 * the number of wrong syncs enabled before switching
	 * the ptp to master.
	 * @return sync_receipt_thresh value
	 */
	unsigned int getSyncReceiptThresh(void)
	{
		return sync_receipt_thresh;
	}

	OSLockResult lock() {
		return glock->lock();
	}

	OSLockResult unlock() {
		return glock->unlock();
	}

	OSLockResult timestamper_lock() {
		if( _hw_timestamper )
			return _hw_timestamper->lock();
		return oslock_ok;
	}

	OSLockResult timestamper_unlock() {
		if( _hw_timestamper )
			return _hw_timestamper->unlock();
		return oslock_ok;
	}

	/**
	 * @brief Initializes the hwtimestamper
	 */
	void timestamper_init(void);

	/**
	 * @brief Resets the hwtimestamper
	 */
	void timestamper_reset(void);

	OSNetworkInterface *getNetIFace()
	{
		return net_iface;
	}

	/**
	 * @brief  Sets the wrongSeqIDCounter variable
	 * @param  cnt Value to be set
	 * @return void
	 */
	void setWrongSeqIDCounter(unsigned int cnt)
	{
		wrongSeqIDCounter = cnt;
	}

	/**
	 * @brief  Gets the wrongSeqIDCounter value
	 * @param  [out] cnt Pointer to the counter value. It must be valid
	 * @return TRUE if ok and lower than the syncReceiptThreshold value.
	 * FALSE otherwise
	 */
	bool getWrongSeqIDCounter(unsigned int *cnt)
	{
		if( cnt == NULL )
		{
			return false;
		}
		*cnt = wrongSeqIDCounter;

		return( *cnt < getSyncReceiptThresh() );
	}

	/**
	 * @brief  Increments the wrongSeqIDCounter value
	 * @param  [out] cnt Pointer to the counter value. Must be valid
	 * @return TRUE if incremented value is lower than the
	 * syncReceiptThreshold. FALSE otherwise.
	 */
	bool incWrongSeqIDCounter(unsigned int *cnt)
	{
		if( port->getAsCapable() )
		{
			wrongSeqIDCounter++;
		}
		bool ret = wrongSeqIDCounter < getSyncReceiptThresh();

		if( cnt != NULL)
		{
			*cnt = wrongSeqIDCounter;
		}

		return ret;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxSyncCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxSyncCount( void )
	{
		counters.ieee8021AsPortStatRxSyncCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxFollowUpCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxFollowUpCount( void )
	{
		counters.ieee8021AsPortStatRxFollowUpCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayRequest
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayRequest( void )
	{
		counters.ieee8021AsPortStatRxPdelayRequest++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayResponse
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayResponse( void )
	{
		counters.ieee8021AsPortStatRxPdelayResponse++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayResponseFollowUp
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayResponseFollowUp( void )
	{
		counters.ieee8021AsPortStatRxPdelayResponseFollowUp++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxAnnounce
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxAnnounce( void )
	{
		counters.ieee8021AsPortStatRxAnnounce++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPTPPacketDiscard
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPTPPacketDiscard( void )
	{
		counters.ieee8021AsPortStatRxPTPPacketDiscard++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxSyncReceiptTimeouts
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts( void )
	{
		counters.ieee8021AsPortStatRxSyncReceiptTimeouts++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatAnnounceReceiptTimeouts
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts( void )
	{
		counters.ieee8021AsPortStatAnnounceReceiptTimeouts++;
	}


	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatPdelayAllowedLostResponsesExceeded
	 * @return void
	 */
	// TODO: Not called
	void incCounter_ieee8021AsPortStatPdelayLostResponsesExceeded( void )
	{
		counters.ieee8021AsPortStatPdelayLostResponsesExceeded++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxSyncCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxSyncCount( void )
	{
		counters.ieee8021AsPortStatTxSyncCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxFollowUpCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxFollowUpCount( void )
	{
		counters.ieee8021AsPortStatTxFollowUpCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayRequest
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayRequest( void )
	{
		counters.ieee8021AsPortStatTxPdelayRequest++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayResponse
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayResponse(void) {
		counters.ieee8021AsPortStatTxPdelayResponse++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayResponseFollowUp
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayResponseFollowUp(void) {
		counters.ieee8021AsPortStatTxPdelayResponseFollowUp++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxAnnounce
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxAnnounce(void) {
		counters.ieee8021AsPortStatTxAnnounce++;
	}

	/**
	 * @brief  Logs port counters
	 * @return void
	 */
	void logIEEEPortCounters(void) {
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxSyncCount : %u",
				counters.ieee8021AsPortStatRxSyncCount );
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxFollowUpCount : %u",
				counters.ieee8021AsPortStatRxFollowUpCount );
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxPdelayRequest : %u",
				counters.ieee8021AsPortStatRxPdelayRequest );
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxPdelayResponse : %u",
				counters.ieee8021AsPortStatRxPdelayResponse );
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxPdelayResponseFollowUp : %u", counters.
				ieee8021AsPortStatRxPdelayResponseFollowUp );
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxAnnounce : %u", 
				counters.ieee8021AsPortStatRxAnnounce);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxPTPPacketDiscard : %u",
				counters.ieee8021AsPortStatRxPTPPacketDiscard);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"RxSyncReceiptTimeouts : %u", counters.
				ieee8021AsPortStatRxSyncReceiptTimeouts);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"AnnounceReceiptTimeouts : %u", counters.
				ieee8021AsPortStatAnnounceReceiptTimeouts);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"PdelayLostResponsesExceeded : %u", counters.
				ieee8021AsPortStatPdelayLostResponsesExceeded);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"TxSyncCount : %u",
				counters.ieee8021AsPortStatTxSyncCount);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"TxFollowUpCount : %u",
				counters.ieee8021AsPortStatTxFollowUpCount);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"TxPdelayRequest : %u",
				counters.ieee8021AsPortStatTxPdelayRequest);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"TxPdelayResponse : %u",
				counters.ieee8021AsPortStatTxPdelayResponse);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"TxPdelayResponseFollowUp : %u", counters.
				ieee8021AsPortStatTxPdelayResponseFollowUp);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStat"
				"TxAnnounce : %u",
				counters.ieee8021AsPortStatTxAnnounce);
	}

};

inline MediaDependentPort::~MediaDependentPort() { }

inline bool MediaDependentPort::
init_port( MediaIndependentPort *port, IEEE1588PortInit_t *portInit )
{
	this->port = port;
	port->setPort( this );
	return true;
}

#endif/*MDPORT_HPP*/
