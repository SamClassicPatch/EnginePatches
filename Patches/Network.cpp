/* Copyright (c) 2022 Dreamy Cecil
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

#include "StdH.h"

#include "Network.h"

#include <CoreLib/Networking/NetworkFunctions.h>

// Original function pointers
extern void (CCommunicationInterface::*pServerInit)(void) = NULL;
extern void (CCommunicationInterface::*pServerClose)(void) = NULL;

void CComIntPatch::P_EndWinsock(void) {
  // Stop master server enumeration
  if (ms_bDebugOutput) {
    CPutString("CCommunicationInterface::EndWinsock() -> MS_EnumCancel()\n");
  }
  MS_EnumCancel();

  // Original function code
  #if SE1_VER != 105
    if (!cci_bWinSockOpen) return;
  #endif

  int iResult = WSACleanup();
  ASSERT(iResult == 0);

  #if SE1_VER != 105
    cci_bWinSockOpen = FALSE;
  #endif
};

void CComIntPatch::P_ServerInit(void) {
  // Proceed to the original function
  (this->*pServerInit)();

  if (ms_bDebugOutput) {
    CPutString("CCommunicationInterface::Server_Init_t()\n");
  }

  // Start new master server
  if (GetComm().IsNetworkEnabled())
  {
    if (ms_bDebugOutput) {
      CPutString("  MS_OnServerStart()\n");
    }
    MS_OnServerStart();
  }
};

void CComIntPatch::P_ServerClose(void) {
  // Proceed to the original function
  (this->*pServerClose)();

  if (ms_bDebugOutput) {
    CPutString("CCommunicationInterface::Server_Close()\n");
  }

  // Stop new master server
  static CSymbolPtr symptr("ser_bEnumeration");

  if (symptr.GetIndex())
  {
    if (ms_bDebugOutput) {
      CPutString("  MS_OnServerEnd()\n");
    }
    MS_OnServerEnd();
  }
};

// Original function pointer
extern void (CMessageDispatcher::*pSendToClient)(INDEX, const CNetworkMessage &) = NULL;

// Server sends a reliable packet
void CMessageDisPatch::P_SendToClientReliable(INDEX iClient, const CNetworkMessage &nmMessage) {
  // Remember message type
  const MESSAGETYPE eMessage = nmMessage.GetType();

  // Proceed to the original function
  (this->*pSendToClient)(iClient, nmMessage);

  if (ms_bDebugOutput) {
    CPrintF("CMessageDispatcher::SendToClientReliable(%d)\n", eMessage);
  }

  // Notify master server that a player is connecting
  static CSymbolPtr symptr("ser_bEnumeration");

  if (eMessage == MSG_REP_CONNECTPLAYER && symptr.GetIndex())
  {
    if (ms_bDebugOutput) {
      CPutString("  MS_OnServerStateChanged()\n");
    }
    MS_OnServerStateChanged();
  }
};

// Server receives a speciifc packet
BOOL CMessageDisPatch::ReceiveFromClientSpecific(INDEX iClient, CNetworkMessage &nmMessage, CReceiveFunc pFunc) {
  // Receive message in static buffer
  nmMessage.nm_slSize = nmMessage.nm_slMaxSize;

  // Receive using a specific method
  BOOL bReceived = (GetComm().*pFunc)(iClient, (void *)nmMessage.nm_pubMessage, nmMessage.nm_slSize);

  // If there is message
  if (bReceived) {
    // Init the message structure
    nmMessage.nm_pubPointer = nmMessage.nm_pubMessage;
    nmMessage.nm_iBit = 0;

    UBYTE ubType;
    nmMessage.Read(&ubType, sizeof(ubType));
    nmMessage.nm_mtType = (MESSAGETYPE)ubType;

    // Replace default CServer packet processor or return TRUE to process through the original function
    return INetwork::ServerHandle(this, iClient, nmMessage);
  }

  return bReceived;
};

// Server receives a packet
BOOL CMessageDisPatch::P_ReceiveFromClient(INDEX iClient, CNetworkMessage &nmMessage) {
  return ReceiveFromClientSpecific(iClient, nmMessage, &CCommunicationInterface::Server_Receive_Unreliable);
};

// Server receives a reliable packet
BOOL CMessageDisPatch::P_ReceiveFromClientReliable(INDEX iClient, CNetworkMessage &nmMessage) {
  return ReceiveFromClientSpecific(iClient, nmMessage, &CCommunicationInterface::Server_Receive_Reliable);
};

// Original function pointers
extern void (CSessionState::*pFlushPredictions)(void) = NULL;
extern void (CSessionState::*pProcGameStreamBlock)(CNetworkMessage &) = NULL;

void CSessionStatePatch::P_FlushProcessedPredictions(void) {
  // Proceed to the original function
  (this->*pFlushPredictions)();

  // Update server for the master server
  static CSymbolPtr symptr("ser_bEnumeration");

  if (GetComm().IsNetworkEnabled() && symptr.GetIndex()) {
    if (ms_bDebugOutput) {
      //CPutString("CSessionState::FlushProcessedPredictions() -> MS_OnServerUpdate()\n");
    }
    MS_OnServerUpdate();
  }
};

// Client processes received packet from the server
void CSessionStatePatch::P_ProcessGameStreamBlock(CNetworkMessage &nmMessage) {
  // Copy the tick to process into tick used for all tasks
  _pTimer->SetCurrentTick(ses_tmLastProcessedTick);

  // Call API every simulation tick
  GetAPI()->OnTick();

  // If cannot handle custom packet
  if (INetwork::ClientHandle(this, nmMessage)) {
    // Call the original function for standard packets
    (this->*pProcGameStreamBlock)(nmMessage);
  }
};

void CSessionStatePatch::P_Stop(void) {
  // Original function code
  ses_bKeepingUpWithTime = TRUE;
  ses_tmLastUpdated = -100;
  ses_bAllowRandom = TRUE;
  ses_bPredicting = FALSE;
  ses_tmPredictionHeadTick = -2.0f;
  ses_tmLastSyncCheck = 0;
  ses_bPause = FALSE;
  ses_bWantPause = FALSE;
  ses_bGameFinished = FALSE;
  ses_bWaitingForServer = FALSE;
  ses_strDisconnected = "";
  ses_ctMaxPlayers = 1;
  ses_fRealTimeFactor = 1.0f;
  ses_bWaitAllPlayers = FALSE;
  ses_apeEvents.PopAll();

  _pTimer->DisableLerp();

  #if SE1_VER >= 107
    CNetworkMessage nmConfirmDisconnect(MSG_REP_DISCONNECTED);

    if (GetComm().cci_bClientInitialized) {
      _pNetwork->SendToServerReliable(nmConfirmDisconnect);
    }
  #endif

  GetComm().Client_Close();
  ForgetOldLevels();

  ses_apltPlayers.Clear();
  ses_apltPlayers.New(NET_MAXGAMEPLAYERS);
};
