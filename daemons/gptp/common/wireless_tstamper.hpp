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

#ifndef WIRELESS_TIMESTAMPER_HPP
#define WIRELESS_TIMESTAMPER_HPP

#include <common_timestamper.hpp>


/* id identifies the timestamper 0 is reserved meaning no timestamper is
   availabled */

typedef struct _PTP_CTX
{
        uint8_t         ElementId;
        uint8_t         Length;
        uint8_t         Data[1];
} PTP_CTX;

typedef struct _TIMINGMSMT_REQUEST
{
        uint8_t         PeerMACAddress[ETHER_ADDR_OCTETS];
        uint8_t         Category;
        uint8_t         Action;
        uint8_t         DialogToken;
        uint8_t         FollowUpDialogToken;
        uint32_t        T1;
        uint32_t        T4;
        uint8_t         MaxT1Error;
        uint8_t         MaxT4Error;
        PTP_CTX         VendorSpecifics;
} TIMINGMSMT_REQUEST;

typedef struct _TIMINGMSMT_EVENT_DATA
{
        uint8_t         PeerMACAddress[ETHER_ADDR_OCTETS];
        uint32_t        DialogToken;
        uint32_t        FollowUpDialogToken;
        uint64_t        T1;
        uint32_t        MaxT1Error;
        uint64_t        T4;
        uint32_t        MaxT4Error;
        uint64_t        T2;
        uint32_t        MaxT2Error;
        uint64_t        T3;
        uint32_t        MaxT3Error;
        PTP_CTX         VendorSpecifics;
} TIMINGMSMT_EVENT_DATA;

typedef struct _TIMINGMSMT_CONFIRM_EVENT_DATA
{
        uint8_t         PeerMACAddress[ETHER_ADDR_OCTETS];
        uint32_t        DialogToken;
        uint64_t        T1;
        uint32_t        MaxT1Error;
        uint64_t        T4;
        uint32_t        MaxT4Error;
} TIMINGMSMT_CONFIRM_EVENT_DATA;

typedef struct _WIRELESS_CORRELATEDTIME
{
        uint64_t        TSC;
        uint64_t        LocalClk;
} WIRELESS_CORRELATEDTIME;

typedef enum _WIRELESS_EVENT_TYPE
{
        TIMINGMSMT_EVENT = 0,
        TIMINGMSMT_CONFIRM_EVENT,
        TIMINGMSMT_CORRELATEDTIME_EVENT,
} WIRELESS_EVENT_TYPE;

class WirelessDialog
{
public:
        Timestamp action;
        uint64_t action_devclk;
        Timestamp ack;
        uint64_t ack_devclk;
        uint8_t dialog_token;
        uint16_t fwup_seq;

        WirelessDialog( uint32_t action, uint32_t ack, uint8_t dialog_token )
	{
                this->action_devclk = action; this->ack_devclk = ack;
                this->dialog_token = dialog_token;
        }

        WirelessDialog() { dialog_token = 0; }

        WirelessDialog& operator=( const WirelessDialog& a )
	{
                if (this != &a) {
                        this->ack = a.ack;
                        this->ack_devclk = a.ack_devclk;
                        this->action = a.action;
                        this->action_devclk = a.action_devclk;
                        this->dialog_token = a.dialog_token;
                }

                return *this;
        }

};

#define OUI_LENGTH (3)

class WirelessTimestamper : public CommonTimestamper
{
public:
	/**
	 * @brief Generic method for sending timing measurement (TM) frame
	 *	(802.11-2012)
	 */
        net_result requestTimingMeasurement
                ( LinkLayerAddress dest, uint8_t dialog_token,
		  WirelessDialog prev_dialog, uint8_t *follow_up,
		  int followup_length );

	/**
	 * @brief OS/driver specific method for sending timing measurement (TM)
	 *	frame (802.11-2012) called by requestTimingMeasurement
	 */
        virtual net_result _requestTimingMeasurement
                ( LinkLayerAddress dest, uint8_t dialog_token,
		  WirelessDialog *prev_dialog, uint8_t *follow_up,
		  int followup_length ) = 0;

};

#endif/*WIRELESS_TIMESTAMPER_HPP*/
