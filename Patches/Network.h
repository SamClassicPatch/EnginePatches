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
#include <CoreLib/Networking/MessageProcessing.h>

#if CLASSICSPATCH_ENGINEPATCHES && CLASSICSPATCH_EXTEND_NETWORK

class CComIntPatch : public CCommunicationInterface {
  public:
  #if CLASSICSPATCH_NEW_QUERY
    void P_EndWinsock(void);
  #endif

    void P_ServerInit(void);
    void P_ServerClose(void);
};

class CMessageDisPatch : public CMessageDispatcher {
  public:
    // Packet receiving method type
    typedef BOOL (CCommunicationInterface::*CReceiveFunc)(INDEX, void *, SLONG &);

  public:
    // Send a reliable packet to the server
    void P_SendToServerReliable(const CNetworkMessage &nmMessage);

    // Server receives a speciifc packet
    BOOL ReceiveFromClientSpecific(INDEX iClient, CNetworkMessage &nmMessage, CReceiveFunc pFunc);

    // Server receives a packet
    BOOL P_ReceiveFromClient(INDEX iClient, CNetworkMessage &nmMessage);

    // Server receives a reliable packet
    BOOL P_ReceiveFromClientReliable(INDEX iClient, CNetworkMessage &nmMessage);
};

class CNetworkPatch : public CNetworkLibrary {
  public:
  #if CLASSICSPATCH_GUID_MASKING
    void P_ChangeLevelInternal(void);
  #endif

    // Save current game
    void P_Save(const CTFileName &fnmGame);

    // Load saved game
    void P_Load(const CTFileName &fnmGame);

    // Stop current game
    void P_StopGame(void);

    // Start playing a demo
    void P_StartDemoPlay(const CTFileName &fnDemo);

    // Start recording a demo
    void P_StartDemoRec(const CTFileName &fnDemo);

    // Stop recording a demo
    void P_StopDemoRec(void);
};

class CSessionStatePatch : public CSessionState {
  public:
    void P_FlushProcessedPredictions(void);

    // Client processes received packet from the server
    void P_ProcessGameStreamBlock(CNetworkMessage &nmMessage);

    void P_Start_AtClient(INDEX ctLocalPlayers);

    void P_Stop(void);

  #if CLASSICSPATCH_GUID_MASKING
    // Send synchronization packet to the server (as client) or add it to the buffer (as server)
    void P_MakeSynchronisationCheck(void);
  #endif
};

#if CLASSICSPATCH_GUID_MASKING

class CPlayerEntityPatch : public CPlayerEntity {
  public:
    void P_Write(CTStream *ostr);

    void P_ChecksumForSync(ULONG &ulCRC, INDEX iExtensiveSyncCheck);
};

#endif // CLASSICSPATCH_GUID_MASKING

#endif // CLASSICSPATCH_EXTEND_NETWORK

#endif
