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

#ifndef ETHTIMESTAMPER_HPP
#define ETHTIMESTAMPER_HPP

#include <avbts_oslock.hpp>
#include <ptptypes.hpp>
#include <vector>
#include <list>
#include <stdint.h>
#include <avbts_osnet.hpp>
#include <timestamper.hpp>


class MediaDependentPort;
class InterfaceLabel;
class OSNetworkInterface;
class Timestamp;
class PortIdentity;
class OSThread;
class OSThreadFactory;
class OSTimerFactory;

/* id identifies the timestamper 0 is reserved meaning no timestamper is 
   availabled */

class EthernetTimestamper : public Timestamper {
private:
	struct phy_delay phy_delay;

public:
	bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface,
	  OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
	  OSTimerFactory *timer_factory );

	bool HWTimestamper_adjclockphase( int64_t phase_adjust );
	virtual bool clear_rx_timestamp_list() { return false; }

	EthernetTimestamper() { }
	virtual ~EthernetTimestamper() {}

	virtual int HWTimestamper_txtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  bool last ) = 0;

	virtual int HWTimestamper_rxtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  bool last ) = 0;

	/**
	 * @brief Initializes the PHY delay for TX and RX
	 * @param [input] mb_tx_phy_delay, mb_rx_phy_delay, gb_tx_phy_delay,
		gb_rx_phy_delay
	 * @return 0
	 **/
	 int init_phy_delay(int phy_delay[4])
	 {
		this->phy_delay.gb_tx_phy_delay = phy_delay[0];
		this->phy_delay.gb_rx_phy_delay = phy_delay[1];
		this->phy_delay.mb_tx_phy_delay = phy_delay[2];
		this->phy_delay.mb_rx_phy_delay = phy_delay[3];


		return 0;
	 }

	 /**
	  * @brief Returns the the PHY delay for TX and RX
	  * @param [input] struct phy_delay  pointer
	  * @return 0
	  **/
	 int get_phy_delay (struct phy_delay *get_delay)
	 {
		get_delay->mb_tx_phy_delay = phy_delay.mb_tx_phy_delay;
		get_delay->mb_rx_phy_delay = phy_delay.mb_rx_phy_delay;
		get_delay->gb_tx_phy_delay = phy_delay.gb_tx_phy_delay;
		get_delay->gb_rx_phy_delay = phy_delay.gb_rx_phy_delay;

		return 0;
	 }

	 /**
	 * @brief Sets the the PHY delay for TX and RX
	 * @param [input] struct phy_delay  pointer
	 * @return 0
	 **/
	 int set_phy_delay(struct phy_delay *set_delay)
	 {
		 phy_delay.mb_tx_phy_delay = set_delay->mb_tx_phy_delay;
		 phy_delay.mb_rx_phy_delay = set_delay->mb_rx_phy_delay;
		 phy_delay.gb_tx_phy_delay = set_delay->gb_tx_phy_delay;
		 phy_delay.gb_rx_phy_delay = set_delay->gb_rx_phy_delay;

		 return 0;
	 }
};


#endif/*ETHTIMESTAMPER_HPP*/
