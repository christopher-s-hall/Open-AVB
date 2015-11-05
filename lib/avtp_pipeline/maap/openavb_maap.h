/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Attributions: The inih library portion of the source code is licensed from
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt.
Complete license and copyright information can be found at
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/
#ifndef OPENAVB_MAAP_H
#define OPENAVB_MAAP_H 1
#include "openavb_types.h"

typedef void (openavbMaapRestartCb_t)(void *handle, struct ether_addr *addr);

typedef enum {
	MAAP_ALLOC_GENERATE,	// normal, random allocation
	MAAP_ALLOC_PROBE,		// specify address to be allocated
	MAAP_ALLOC_DEFEND		// specify address, and skip to DEFEND state
} openavbMaapAllocAction_t;

// MAAP library lifecycle
bool openavbMaapInitialize(const char *ifname, openavbMaapRestartCb_t* cbfn);
void openavbMaapFinalize();

// Normal address allocation
void* openavbMaapAllocate(int count,
					  /* out */ struct ether_addr *addr);
// Extended address allocation (testing)
void* openavbMaapExtAlloc(int count,
					  openavbMaapAllocAction_t action,
					  /* in/out */ struct ether_addr *addr);
// Release an allocated address range
void openavbMaapRelease(void* handle);

// Dump state info for MAAP
void openavbMaapListAllocs();
// Dump heard announcements
void openavbMaapListHeard();

#endif // OPENAVB_MAAP_H
