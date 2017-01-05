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

#ifndef MIN_PORT_HPP
#define MIN_PORT_HPP

#include <ieee1588.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <avbap_message.hpp>

#include <stdint.h>

#include <list>

#define GPTP_MULTICAST 0x0180C200000EULL		/*!< GPTP multicast adddress */
#define OTHER_MULTICAST GPTP_MULTICAST			/*!< OTHER multicast value */

#define PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER 3	/*!< PDelay timeout multiplier*/
#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 3			/*!< Sync receipt timeout multiplier*/
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 3		/*!< Announce receipt timeout multiplier*/

#define LOG2_INTERVAL_INVALID -127	/* Simple out of range Log base 2 value used for Sync and PDelay msg internvals */

class MediaDependentPort;
class PTPMessageAnnounce;

typedef enum {
	V1,
	V2_E2E,
	V2_P2P
} PortType;

/**
 * @brief PortIdentity interface
 * Defined at IEEE 802.1AS Clause 8.5.2
 */
class PortIdentity {
private:
	ClockIdentity clock_id;
	uint16_t portNumber;
public:
	/**
	 * @brief Default Constructor
	 */
	PortIdentity() { };

	/**
	 * @brief  Constructs PortIdentity interface.
	 * @param  clock_id Clock ID value as defined at IEEE 802.1AS
	 * Clause 8.5.2.2
	 * @param  portNumber Port Number
	 */
	PortIdentity(uint8_t * clock_id, uint16_t * portNumber) {
		this->portNumber = *portNumber;
		this->portNumber = PLAT_ntohs(this->portNumber);
		this->clock_id.set(clock_id);
	}

	/**
	 * @brief  Implements the operator '!=' overloading method. Compares
	 *clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value differs from the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator!=(const PortIdentity & cmp) const {
		return
			!(this->clock_id == cmp.clock_id) ||
			this->portNumber != cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '==' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value equals to the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator==(const PortIdentity & cmp)const {
		return
			this->clock_id == cmp.clock_id &&
			this->portNumber == cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '<' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value is lower than the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator<(const PortIdentity & cmp)const {
		return
			this->clock_id < cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber < cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '>' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value is greater than the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator>(const PortIdentity & cmp)const {
		return
			this->clock_id > cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber > cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Gets the ClockIdentity string
	 * @param  id [out] Pointer to an array of octets.
	 * @return void
	 */
	void getClockIdentityString(uint8_t *id) {
		clock_id.getIdentityString(id);
	}

	/**
	 * @brief  Sets the ClockIdentity.
	 * @param  clock_id Clock Identity to be set.
	 * @return void
	 */
	void setClockIdentity(ClockIdentity clock_id) {
		this->clock_id = clock_id;
	}

	/**
	 * @brief  Gets the clockIdentity value
	 * @return A copy of Clock identity value.
	 */
	ClockIdentity getClockIdentity( void ) {
		return this->clock_id;
	}

	/**
	 * @brief  Gets the port number following the network byte order, i.e.
	 * Big-Endian.
	 * @param  id [out] Port number
	 * @return void
	 */
	void getPortNumberNO(uint16_t * id) {	// Network byte order
		uint16_t portNumberNO = PLAT_htons(portNumber);
		*id = portNumberNO;
	}

	/**
	 * @brief  Gets the port number in the host byte order, which can be
	 * either Big-Endian
	 * or Little-Endian, depending on the processor where it is running.
	 * @param  id Port number
	 * @return void
	 */
	void getPortNumber(uint16_t * id) {	// Host byte order
		*id = portNumber;
	}

	/**
	 * @brief  Sets the Port number
	 * @param  id [in] Port number
	 * @return void
	 */
	void setPortNumber(uint16_t * id) {
		portNumber = *id;
	}
};

class MediaIndependentPort {
	PortIdentity port_identity;

	// Port Status
	//
	// 0 for master, ++ for each sync receive as slave
	unsigned sync_count;

	// set to 0 when asCapable is false, increment for each pdelay recvd
	unsigned pdelay_count;

	// Port Configuration
	PortState port_state;

	uint8_t log_sync_interval;
	uint8_t log_announce_interval;
	uint8_t log_pdelay_req_interval;

	// Automotive Profile : Static variables
	bool testMode;
	char initialLogPdelayReqInterval;
	char initialLogSyncInterval;
	char operLogPdelayReqInterval;
	char operLogSyncInterval;

	// Physical interface number that the object represents
	uint16_t ifindex;
	bool automotive_profile;

	StationState_t stationState;

	// Implementation Specific data/methods
	IEEE1588Clock *clock;

	bool asCapable;

	// Automotive Profile AVB SYNC state indicator
	//
	// > 0 will inditate valid AVB SYNC state
	uint32_t avbSyncState;

	FrequencyRatio _peer_rate_offset;
	Timestamp _peer_offset_ts_theirs;
	Timestamp _peer_offset_ts_mine;
	bool _peer_offset_init;

	bool _local_system_freq_offset_init;
	Timestamp _prev_local_time;
	Timestamp _prev_system_time;

	PTPMessageAnnounce *qualified_announce;

	uint16_t announce_sequence_id;
	uint16_t signal_sequence_id;
	uint16_t sync_sequence_id;
	uint16_t pdelay_sequence_id;

	OSLock *syncReceiptTimerLock;
	OSLock *syncIntervalTimerLock;
	OSLock *announceIntervalTimerLock;
	OSLock *pDelayIntervalTimerLock;

	bool pdelay_started;
	bool pdelay_halted;
	bool sync_rate_interval_timer_started;

	bool _new_syntonization_set_point;

	uint16_t lastGmTimeBaseIndicator;

	OSCondition *port_ready_condition;

	OSThreadFactory *thread_factory;
	OSTimerFactory *timer_factory;
	OSLockFactory *lock_factory;
	OSConditionFactory *condition_factory;

	OSLock *glock;

	MediaDependentPort *inferior;

	/**
	 * @brief	Process state change event for non-automotive profile
	 *	GM-capable configurations
	 * @return	false on failure
	 */
	bool processStateChangeEvent( void );

	/**
	 * @brief	Process sync/announce receipt timeout for
	 *	non-automotive profile
	 * @return	false on failure
	 */
	bool processSyncAnncReceiptTimeout( void );

	/**
	 * @brief	Process sync interval timeout
	 * @return	false on failure
	 */
	bool processSyncIntervalTimeout( void );

public:
	static const LinkLayerAddress other_multicast;

	/**
	 * @brief  Creates the MediaIndependentPort obect
	 * @param  init IEEE1588PortInit initialization parameters
	 */
	MediaIndependentPort(IEEE1588PortInit_t *portInit);

	/**
	 * @brief Finalizes cleanup of MediaIndependentPort
	 */
	~MediaIndependentPort();

	/**
	 * @brief  Serializes (i.e. copy over buf pointer) the information from
	 * the variables (in that order):
	 *  - asCapable;
	 *  - Port State;
	 *  - Neighbor Rate Ratio
	 * Calls media dependent port serialize
	 * @param  buf [out] Buffer where to put the results.
	 * @param  count [inout] Length of buffer. It contains maximum length
	 * to be written when the function is called, and the value is
	 * decremented by the same amount the buf size increases.
	 * @return TRUE if it has successfully written to buf all the values
	 * or if buf is NULL.
	 * FALSE if count should be updated with the right size.
	 */
	bool serializeState( void *buf, long *count );

	/**
	 * @brief  Restores the serialized state from the buffer. Copies the
	 * information from buffer
	 * to the variables (in that order):
	 *  - asCapable;
	 *  - Port State;
	 *  - Neighbor Rate Ratio
	 * Calls media dependent port de-serialize
	 * @param  buf Buffer containing the serialized state.
	 * @param  count Buffer lenght. It is decremented by the same size of
	 * the variables that are being copied.
	 * @return TRUE if everything was copied successfully, FALSE otherwise.
	 */
	bool restoreSerializedState( void *buf, long *count );

        /**
         * @brief	Gets testmode configuration
         * @return	true if testmode is configured
         */
	bool isInTestMode() {
		return testMode;
	}

	/**
	 * @brief  Starts pDelay event timer if not yet started.
	 * @return void
	 */
	void syncDone();

	/**
	 * @brief  Initializes the port. Creates network interface, initializes
	 * hardware timestamper and create OS locks conditions
	 * @return FALSE if error during building the interface.
	 * TRUE if success
	 */
	bool init_port(IEEE1588PortInit_t *portInit);

	/**
	 * @brief Receives messages from the network interface
	 * @return Its an infinite loop. Returns NULL in case of error.
	 */
	bool openPort();

	/**
	 * @brief Watch for link up and down events.
	 * @return Its an infinite loop. Returns NULL in case of error.
	 */
	void *watchNetLink(void);

	/**
	 * @brief  Currently doesnt do anything. Just returns.
	 * @return void
	 */
	bool recoverPort();

	/**
	 * @brief  Switches port to a gPTP master
	 * @param  annc If TRUE, starts announce event timer.
	 * @return void
	 */
	void becomeMaster( bool annc );

	/**
	 * @brief  Switches port to a gPTP slave.
	 * @param  restart_syntonization if TRUE, restarts the syntonization
	 * @return void
	 */
	void becomeSlave( bool );

	/**
	 * @brief  Starts pDelay event timer.
	 * @return void
	 */
	bool startPDelay();

	/**
	 * @brief Stops PDelay event timer
	 * @return void
	 */
	void stopPDelay();

	/**
	 * @brief Enable/Disable PDelay Request messages
	 * @param hlt True to HALT (stop sending), False to resume
	 * (start sending).
	 */
	void haltPdelay(bool hlt)
	{
		pdelay_halted = hlt;
	}

	/**
	 * @brief Get the status of pdelayHalted condition.
	 * @return True PdelayRequest halted. False when PDelay Request is
	 * running
	 */
	bool pdelayHalted(void)
	{
		return pdelay_halted;
	}

        /**
         * @brief  Start sync interval timer
         * @param  waitTime time interval in nanoseconds
         * @return none
         */
        void startSyncIntervalTimer(long long unsigned int waitTime);

	/**
	 * @brief  Starts Sync Rate Interval event timer. Used for the
	 *         Automotive Profile.
	 * @return void
	 */
	void startSyncRateIntervalTimer();

	/**
	 * @brief  Starts announce event timer
	 * @return void
	 */
	bool startAnnounce();

	void stopSyncReceiptTimeout();
	void startSyncReceiptTimeout();

	/**
	 * @brief  Gets a pointer to timer_factory object
	 * @return timer_factory pointer
	 */
	OSTimerFactory *getTimerFactory() const {
		return timer_factory;
	}

	/**
	 * @brief  Restart PDelay
	 * @return void
	 */
	void restartPDelay() {
		_peer_offset_init = false;
	}

	/**
	 * @brief  Sets asCapable flag
	 * @param  ascap flag to be set. If FALSE, marks peer_offset_init as
	 * false.
	 * @return void
	 */
	void setAsCapable();

	/**
	 * @brief  Gets the asCapable flag
	 * @return asCapable flag.
	 */
	bool getAsCapable() {
		return asCapable;
	}

	/**
	 * @brief  Gets the AVnu automotive profile flag
	 * @return automotive_profile flag
	 */
	bool getAutomotiveProfile() { return( automotive_profile ); }

	/**
	 * @brief  Process all events for a IEEE1588Port
	 * @param  e Event to be processed
	 * @return void
	 */
	bool processEvent(Event e);

	/**
	 * @brief  Gets the "best" announce
	 * @return Pointer to PTPMessageAnnounce
	 */
	PTPMessageAnnounce *calculateERBest(void);

	/**
	 * @brief  Gets the sync interval value
	 * @return Sync Interval
	 */
	char getSyncInterval(void) const {
		return log_sync_interval;
	}

	/**
	 * @brief  Sets the sync interval value
	 * @param  val time interval
	 * @return none
	 */
	void setSyncInterval(char val) {
		log_sync_interval = val;
	}

	/**
	 * @brief  Sets the sync interval back to initial value
	 * @return none
	 */
	void setInitSyncInterval(void) {
		log_sync_interval = initialLogSyncInterval;;
	}

	/**
	 * @brief  Gets the announce interval
	 * @return Announce interval
	 */
	char getAnnounceInterval(void) {
		return log_announce_interval;
	}

	/**
	 * @brief  Sets the announce interval
	 * @param  val time interval
	 * @return none
	 */
	void setAnnounceInterval(char val) {
		log_announce_interval = val;
	}

	/**
	 * @brief  Gets the pDelay minimum interval
	 * @return PDelay interval
	 */
	char getPDelayInterval(void) {
		return log_pdelay_req_interval;
	}

	/**
	 * @brief  Sets the pDelay minimum interval
	 * @param  val time interval
	 * @return none
	 */
	void setPDelayInterval(char val) {
		log_pdelay_req_interval = val;
	}

	/**
	 * @brief  Sets the pDelay minimum interval back to initial
	 *         value
	 * @return none
	 */
	void setInitPDelayInterval(void) {
		log_pdelay_req_interval = initialLogPdelayReqInterval;
	}

	/**
	 * @brief  Gets the portState information
	 * @return PortState
	 */
	PortState getPortState(void) const {
		return port_state;
	}

	/**
	 * @brief Sets the PortState
	 * @param state value to be set
	 * @return void
	 */
	void setPortState( PortState state ) {
		port_state = state;
	}

	/**
	 * @brief  Gets port identity
	 * @param  identity [out] Reference to PortIdentity
	 * @return void
	 */
	void getPortIdentity(PortIdentity & identity) const {
		identity = this->port_identity;
	}

	/**
	 * @brief  Increments announce sequence id and returns
	 * @return Next announce sequence id.
	 */
	uint16_t getNextAnnounceSequenceId() {
		return announce_sequence_id++;
	}

	/**
	 * @brief  Increments sync sequence ID and returns
	 * @return Next synce sequence id.
	 */
	uint16_t getNextSyncSequenceId() {
		return sync_sequence_id++;
	}

	/**
	 * @brief  Increments PDelay sequence ID and returns.
	 * @return Next PDelay sequence id.
	 */
	uint16_t getNextPDelaySequenceId() {
		return pdelay_sequence_id++;
	}

	/**
	 * @brief  Gets a pointer to IEEE1588Clock
	 * @return Pointer to clock
	 */
	IEEE1588Clock *getClock(void) const {
		return clock;
	}

	/**
	 * @brief  Sets the Station State for the Test Status message
	 * @param  StationState_t [in] The station state
	 * @return none
	 */
	void setStationState(StationState_t _stationState) {
		stationState = _stationState;
		if (stationState == STATION_STATE_ETHERNET_READY) {
			GPTP_LOG_STATUS("AVnu AP Status : "
					"STATION_STATE_ETHERNET_READY");
		}
		else if (stationState == STATION_STATE_AVB_SYNC) {
			GPTP_LOG_STATUS("AVnu AP Status : "
					"STATION_STATE_AVB_SYNC");
		}
		else if (stationState == STATION_STATE_AVB_MEDIA_READY) {
			GPTP_LOG_STATUS("AVnu AP Status : "
					"STATION_STATE_AVB_MEDIA_READY");
		}
	}

	/**
	 * @brief  Gets the Station State for the Test Status
	 *         message
	 * @return station state
	 */
	StationState_t getStationState() {
		return stationState;
	}

        /**
         * @brief	Gets the Peer rate offset. Used to calculate neighbor
	 * 	rate ratio.
         * @return	FrequencyRatio peer rate offset
         */
	FrequencyRatio getPeerRateOffset(void) const {
		return _peer_rate_offset;
	}

	/**
	 * @brief  Sets the peer rate offset. Used to calculate neighbor rate
	 * ratio.
	 * @param  offset Offset to be set
	 * @return void
	 */
	void setPeerRateOffset( FrequencyRatio offset ) {
		_peer_rate_offset = offset;
	}

	/**
	 * @brief  Sets current peer offset timestamps
	 * @param  mine Local timestamps
	 * @param  theirs Remote timestamps
	 * @return void
	 */
	void setPeerOffset(Timestamp mine, Timestamp theirs) {
		_peer_offset_ts_mine = mine;
		_peer_offset_ts_theirs = theirs;
		_peer_offset_init = true;
	}

	/**
	 * @brief  Gets current peer offset timestamps
	 * @param  mine [out]	Local timestamps
	 * @param  theirs [out]	Remote timestamps
	 * @return false if uninitialized
	 */
	bool getPeerOffset(Timestamp & mine, Timestamp & theirs) const {
		if( _peer_offset_init ) {
			mine = _peer_offset_ts_mine;
			theirs = _peer_offset_ts_theirs;
		}
		return _peer_offset_init;
	}

	/**
	 * @brief	Make adjustment to PI controller
	 * @param	master_local_offset phase error
	 * @param	master_local_freq_offset frequency error
	 * @return	False if failure occurs
	 */
	bool adjustPhaseError( int64_t master_local_offset,
			       FrequencyRatio master_local_freq_offset );

	/**
	 * @brief Restart syntonization - reset PI controller
	 */
	void newSyntonizationSetPoint() {
		_new_syntonization_set_point = true;
	}

        /**
         * @brief	Changes the port state
         * @param	state Current state
         * @param	changed_external_master TRUE if external master has
	 *	changed, FALSE otherwise
         * @return	void
         */
	void recommendState(PortState state, bool changed_external_master);

	/**
	 * @brief  Adds a new qualified announce the port. IEEE 802.1AS Clause
	 * 10.3.10.2
	 * @param  msg PTP announce message
	 * @return void
	 */
	void addQualifiedAnnounce(PTPMessageAnnounce *msg);

	/**
	 * @brief	Get associated media dependent port object
	 * @return	associated media dependent port, NULL if uninitialized
	 */
	MediaDependentPort *getPort() {
		return inferior;
	}

	/**
	 * @brief	Set associated media dependent port object
	 * @param	associated media dependent port
	 */
	void setPort( MediaDependentPort *port ) {
		this->inferior = port;
	}

	/**
	 * @brief	Lock port state
	 * @return	oslock_ok on success
	 */
	OSLockResult lock() {
		return glock->lock();
	}

	/**
	 * @brief	Unlock port state
	 * @return	oslock_ok on success
	 */
	OSLockResult unlock() {
		return glock->unlock();
	}

	/**
	 * @brief  Sets current pdelay count value.
	 * @param  cnt [in] pdelay count value
	 * @return void
	 */
	void setPdelayCount(unsigned int cnt) {
		pdelay_count = cnt;
	}

	/**
	 * @brief  Increments Pdelay count
	 * @return void
	 */
	void incPdelayCount() {
		++pdelay_count;
	}

	/**
	 * @brief  Gets current pdelay count value. It is set to zero
	 * when asCapable is false.
	 * @return pdelay count
	 */
	unsigned getPdelayCount() {
		return pdelay_count;
	}

	/**
	 * @brief  Sets current sync count value.
	 * @param  cnt [in] sync count value
	 * @return void
	 */
	void setSyncCount(unsigned int cnt) {
		sync_count = cnt;
	}

	/**
	 * @brief  Increments sync count
	 * @return void
	 */
	void incSyncCount() {
		++sync_count;
	}

	/**
	 * @brief  Gets current sync count value. It is set to zero
	 * when master and incremented at each sync received for slave.
	 * @return sync count
	 */
	unsigned getSyncCount() {
		return sync_count;
	}
};

#endif/*MIN_PORT_HPP*/
