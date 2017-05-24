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

#include <wireless_timestamper.hpp>

net_result WirelessTimestamper::requestTimingMeasurement
(LinkLayerAddress dest, uint16_t seq, WirelessDialog prev_dialog,
uint8_t *follow_up, int followup_length) {
        uint8_t dialog_token = (seq % 255) + 1;
        WirelessDialog next_dialog;
        next_dialog.dialog_token = dialog_token;
        next_dialog.fwup_seq = seq;
        next_dialog.action_devclk = 0;
        net_result retval = net_succeed;

        lock();
        PeerMapIter iter = peer_map.find(dest);
        if (iter == peer_map.end()) {
                XPTPD_ERROR("Got timing measurement request for unknown MAC addr
ess, %s", dest.toString());
                retval = net_trfail;
                goto bail;
        }
        iter->second->wl_port->lock();
        iter->second->wl_port->setPrevDialog(&next_dialog);
        iter->second->wl_port->unlock();
bail:
        unlock();
        if(retval == net_succeed)
                retval = _requestTimingMeasurement(dest, dialog_token, &prev_dialog, follow_up, followup_length);
        return retval;
}
