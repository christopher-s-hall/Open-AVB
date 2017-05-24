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

#include <wireless_port.hpp>

OSThreadExitCode openPortWrapper(void *arg);

WirelessPort::WirelessPort( PortInit_t *portInit ) :
	CommonPort( portInit )
{
}

WirelessPort::~WirelessPort()
{
	delete port_ready;
}

bool WirelessPort::_init_port( void )
{
	port_ready = condition_factory->createCondition();

	return true;
}

OSThreadExitCode WirelessPort::openPort( void *parg )
{
	WirelessPort *port = (WirelessPort *) parg;
	
	port->port_ready->signal();

	while (1) {
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);

		if ( ( rrecv = port->recv( &remote, buf, length ))
		     == net_succeed )
		{
			port->processMessage
				((char *)buf, (int)length, &remote );
		} else if (rrecv == net_fatal) {
			GPTP_LOG_ERROR("read from network interface failed");
			port->processEvent(FAULT_DETECTED);
			break;
		}
	}

	return osthread_ok;
}

void WirelessPort::processMessage
( char *buf, int length, LinkLayerAddress *remote )
{
	GPTP_LOG_VERBOSE("Processing network buffer");

	PTPMessageCommon *msg =
		buildPTPMessageCommon( buf, (int)length, remote, this );

	if (msg == NULL)
	{
		GPTP_LOG_ERROR("Discarding invalid message");
		return;
	}
	GPTP_LOG_VERBOSE("Processing message");
}

bool WirelessPort::_processEvent( Event e )
{
	bool ret;

	switch ( e )
	{
        default:
                GPTP_LOG_ERROR
			( "Unhandled event type in %s, %d",
			  __PRETTY_FUNCTION__, e );
                ret = false;
                break;

	case POWERUP:
	case INITIALIZE:
		port_ready->wait_prelock();

		if( !linkOpen(openPort, (void *)this) )
		{
			GPTP_LOG_ERROR("Error creating port thread");
			ret = false;
			break;
		}

		port_ready->wait();

		ret = true;
		break;

	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		sendSync();
	}

	return ret;
}

uint8_t seqid_to_dialogtoken( uint16_t seqid )
{
	return ( seqid % (( 1 << 8 ) - 1 )) + 1; // Skip invalid dialog token
}

bool WirelessPort::sendSync()
{
	uint16_t next_seqid;
	uint8_t next_dialog_token;
	std::array<uint8_t, 128> buf;
	unsigned length = 0;
	PTPMessageFollowUp fwup_msg;
	PortIdentity port_id;

	wl_timestamper =
		dynamic_cast<decltype(wl_timestamper)>(_hw_timestamper);

	if( wl_timestamper == NULL )
	{
		GPTP_LOG_ERROR( "Internal error: wrong timestamp module for "
				"wireless port" );
		return false;
	}

	next_seqid = getNextSyncSequenceId();
	next_dialog_token = seqid_to_dialogtoken( next_seqid );

	// If previous dialog token is invalid there isn't anything to do
	// the current offset calculation (n) depends on the previous (n-1)
	if( prev_dialog.dialog_token == INVALID_DIALOG_TOKEN )
		goto send_tm;

	getPortIdentity( port_id );
	fwup_msg.setPortIdentity( &port_id );
	fwup_msg.setClockSourceTime( getFUPInfo() );
	fwup_msg.setSequenceId( next_seqid );
	fwup_msg.setPreciseOriginTimestamp( prev_dialog.action );

	fwup_msg.writeTxBuffer( buf.data(), this );

send_tm:
	return net_succeed == wl_timestamper->requestTimingMeasurement
		( peer, next_dialog_token, &prev_dialog, buf.data(), length );
}

