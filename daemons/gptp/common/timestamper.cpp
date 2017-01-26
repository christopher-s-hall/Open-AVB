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

#include <timestamper.hpp>
#include <mdport.hpp>
#include <avbts_osthread.hpp>
#include <debugout.hpp>
#include <vector>

unsigned Timestamper::index = 0;

#define MIN_POLL_INTERVAL (16) /*ms*/
#define EST_POLL_INTERVAL (50) /*ms*/
#define POLL_INTERVAL (750) /*ms*/

typedef std::vector<MediaDependentPort *> PortList;
typedef PortList::const_iterator PortListIter;

struct TimestamperPortIter
{
	PortListIter iter;
};

struct TimestamperPortList
{
	PortList list;
};

void Timestamper::getPortIter( TimestamperPortIter_t *iter )
{
	iter = std::make_unique<struct TimestamperPortIter>();
	iter->iter = port_list->list.cbegin();
}

MediaDependentPort *Timestamper::getNextPort( TimestamperPortIter_t iter )
{
	MediaDependentPort *ret;

	if( iter->iter != port_list->list.cend() )
	{
		ret = *iter->iter;
		++iter->iter;
	} else
		ret = NULL;

	return ret;
}

bool Timestamper::registerPort( MediaDependentPort *port )
{
	if( lock() != oslock_ok ) return false;
	port_list->list.push_back( port );
	if( unlock() != oslock_ok ) return false;
	return true;
}

OSThreadExitCode TimestamperThreadFunction( void *arg_in ) {
	Timestamper *timestamper = (Timestamper *) arg_in;
	OSTimer *timer = timestamper->getTimerFactory()->createTimer();
	int i;

	Timestamp prev_sys, prev_net;
	
	// Wait approximately 1 second (1000 ms); execute at least once
	for( i = 0; i < (1000/MIN_POLL_INTERVAL)+1; ++i ) {
		if( timestamper->HWTimestamper_gettime( &prev_sys, &prev_net )) {
			break;
		}
		timer->sleep( MIN_POLL_INTERVAL*1000 );
	}
	if( i == (1000/MIN_POLL_INTERVAL)+1 ) {
		return osthread_error;
	}
	timer->sleep( EST_POLL_INTERVAL*1000 );
	while( true ) {
		Timestamp sys, net;
		FrequencyRatio local_system_ratio;

		if( !timestamper->HWTimestamper_gettime( &sys, &net )) {
			return osthread_error;
		}
		local_system_ratio  = (long double) TIMESTAMP_TO_NS(net - prev_net);
		local_system_ratio /= TIMESTAMP_TO_NS(sys - prev_sys);
		timestamper->lock();
		timestamper->setLocalSystemRatio( local_system_ratio );
		timestamper->unlock();
		XPTPD_WDEBUG("local_system_ratio=%Lf(Net(N)=%llu,Net(N-1)=%llu,System(N)=%llu,System(N-1)=%llu)",
			local_system_ratio, TIMESTAMP_TO_NS(net), TIMESTAMP_TO_NS(prev_net), TIMESTAMP_TO_NS(sys), TIMESTAMP_TO_NS(prev_sys));

		XPTPD_INFOL
			( TIMESTAMP_DEBUG, "local system ratio: %Lf(%llu,%s,%s)\n",
			  local_system_ratio, TIMESTAMP_TO_NS(net - prev_net),
			  sys.toString(),prev_sys.toString());

		prev_sys = sys;
		prev_net = net;
		timer->sleep( POLL_INTERVAL*1000 );
	}

	return osthread_ok;
}

bool Timestamper::HWTimestamper_init
( InterfaceLabel *iface_label, OSNetworkInterface *iface,
  OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
  OSTimerFactory *timer_factory ) {
	if (!initialized) {
		// One time stuff
		if (!(thread = thread_factory->createThread())) {
			return false;
		}
		this->lock_factory = lock_factory;
		this->timer_factory = timer_factory;
		this->thread_factory = thread_factory;

		if (!(glock = lock_factory->createLock(oslock_nonrecursive, "timestamper", true))) {
			return false;
		}

		if (!(thread->start(TimestamperThreadFunction, this))) {
			return false;
		}
		initialized = true;
	}

	if(iface != NULL ) iface_list.push_front(iface);

	port_list = new TimestamperPortList;
	if( port_list == NULL )
	{
		GPTP_LOG_CRITICAL( "Out of memory" );
		return false;
	}
	
	return true;
}

Timestamper::Timestamper()
{
	version = 0;
	idx = gl_idx++;
	local_system_ratio = 1.0;
	port_list = make_unique<struct TimestamperPortList>;
	
	initialized = false;
}

bool Timestamper::suspendTransmission() {
	TimetamperPortIter *iter;
	MediaIndependentPort *port;
	bool ret = true;
	
	GetPortIter( &iter );

	port = GetNextPort( iter );
	while( port != NULL )
	{
		if( !port->suspendTransmission()) {
			ret = false;
			goto do_exit;
		}
		port = GetNextPort( iter );
	}

do_exit:
	PutPortIter( &iter );
	return ret;
}

bool Timestamper::resumeTransmission() {
	TimetamperPortIter *iter;
	MediaIndependentPort *port;
	bool ret = true;
	
	GetPortIter( &iter );

	port = GetNextPort( iter );
	while( port != NULL )
	{
		if( !(*curr)->resumeTransmission()) {
			ret = false;
			goto do_exit;
		}
	}

do_exit:
	PutPortIter( &iter );
	return ret;
}

