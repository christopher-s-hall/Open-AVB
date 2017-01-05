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

#include <mdport.hpp>

MediaDependentPort::MediaDependentPort(IEEE1588PortInit_t *portInit);
{
	this->timer_factory	= portInit->timer_factory;
	this->thread_factory	= portInit->thread_factory;

	this->net_label 	= portInit->net_label;

	this->condition_factory	= portInit->condition_factory;
	this->lock_factory	= portInit->lock_factory;

	this->_hw_timestamper	= portInit->timestamper;

	unicast_min = false;	// Default to multicast messaging

	one_way_delay = ONE_WAY_DELAY_DEFAULT;
}

void MediaDependentPort::timestamper_init( void )
{
        if( _hw_timestamper != NULL ) {
                _hw_timestamper->init_phy_delay(this->link_delay);
                if( !_hw_timestamper->HWTimestamper_init
		    ( net_label, net_iface ))
		{
                        GPTP_LOG_ERROR
                                ( "Failed to initialize hardware timestamper, "
                                  "falling back to software timestamping" );
                        _hw_timestamper = NULL;
                        return;
                }
        }
}

bool MediaDependentPort::init_port
	( MediaIndependentPort *port, IEEE1588PortInit_t *portInit )
{
	glock = lock_factory->createLock( oslock_nonrecursive );
	if( glock == NULL )
		return false;

	if (!OSNetworkInterfaceFactory::buildInterface
	    ( &net_iface, factory_name_t("default"), net_label,
	      _hw_timestamper))
		return false;

	this->port = port;
	net_iface->getLinkLayerAddress(&local_addr);
	port->getClock()->setClockIdentity(&local_addr);

	timestamper_init();

	return _init_port( port, portInit );
}

OSThreadExitCode watchNetLinkWrapper( void *arg_in )
{
	OSThreadExitCode ret = osthread_ok;
	WatchNetLinkWrapperArg *arg = (WatchNetLinkWrapperArg *) arg_in;
	tristate_t err;

	err = arg->port->watchNetLink( arg->ready );
	ret = err != false ? ret : osthread_error;

	return ret;
}

OSThreadExitCode listenPortWrapper( void *arg_in )
{
	OSThreadExitCode ret = osthread_ok;
	ListenPortWrapperArg *arg = (ListenPortWrapperArg *) arg_in;
	tristate_t err;

	err = (tristate_t) arg->port->listenPort( arg->ready );
	ret = err != false ? ret : osthread_error;

	return ret;
}

bool MediaDependentEtherPort::startPort() {
	WatchNetLinkWrapperArg lk_arg;
	ListenPortWrapperArg ln_arg;
	bool ret = true;

	listening_thread = thread_factory->createThread();
	link_thread = thread_factory->createThread();
	if( listening_thread == NULL || link_thread == NULL )
	{
		GPTP_LOG_ERROR("Error creating port thread");
		ret = false;
		goto 
	}

	ln_arg.port = this;
	ln_arg.ready = condition_factory->createCondition();

	lk_arg.port = this;
	lk_arg.ready = condition_factory->createCondition();

	if( lk_arg->ready == NULL || ln_arg->ready == NULL )
	{
		GPTP_LOG_ERROR("Error creating port thread condition");
		ret = false;
		goto do_condition_fail;
	}

	if( !lk_arg->ready->wait_prelock() || !ln_arg->ready->wait_prelock() )
	{
		GPTP_LOG_ERROR("Error prelock thread condition");
		ret = false;
		goto do_condition_fail;
	}

	if( !link_thread->start( watchNetLinkWrapper, (void *)lk_arg ))
	{
		GPTP_LOG_ERROR("Error starting port link thread");
		ret = false;
		goto do_start_fail;
	}
	else
	{
		lk_arg->ready->wait();
	}

	
	if ( !listening_thread->start( listenPortWrapper, (void *)ln_arg ))
	{
		GPTP_LOG_ERROR("Error creating port watch thread");
		ret = false;
		goto do_start_fail;
	}
	else
	{
		ln_arg->ready->wait();
	}

do_start_fail:
do_cond_fail:
	delete lk_arg.ready;
	delete ln_arg.ready;
do_thread_fail:
	if( ret == false )
	{
		delete listening_thread;
		delete link_thread;
	}

	return ret;
}

bool MediaDependentPort::setLinkDelay(int64_t delay);
{
	one_way_delay = delay;
	int64_t abs_delay =
		(one_way_delay < 0 ? -one_way_delay : one_way_delay);

	if (port->isInTestMode()) {
		GPTP_LOG_STATUS("Link delay: %d", delay);
	}

	return (abs_delay <= neighbor_prop_delay_thresh);
}

bool MediaDependentPort::processPDelay( uint16_t seq, long *elapsed_time )
{
	port->getClock()->
		deleteEventTimer
		(port, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES);

	return true;
}

bool MediaDependentPort::getDeviceTime
(Timestamp &system_time, Timestamp &device_time ) const
{
	if (_hw_timestamper) {
		return _hw_timestamper->HWTimestamper_gettime
			( &system_time, &device_time );
	}

	device_time = system_time = getClock()->getSystemTime();
	return true;
}

