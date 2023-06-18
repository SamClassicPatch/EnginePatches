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

#if CLASSICSPATCH_ENGINEPATCHES

#include "Network.h"

#include <CoreLib/Query/QueryManager.h>
#include <CoreLib/Networking/NetworkFunctions.h>

#include <CoreLib/Definitions/ActionBufferDefs.inl>
#include <CoreLib/Definitions/PlayerCharacterDefs.inl>
#include <CoreLib/Definitions/PlayerTargetDefs.inl>

#if CLASSICSPATCH_EXTEND_NETWORK

// Original function pointers
void (CCommunicationInterface::*pServerInit)(void) = NULL;
void (CCommunicationInterface::*pServerClose)(void) = NULL;

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

#if CLASSICSPATCH_GUID_MASKING
  IProcessPacket::ClearSyncChecks();
#endif

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

#if CLASSICSPATCH_GUID_MASKING
  IProcessPacket::ClearSyncChecks();
#endif

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

    // Process received packet
    return TRUE;
  }

  return bReceived;
};

// Server receives a packet
BOOL CMessageDisPatch::P_ReceiveFromClient(INDEX iClient, CNetworkMessage &nmMessage) {
  FOREVER {
    // Process unreliable message
    if (ReceiveFromClientSpecific(iClient, nmMessage, &CCommunicationInterface::Server_Receive_Unreliable)) {
      // Set client that's being handled
      IProcessPacket::_iHandlingClient = iClient;
      BOOL bPass = INetwork::ServerHandle(this, iClient, nmMessage);

      // Exit to process through engine's CServer::Handle()
      if (bPass) return TRUE;

    // No more messages
    } else {
      break;
    }
  }

  // Exit engine's loop
  return FALSE;
};

// Server receives a reliable packet
BOOL CMessageDisPatch::P_ReceiveFromClientReliable(INDEX iClient, CNetworkMessage &nmMessage) {
  FOREVER {
    // Process reliable message
    if (ReceiveFromClientSpecific(iClient, nmMessage, &CCommunicationInterface::Server_Receive_Reliable)) {
      // Set client that's being handled
      IProcessPacket::_iHandlingClient = iClient;
      BOOL bPass = INetwork::ServerHandle(this, iClient, nmMessage);

      // Exit to process through engine's CServer::Handle()
      if (bPass) return TRUE;

    // Proceed to unreliable messages
    } else {
      break;
    }
  }

  // Exit engine's loop
  return FALSE;
};

// Original function pointers
void (CSessionState::*pFlushPredictions)(void) = NULL;

void (CNetworkLibrary::*pLoadGame)(const CTFileName &) = NULL;
void (CNetworkLibrary::*pStopGame)(void) = NULL;
void (CNetworkLibrary::*pStartDemoPlay)(const CTFileName &) = NULL;

#if CLASSICSPATCH_GUID_MASKING

void (CNetworkLibrary::*pChangeLevel)(void) = NULL;

void CNetworkPatch::P_ChangeLevelInternal(void) {
  // Proceed to the original function
  (this->*pChangeLevel)();

  // Clear sync checks for each client on a new level
  if (IsServer()) {
    IProcessPacket::ClearSyncChecks();
  }
};

#endif // CLASSICSPATCH_GUID_MASKING

// Save current game
void CNetworkPatch::P_Save(const CTFileName &fnmGame) {
  // Synchronize access to network
  CTSingleLock slNetwork(&ga_csNetwork, TRUE);

  // Must be the server
  if (!IsServer()) {
    ThrowF_t(LOCALIZE("Cannot save game - not a server!\n"));
  }

#if CLASSICSPATCH_GUID_MASKING

  // Currently saving
  IProcessPacket::_iHandlingClient = IProcessPacket::CLT_SAVE;

#endif // CLASSICSPATCH_GUID_MASKING

  // Create the file
  CTFileStream strmFile;
  strmFile.Create_t(fnmGame);

  // Write game to stream
  strmFile.WriteID_t("GAME");
  ga_sesSessionState.Write_t(&strmFile);
  strmFile.WriteID_t("GEND"); // Game end

  // [Cecil] Save game for Core
  GetAPI()->OnGameSave(fnmGame);
};

// Load saved game
void CNetworkPatch::P_Load(const CTFileName &fnmGame) {
  // Proceed to the original function
  (this->*pLoadGame)(fnmGame);

  // Load game for Core
  GetAPI()->OnGameLoad(fnmGame);
};

// Stop current game
void CNetworkPatch::P_StopGame(void) {
  // Stop game for Core
  GetAPI()->OnGameStop();

  // Proceed to the original function
  (this->*pStopGame)();
};

// Start playing a demo
void CNetworkPatch::P_StartDemoPlay(const CTFileName &fnDemo) {
  // Proceed to the original function
  (this->*pStartDemoPlay)(fnDemo);

  // Play demo for Core
  GetAPI()->OnDemoPlay(fnDemo);
};

// Start recording a demo
void CNetworkPatch::P_StartDemoRec(const CTFileName &fnDemo) {
  // Synchronize access to network
  CTSingleLock slNetwork(&ga_csNetwork, TRUE);

  // Already recording
  if (ga_bDemoRec) {
    throw LOCALIZE("Already recording a demo!");
  }

  // Create the file
  ga_strmDemoRec.Create_t(fnDemo);

  // Write initial info to stream
  ga_strmDemoRec.WriteID_t("DEMO");
  ga_strmDemoRec.WriteID_t("MVER");
  ga_strmDemoRec << ULONG(_SE_BUILD_MINOR);
  ga_sesSessionState.Write_t(&ga_strmDemoRec);

  // Set demo recording state
  ga_bDemoRec = TRUE;

  // [Cecil] Start demo recording for Core
  GetAPI()->OnDemoStart(fnDemo);
};

// Stop recording a demo
void CNetworkPatch::P_StopDemoRec(void) {
  // Synchronize access to network
  CTSingleLock slNetwork(&ga_csNetwork, TRUE);

  // Not recording
  if (!ga_bDemoRec) return;

  // Write terminal info to the stream and close the file
  ga_strmDemoRec.WriteID_t("DEND"); // Demo end
  ga_strmDemoRec.Close();

  // Set demo recording state
  ga_bDemoRec = FALSE;

  // [Cecil] Stop demo recording for Core
  GetAPI()->OnDemoStop();
};

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

  // Notify server about the disconnection
  if (IsCommInitialized()) {
    CNetworkMessage nmConfirmDisconnect((MESSAGETYPE)INetwork::PCK_REP_DISCONNECTED);
    _pNetwork->SendToServerReliable(nmConfirmDisconnect);
  }

  GetComm().Client_Close();
  ForgetOldLevels();

  ses_apltPlayers.Clear();
  ses_apltPlayers.New(NET_MAXGAMEPLAYERS);
};

#if CLASSICSPATCH_GUID_MASKING

// Check if should mask player GUIDs
static inline BOOL ShouldMaskGUIDs(void) {
  return IProcessPacket::_bMaskGUIDs && _pNetwork->IsServer();
};

// Send synchronization packet to the server (as client) or add it to the buffer (as server)
void CSessionStatePatch::P_MakeSynchronisationCheck(void) {
  if (!IsCommInitialized()) return;

  // Don't check yet
  if (ses_tmLastSyncCheck + ses_tmSyncCheckFrequency > ses_tmLastProcessedTick) {
    return;
  }

  ses_tmLastSyncCheck = ses_tmLastProcessedTick;

  ULONG ulLocalCRC;
  CSyncCheck scLocal;

  // Buffer sync checks for the server
  if (ShouldMaskGUIDs()) {
    CServer &srv = _pNetwork->ga_srvServer;

    // Make local checksum for each session separately
    for (INDEX iSession = 0; iSession < srv.srv_assoSessions.Count(); iSession++) {
      CSessionSocket &sso = srv.srv_assoSessions[iSession];

      if (iSession > 0 && !sso.sso_bActive) {
        continue;
      }

      IProcessPacket::_iHandlingClient = iSession;

      CRC_Start(ulLocalCRC);
      ChecksumForSync(ulLocalCRC, ses_iExtensiveSyncCheck);
      CRC_Finish(ulLocalCRC);

      // Create sync check
      CSyncCheck sc;
      sc.sc_tmTick = ses_tmLastSyncCheck;
      sc.sc_iSequence = ses_iLastProcessedSequence; 
      sc.sc_ulCRC = ulLocalCRC;
      sc.sc_iLevel = ses_iLevel;

      // Add this sync check to this client
      IProcessPacket::AddSyncCheck(iSession, sc);

      // Save local sync check for the server client
      if (iSession == 0) {
        scLocal = sc;
      }
    }

  // Local client sync check
  } else {
    CRC_Start(ulLocalCRC);
    ChecksumForSync(ulLocalCRC, ses_iExtensiveSyncCheck);
    CRC_Finish(ulLocalCRC);

    scLocal.sc_tmTick = ses_tmLastSyncCheck;
    scLocal.sc_iSequence = ses_iLastProcessedSequence; 
    scLocal.sc_ulCRC = ulLocalCRC;
    scLocal.sc_iLevel = ses_iLevel;

    // Add local sync check to the server if not masking
    if (_pNetwork->IsServer()) {
      IProcessPacket::AddSyncCheck(0, scLocal);
    }
  }

  IProcessPacket::_iHandlingClient = IProcessPacket::CLT_NONE;

  // Send sync check to the server (including the server client)
  CNetworkMessage nmSyncCheck(MSG_SYNCCHECK);
  nmSyncCheck.Write(&scLocal, sizeof(scLocal));

  _pNetwork->SendToServer(nmSyncCheck);
};

// Get player buffer from the server associated with a player entity in the world
static CPlayerBuffer *PlayerBufferFromEntity(CPlayerEntity *pen) {
  CStaticArray<CPlayerTarget> &aPlayerTargets = _pNetwork->ga_sesSessionState.ses_apltPlayers;

  for (INDEX i = 0; i < aPlayerTargets.Count(); i++) {
    if (aPlayerTargets[i].plt_penPlayerEntity == pen) {
      return &_pNetwork->ga_srvServer.srv_aplbPlayers[i];
    }
  }

  return NULL;
};

void CPlayerEntityPatch::P_Write(CTStream *ostr) {
  CMovableModelEntity::Write_t(ostr);
  const INDEX iClient = IProcessPacket::_iHandlingClient;

  // Normal writing for clients
  if (!ShouldMaskGUIDs() || iClient == IProcessPacket::CLT_NONE) {
    *ostr << en_pcCharacter << en_plViewpoint;
    return;
  }

  // Get player buffer for this entity
  CPlayerBuffer *pplb = PlayerBufferFromEntity(this);

  // Start with invalid GUID
  UBYTE aubGUID[16];
  memset(aubGUID, 0xFFFFFFFF, sizeof(aubGUID));

  // If there's an associated player buffer
  if (pplb != NULL) {
    // Only save GUID of the server client
    if (iClient == IProcessPacket::CLT_SAVE) {
      if (pplb->plb_iClient == 0) {
        memcpy(aubGUID, pplb->plb_pcCharacter.pc_aubGUID, sizeof(aubGUID));
      }

    // Use GUID for the current client
    } else if (iClient == pplb->plb_iClient) {
      memcpy(aubGUID, pplb->plb_pcCharacter.pc_aubGUID, sizeof(aubGUID));

    // Otherwise mask it
    } else {
      IProcessPacket::MaskGUID(aubGUID, *pplb);
    }
  }

  // Serialize CPlayerCharacter
  ostr->WriteID_t("PLC4");
  *ostr << en_pcCharacter.pc_strName << en_pcCharacter.pc_strTeam;
  ostr->Write_t(aubGUID, sizeof(aubGUID));
  ostr->Write_t(en_pcCharacter.pc_aubAppearance, sizeof(en_pcCharacter.pc_aubAppearance));

  *ostr << en_plViewpoint;
};

void CPlayerEntityPatch::P_ChecksumForSync(ULONG &ulCRC, INDEX iExtensiveSyncCheck) {
  CMovableModelEntity::ChecksumForSync(ulCRC, iExtensiveSyncCheck);
  const INDEX iClient = IProcessPacket::_iHandlingClient;

  // Normal check for clients
  if (!ShouldMaskGUIDs() || iClient == IProcessPacket::CLT_NONE) {
    CRC_AddBlock(ulCRC, en_pcCharacter.pc_aubGUID, sizeof(en_pcCharacter.pc_aubGUID));
    CRC_AddBlock(ulCRC, en_pcCharacter.pc_aubAppearance, sizeof(en_pcCharacter.pc_aubAppearance));
    return;
  }

  // Get player buffer for this entity
  CPlayerBuffer *pplb = PlayerBufferFromEntity(this);

  UBYTE aubGUID[16];

  // Use GUID from the buffer for the current client
  if (iClient == pplb->plb_iClient) {
    memcpy(aubGUID, pplb->plb_pcCharacter.pc_aubGUID, sizeof(aubGUID));

  } else {
    IProcessPacket::MaskGUID(aubGUID, *pplb);
  }

  CRC_AddBlock(ulCRC, aubGUID, sizeof(aubGUID));
  CRC_AddBlock(ulCRC, en_pcCharacter.pc_aubAppearance, sizeof(en_pcCharacter.pc_aubAppearance));
};

#endif // CLASSICSPATCH_GUID_MASKING

#endif // CLASSICSPATCH_EXTEND_NETWORK

#endif // CLASSICSPATCH_ENGINEPATCHES
