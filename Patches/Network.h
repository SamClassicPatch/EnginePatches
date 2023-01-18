/* Copyright (c) 2022-2023 Dreamy Cecil
This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as published by
the Free Software Foundation


This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#ifndef CECIL_INCL_PATCHES_NETWORK_H
#define CECIL_INCL_PATCHES_NETWORK_H

#ifdef PRAGMA_ONCE
  #pragma once
#endif

#include <CoreLib/Networking/CommInterface.h>

class CComIntPatch : public CCommunicationInterface {
  public:
    void P_EndWinsock(void);

    void P_ServerInit(void);
    void P_ServerClose(void);
};

class CMessageDisPatch : public CMessageDispatcher {
  public:
    // Packet receiving method type
    typedef BOOL (CCommunicationInterface::*CReceiveFunc)(INDEX, void *, SLONG &);

  public:
    // Server receives a speciifc packet
    BOOL ReceiveFromClientSpecific(INDEX iClient, CNetworkMessage &nmMessage, CReceiveFunc pFunc);

    // Server receives a packet
    BOOL P_ReceiveFromClient(INDEX iClient, CNetworkMessage &nmMessage);

    // Server receives a reliable packet
    BOOL P_ReceiveFromClientReliable(INDEX iClient, CNetworkMessage &nmMessage);
};

class CSessionStatePatch : public CSessionState {
  public:
    void P_FlushProcessedPredictions(void);

    // Client processes received packet from the server
    void P_ProcessGameStreamBlock(CNetworkMessage &nmMessage);

    void P_Stop(void);
};

#endif
