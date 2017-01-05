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

#ifndef TIMESTAMPER_HPP
#define TIMESTAMPER_HPP

#include <avbts_oslock.hpp>
#include <ptptypes.hpp>
#include <stdint.h>
#include <avbts_osnet.hpp>


class MediaDependentPort;
class InterfaceLabel;
class OSNetworkInterface;
class Timestamp;
class PortIdentity;
class OSThread;
class OSThreadFactory;
class OSTimerFactory;

typedef std::unique_ptr<struct TimestamperPortList> TimestamperPortList_t;
typedef std::unique_ptr<struct TimestamperPortIter> TimestamperPortIter_t;

/* id identifies the timestamper 0 is reserved meaning no timestamper is 
   availabled */

/**
 * @brief Provides a generic interface for hardware timestamping
 */
class Timestamper {
private:
	TimestamperPortList_t port_list;	//!< List of ports using this timestamper

	unsigned idx;				//!< Denotes HW timestamp origin
	static unsigned gl_idx;			//!< Uniquely assigns idx to each object

	FrequencyRatio local_system_ratio;
protected:
	bool initialized;	//!< Set to true, when init is complete
	uint8_t version;	//!< incremented HW clock phase is adjusted

	OSLock *glock;
	OSThread *thread;

	OSTimerFactory *timer_factory;
	OSLockFactory *lock_factory;
	OSThreadFactory *thread_factory;

	virtual bool _adjclockphase( int64_t phase_adjust ) { return false; }

public:
	/**
	 * @brief Initializes the hardware timestamp object
	 * @param iface_label [in] Interface label
	 * @param iface [in] Network interface
	 * @param lock_factory [in] lock creation
	 * @param thread_factory [in] thread creation
	 * @param timer_factory [in] timer creation
	 * @return true
	 */
	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface,
	  OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
	  OSTimerFactory *timer_factory );

	/**
	 * @brief  This method is called before the object is de-allocated.
	 * @return void
	 */
	virtual void HWTimestamper_final( void ) { }

	/**
	 * @brief Reset the hardware timestamp unit
	 * @return void
	 */
	virtual void HWTimestamper_reset(void) {
	}

	/**
	 * @brief  Adjusts the hardware clock frequency
	 * @param  frequency_offset Frequency offset
	 * @return false
	 */
	virtual bool HWTimestamper_adjclockrate( float frequency_offset )
	{ return false; }

	/**
	 * @brief  Adjusts the hardware clock phase
	 * @param  phase_adjust Phase offset
	 * @return false
	 */
	bool HWTimestamper_adjclockphase( int64_t phase_adjust )
	{ return false; }

	/**
	 * @brief  Get the cross timestamping information.
	 * The gPTP subsystem uses these samples to calculate
	 * ratios which can be used to translate or extrapolate
	 * one clock into another clock reference. The gPTP service
	 * uses these supplied cross timestamps to perform internal
	 * rate estimation and conversion functions.
	 * @param  system_time [out] System time
	 * @param  device_time [out] Device time
	 * @return True in case of success. FALSE in case of error
	 */
	virtual bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp *device_time ) = 0;

	/**
	 * @brief  Starts the PPS (pulse per second) interface
	 * @return false
	 */
	virtual bool HWTimestamper_PPS_start()
	{ return false; }

	/**
	 * @brief  Stops the PPS (pulse per second) interface
	 * @return true
	 */
	virtual bool HWTimestamper_PPS_stop()
	{ return true; }

	bool suspendTransmission();
	bool resumeTransmission();

	int getVersion() {
		return version;
	}

	 /**
	 * @brief Default constructor. Sets default value.
	 */
	Timestamper();
	virtual ~Timestamper() { }

	virtual OSLockResult lock() { return glock->lock(); }
	virtual OSLockResult unlock() { return glock->unlock(); }

	MediaDependentPort *getNextPort( TimestamperPortIter_t *iter );

	void getPortIter( TimestamperPortIter_t *iter );
	bool registerPort( MediaDependentPort *port );

	unsigned getDeviceIdx() { return idx; }

	FrequencyRatio getLocalSystemRatio() {
		return local_system_ratio;
	}
};

unsigned Timestamper::gl_idx = 0;

#endif/*TIMESTAMPER_HPP*/
