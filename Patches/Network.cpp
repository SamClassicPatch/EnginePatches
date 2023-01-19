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

#include "StdH.h"

#include "Network.h"

#include <CoreLib/Query/QueryManager.h>
#include <CoreLib/Networking/NetworkFunctions.h>

#include <CoreLib/Definitions/ActionBufferDefs.inl>
#include <CoreLib/Definitions/PlayerCharacterDefs.inl>
#include <CoreLib/Definitions/PlayerTargetDefs.inl>

// Original function pointers
extern void (CCommunicationInterface::*pServerInit)(void) = NULL;
extern void (CCommunicationInterface::*pServerClose)(void) = NULL;

void CComIntPatch::P_EndWinsock(void) {
  // Stop master server enumeration
  if (ms_bDebugOutput) {
    CPutString("CCommunicationInterface::EndWinsock() -> IMasterServer::EnumCancel()\n");
  }
  IMasterServer::EnumCancel();

  // Original function code
  #if SE1_VER >= SE1_107
    if (!cci_bWinSockOpen) return;
  #endif

  int iResult = WSACleanup();
  ASSERT(iResult == 0);

  #if SE1_VER >= SE1_107
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
  if (GetComm().IsNetworkEnabled()) {
    IMasterServer::OnServerStart();
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

  if (symptr.GetIndex()) {
    IMasterServer::OnServerEnd();
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

void CSessionStatePatch::P_FlushProcessedPredictions(void) {
  // Proceed to the original function
  (this->*pFlushPredictions)();

  // Update server for the master server
  static CSymbolPtr symptr("ser_bEnumeration");

  if (GetComm().IsNetworkEnabled() && symptr.GetIndex()) {
    if (ms_bDebugOutput) {
      //CPutString("CSessionState::FlushProcessedPredictions() -> IMasterServer::OnServerUpdate()\n");
    }
    IMasterServer::OnServerUpdate();
  }
};

// Client processes received packet from the server
void CSessionStatePatch::P_ProcessGameStreamBlock(CNetworkMessage &nmMessage) {
  // Copy the tick to process into tick used for all tasks
  _pTimer->SetCurrentTick(ses_tmLastProcessedTick);

  // Call API every simulation tick
  GetAPI()->OnTick();

  // Quit if don't need to process standard packets
  if (!INetwork::ClientHandle(this, nmMessage)) {
    return;
  }

  switch (nmMessage.GetType())
  {
    // Adding a new player
    case MSG_SEQ_ADDPLAYER: {
      // Non-action sequence
      _pNetwork->AddNetGraphValue(NGET_NONACTION, 1.0f);

      // Read player index and the character
      INDEX iNewPlayer;
      CPlayerCharacter pcCharacter;
      nmMessage >> iNewPlayer >> pcCharacter;

      // Delete all predictors
      _pNetwork->ga_World.DeletePredictors();

      // Activate the player
      ses_apltPlayers[iNewPlayer].Activate();

      // Find entity with this character
      CPlayerEntity *penNewPlayer = _pNetwork->ga_World.FindEntityWithCharacter(pcCharacter);

      // If none found
      if (penNewPlayer == NULL) {
        // Create a new player entity
        const CPlacement3D pl(FLOAT3D(0.0f, 0.0f, 0.0f), ANGLE3D(0.0f, 0.0f, 0.0f));

        try {
          static const CTString strPlayerClass = "Classes\\Player.ecl";
          penNewPlayer = (CPlayerEntity *)_pNetwork->ga_World.CreateEntity_t(pl, strPlayerClass);

          // Attach entity to client data
          ses_apltPlayers[iNewPlayer].AttachEntity(penNewPlayer);

          // Attach character to it
          penNewPlayer->en_pcCharacter = pcCharacter;

          // Prepare the entity
          penNewPlayer->Initialize();

        } catch (char *strError) {
          FatalError(LOCALIZE("Cannot load Player class:\n%s"), strError);
        }

        if (!_pNetwork->IsPlayerLocal(penNewPlayer)) {
          CPrintF(LOCALIZE("%s joined\n"), penNewPlayer->GetPlayerName());
        }

      // If found some entity
      } else {
        // Attach entity to client data
        ses_apltPlayers[iNewPlayer].AttachEntity(penNewPlayer);

        // Update its character
        penNewPlayer->CharacterChanged(pcCharacter);

        if (!_pNetwork->IsPlayerLocal(penNewPlayer)) {
          CPrintF(LOCALIZE("%s rejoined\n"), penNewPlayer->GetPlayerName());
        }
      }
    } break;

    // Removing a player
    case MSG_SEQ_REMPLAYER: {
      // Non-action sequence
      _pNetwork->AddNetGraphValue(NGET_NONACTION, 1.0f);

      // Read player index
      INDEX iPlayer;
      nmMessage >> iPlayer;

      // Delete all predictors
      _pNetwork->ga_World.DeletePredictors();

      // Inform entity of disconnnection
      CPrintF(LOCALIZE("%s left\n"), ses_apltPlayers[iPlayer].plt_penPlayerEntity->GetPlayerName());
      ses_apltPlayers[iPlayer].plt_penPlayerEntity->Disconnect();

      // Deactivate the player
      ses_apltPlayers[iPlayer].Deactivate();

      // Handle sent entity events
      ses_bAllowRandom = TRUE;
      CEntity::HandleSentEvents();
      ses_bAllowRandom = FALSE;
    } break;

    // Character change
    case MSG_SEQ_CHARACTERCHANGE: {
      // Non-action sequence
      _pNetwork->AddNetGraphValue(NGET_NONACTION, 1.0f);

      // Read player index and the character
      INDEX iPlayer;
      CPlayerCharacter pcCharacter;
      nmMessage >> iPlayer >> pcCharacter;

      // Delete all predictors
      _pNetwork->ga_World.DeletePredictors();

      // Change the character
      ses_apltPlayers[iPlayer].plt_penPlayerEntity->CharacterChanged(pcCharacter);

      // Handle sent entity events
      ses_bAllowRandom = TRUE;
      CEntity::HandleSentEvents();
      ses_bAllowRandom = FALSE;
    } break;

    // Client actions
    case MSG_SEQ_ALLACTIONS: {
      // Read packet time
      TIME tmPacket;
      nmMessage >> tmPacket;

      // Time must be greater than what has been previously received
      TIME tmTickQuantum = _pTimer->TickQuantum;
      TIME tmPacketDelta = tmPacket - ses_tmLastProcessedTick;

      // Report debug info upon mistimed actions
      if (Abs(tmPacketDelta - tmTickQuantum) >= tmTickQuantum / 10.0f) {
        CPrintF(LOCALIZE("Session state: Mistimed MSG_ALLACTIONS: Last received tick %g, this tick %g\n"),
          ses_tmLastProcessedTick, tmPacket);
      }

      // Remember received tick
      ses_tmLastProcessedTick = tmPacket;

      // Don't wait for new players anymore
      ses_bWaitAllPlayers = FALSE;

      // Delete all predictors
      _pNetwork->ga_World.DeletePredictors();

      // Process the tick
      ProcessGameTick(nmMessage, tmPacket);
    } break;

    // Pause message
    case MSG_SEQ_PAUSE: {
      // Non-action sequence
      _pNetwork->AddNetGraphValue(NGET_NONACTION, 1.0f);

      // Delete all predictors
      _pNetwork->ga_World.DeletePredictors();

      BOOL bPauseBefore = ses_bPause;

      // Read pause state and the client
      nmMessage >> (INDEX &)ses_bPause;

      CTString strPauser;
      nmMessage >> strPauser;

      // Report who paused
      if (ses_bPause != bPauseBefore && strPauser != LOCALIZE("Local machine")) {
        if (ses_bPause) {
          CPrintF(LOCALIZE("Paused by '%s'\n"), strPauser);
        } else {
          CPrintF(LOCALIZE("Unpaused by '%s'\n"), strPauser);
        }
      }

      // Must keep wanting current state
      ses_bWantPause = ses_bPause;
    } break;

    // Invalid packet
    default: ASSERT(FALSE);
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

  #if SE1_VER >= SE1_107
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
