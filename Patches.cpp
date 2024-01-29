/* Copyright (c) 2022-2024 Dreamy Cecil
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

#include <CoreLib/Base/Unzip.h>

#if CLASSICSPATCH_ENGINEPATCHES

// Singleton for patching
CPatches _EnginePatches;

// Constructor
CPatches::CPatches() {
  _bAdjustForAspectRatio = TRUE;
  _bUseVerticalFOV = 2; // Only player FOV
  _fCustomFOV = -1.0f;
  _fThirdPersonFOV = -1.0f;
  _bCheckFOV = FALSE;

  _bNoListening = FALSE;

  _ulMaxWriteMemory = (1 << 20) * 128; // 128 MB

  _eWorldFormat = E_LF_CURRENT;
  _bReinitWorld = FALSE;
};

#include "Patches/Entities.h"

void CPatches::Entities(void) {
#if CLASSICSPATCH_EXTEND_ENTITIES

  // CEntity
  void (CEntity::*pReadProps)(CTStream &) = &CEntity::ReadProperties_t;
  NewPatch(pReadProps, &CEntityPatch::P_ReadProperties, "CEntity::ReadProperties_t(...)");

  extern void (CEntity::*pSendEvent)(const CEntityEvent &);
  pSendEvent = &CEntity::SendEvent;
  NewPatch(pSendEvent, &CEntityPatch::P_SendEvent, "CEntity::SendEvent(...)");

  // CRationalEntity
  void (CRationalEntity::*pCall)(SLONG, SLONG, BOOL, const CEntityEvent &) = &CRationalEntity::Call;
  NewPatch(pCall, &CRationalEntityPatch::P_Call, "CRationalEntity::Call(...)");

  // CPlayer
  extern CEntityPatch::CReceiveItem pReceiveItem;
  StructPtr pReceiveItemPtr(GetPatchAPI()->GetEntitiesSymbol("?ReceiveItem@CPlayer@@UAEHABVCEntityEvent@@@Z"));

  if (pReceiveItemPtr.iAddress != NULL) {
    pReceiveItem = pReceiveItemPtr(CEntityPatch::CReceiveItem());
    NewPatch(pReceiveItem, &CEntityPatch::P_ReceiveItem, "CPlayer::ReceiveItem(...)");
  }

  // Mod's brush entities
  CWorld woTemp;

  try {
    static const CPlacement3D plDummy(FLOAT3D(0, 0, 0), ANGLE3D(0, 0, 0));
    CEntity *penWB = woTemp.CreateEntity_t(plDummy, CTFILENAME("Classes\\WorldBase.ecl"));
    CEntity *penMB = woTemp.CreateEntity_t(plDummy, CTFILENAME("Classes\\MovingBrush.ecl"));

    // Not sure if required but I don't wanna risk it
    penWB->Initialize();
    penMB->Initialize();

    #define GETFORCE_OFFSET CHOOSE_FOR_GAME(27, 27, 31)
    StructPtr pEntityGetForcePtr = GetPatchAPI()->GetEngineSymbol("?GetForce@CEntity@@UAEXJABV?$Vector@M$02@@AAVCForceStrength@@1@Z");
    CEntityPatch::CGetForce pEntityGetForce = pEntityGetForcePtr(CEntityPatch::CGetForce());

    // Pointer to the virtual table of CWorldBase
    size_t *pVFTable = *(size_t **)penWB;

    extern CEntityPatch::CGetForce pWorldBase_GetForce;
    pWorldBase_GetForce = *(CEntityPatch::CGetForce *)(pVFTable + GETFORCE_OFFSET);

    // Don't patch engine function by mistake
    if (pWorldBase_GetForce != pEntityGetForce) {
      NewPatch(pWorldBase_GetForce, &CEntityPatch::P_WorldBase_GetForce, "CWorldBase::GetForce(...)");
    }

    // Pointer to the virtual table of CMovingBrush
    pVFTable = *(size_t **)penMB;

    extern CEntityPatch::CGetForce pMovingBrush_GetForce;
    pMovingBrush_GetForce = *(CEntityPatch::CGetForce *)(pVFTable + GETFORCE_OFFSET);

    // Don't patch engine function by mistake
    if (pMovingBrush_GetForce != pEntityGetForce) {
      NewPatch(pMovingBrush_GetForce, &CEntityPatch::P_MovingBrush_GetForce, "CMovingBrush::GetForce(...)");
    }

    // Same as for initialization
    penWB->Destroy();
    penMB->Destroy();

  // Ignore errors
  } catch (char *strError) {
    (void)strError;
  }

#endif // CLASSICSPATCH_EXTEND_ENTITIES
};

#include "Patches/LogicTimers.h"

void CPatches::LogicTimers(void) {
#if CLASSICSPATCH_FIX_LOGICTIMERS

  void (CRationalEntity::*pSetTimerAfter)(TIME) = &CRationalEntity::SetTimerAfter;
  NewPatch(pSetTimerAfter, &CRationalEntityTimerPatch::P_SetTimerAfter, "CRationalEntity::SetTimerAfter(...)");

#endif // CLASSICSPATCH_FIX_LOGICTIMERS
};

#include "Patches/Network.h"

void CPatches::Network(void) {
#if CLASSICSPATCH_EXTEND_NETWORK

  // CCommunicationInterface
#if CLASSICSPATCH_NEW_QUERY
  void (CCommunicationInterface::*pEndWindock)(void) = &CCommunicationInterface::EndWinsock;
  NewPatch(pEndWindock, &CComIntPatch::P_EndWinsock, "CCommunicationInterface::EndWinsock()");
#endif

  extern void (CCommunicationInterface::*pServerInit)(void);
  pServerInit = &CCommunicationInterface::Server_Init_t;
  NewPatch(pServerInit, &CComIntPatch::P_ServerInit, "CCommunicationInterface::Server_Init_t()");

  extern void (CCommunicationInterface::*pServerClose)(void);
  pServerClose = &CCommunicationInterface::Server_Close;
  NewPatch(pServerClose, &CComIntPatch::P_ServerClose, "CCommunicationInterface::Server_Close()");

  // CMessageDispatcher
  void (CMessageDispatcher::*pSendToServer)(const CNetworkMessage &) = &CMessageDispatcher::SendToServerReliable;
  NewPatch(pSendToServer, &CMessageDisPatch::P_SendToServerReliable, "CMessageDispatcher::SendToServerReliable(...)");

  BOOL (CMessageDispatcher::*pRecFromClient)(INDEX, CNetworkMessage &) = &CMessageDispatcher::ReceiveFromClient;
  NewPatch(pRecFromClient, &CMessageDisPatch::P_ReceiveFromClient, "CMessageDispatcher::ReceiveFromClient(...)");

  pRecFromClient = &CMessageDispatcher::ReceiveFromClientReliable;
  NewPatch(pRecFromClient, &CMessageDisPatch::P_ReceiveFromClientReliable, "CMessageDispatcher::ReceiveFromClientReliable(...)");

  // CNetworkLibrary
  extern void (CNetworkLibrary::*pChangeLevel)(void);
  pChangeLevel = &CNetworkLibrary::ChangeLevel_internal;
  NewPatch(pChangeLevel, &CNetworkPatch::P_ChangeLevelInternal, "CNetworkLibrary::ChangeLevel_internal()");

  void (CNetworkLibrary::*pSaveGame)(const CTFileName &) = &CNetworkLibrary::Save_t;
  NewPatch(pSaveGame, &CNetworkPatch::P_Save, "CNetworkLibrary::Save_t(...)");

  extern void (CNetworkLibrary::*pLoadGame)(const CTFileName &);
  pLoadGame = &CNetworkLibrary::Load_t;
  NewPatch(pLoadGame, &CNetworkPatch::P_Load, "CNetworkLibrary::Load_t(...)");

  extern void (CNetworkLibrary::*pStopGame)(void);
  pStopGame = &CNetworkLibrary::StopGame;
  NewPatch(pStopGame, &CNetworkPatch::P_StopGame, "CNetworkLibrary::StopGame()");

  extern CNetworkPatch::CStartP2PFunc pStartPeerToPeer;
  pStartPeerToPeer = &CNetworkLibrary::StartPeerToPeer_t;
  NewPatch(pStartPeerToPeer, &CNetworkPatch::P_StartPeerToPeer, "CNetworkLibrary::StartPeerToPeer_t(...)");

  extern void (CNetworkLibrary::*pStartDemoPlay)(const CTFileName &);
  pStartDemoPlay = &CNetworkLibrary::StartDemoPlay_t;
  NewPatch(pStartDemoPlay, &CNetworkPatch::P_StartDemoPlay, "CNetworkLibrary::StartDemoPlay_t(...)");

  void (CNetworkLibrary::*pStartDemoRec)(const CTFileName &) = &CNetworkLibrary::StartDemoRec_t;
  NewPatch(pStartDemoRec, &CNetworkPatch::P_StartDemoRec, "CNetworkLibrary::StartDemoRec_t(...)");

  void (CNetworkLibrary::*pStopDemoRec)(void) = &CNetworkLibrary::StopDemoRec;
  NewPatch(pStopDemoRec, &CNetworkPatch::P_StopDemoRec, "CNetworkLibrary::StopDemoRec()");

  // CSessionState
  extern void (CSessionState::*pFlushPredictions)(void);
  pFlushPredictions = &CSessionState::FlushProcessedPredictions;
  NewPatch(pFlushPredictions, &CSessionStatePatch::P_FlushProcessedPredictions, "CSessionState::FlushProcessedPredictions()");

  extern void (CSessionState::*pStartAtClient)(INDEX);
  pStartAtClient = &CSessionState::Start_AtClient_t;
  NewPatch(pStartAtClient, &CSessionStatePatch::P_Start_AtClient, "CSessionState::Start_AtClient_t(...)");

  void (CSessionState::*pWaitStream)(CTMemoryStream &, const CTString &, INDEX) = &CSessionState::WaitStream_t;
  NewPatch(pWaitStream, &CSessionStatePatch::P_WaitStream, "CSessionState::WaitStream_t(...)");

  void (CSessionState::*pProcGameStreamBlock)(CNetworkMessage &) = &CSessionState::ProcessGameStreamBlock;
  NewPatch(pProcGameStreamBlock, &CSessionStatePatch::P_ProcessGameStreamBlock, "CSessionState::ProcessGameStreamBlock(...)");

  void (CSessionState::*pStopSession)(void) = &CSessionState::Stop;
  NewPatch(pStopSession, &CSessionStatePatch::P_Stop, "CSessionState::Stop()");

  extern void (CSessionState::*pReadSessionState)(CTStream *);
  pReadSessionState = &CSessionState::Read_t;
  NewPatch(pReadSessionState, &CSessionStatePatch::P_Read, "CSessionState::Read_t(...)");

  extern void (CSessionState::*pWriteSessionState)(CTStream *);
  pWriteSessionState = &CSessionState::Write_t;
  NewPatch(pWriteSessionState, &CSessionStatePatch::P_Write, "CSessionState::Write_t(...)");

#if CLASSICSPATCH_GUID_MASKING

  void (CSessionState::*pMakeSyncCheck)(void) = &CSessionState::MakeSynchronisationCheck;
  NewPatch(pMakeSyncCheck, &CSessionStatePatch::P_MakeSynchronisationCheck, "CSessionState::MakeSynchronisationCheck()");

  // CPlayerEntity

  // Pointer to CPlayerEntity::Write_t()
  void *pPlayerWrite = GetPatchAPI()->GetEngineSymbol("?Write_t@CPlayerEntity@@UAEXPAVCTStream@@@Z");
  NewPatch(pPlayerWrite, &CPlayerEntityPatch::P_Write, "CPlayerEntity::Write_t(...)");

  // Pointer to CPlayerEntity::ChecksumForSync()
  void *pChecksumForSync = GetPatchAPI()->GetEngineSymbol("?ChecksumForSync@CPlayerEntity@@UAEXAAKJ@Z");
  NewPatch(pChecksumForSync, &CPlayerEntityPatch::P_ChecksumForSync, "CPlayerEntity::ChecksumForSync(...)");

  // Custom symbols
  _pShell->DeclareSymbol("persistent user INDEX ser_bMaskGUIDs pre:UpdateServerSymbolValue;", &IProcessPacket::_bMaskGUIDs);

#endif // CLASSICSPATCH_GUID_MASKING

#endif // CLASSICSPATCH_EXTEND_NETWORK
};

#include "Patches/Rendering.h"

void CPatches::Rendering(void) {
#if CLASSICSPATCH_FIX_RENDERING

  extern void (*pRenderView)(CWorld &, CEntity &, CAnyProjection3D &, CDrawPort &);
  pRenderView = &RenderView;
  NewPatch(pRenderView, &P_RenderView, "::RenderView(...)");

  // Pointer to CPerspectiveProjection3D::Prepare()
  void *pPrepare = GetPatchAPI()->GetEngineSymbol("?Prepare@CPerspectiveProjection3D@@UAEXXZ");
  NewPatch(pPrepare, &CProjectionPatch::P_Prepare, "CPerspectiveProjection3D::Prepare()");

  // Custom symbols
  _pShell->DeclareSymbol("persistent user INDEX sam_bAdjustForAspectRatio;", &_EnginePatches._bAdjustForAspectRatio);
  _pShell->DeclareSymbol("persistent user INDEX sam_bUseVerticalFOV;", &_EnginePatches._bUseVerticalFOV);
  _pShell->DeclareSymbol("persistent user FLOAT sam_fCustomFOV;",      &_EnginePatches._fCustomFOV);
  _pShell->DeclareSymbol("persistent user FLOAT sam_fThirdPersonFOV;", &_EnginePatches._fThirdPersonFOV);
  _pShell->DeclareSymbol("           user INDEX sam_bCheckFOV;",       &_EnginePatches._bCheckFOV);

#endif // CLASSICSPATCH_FIX_RENDERING
};

#if SE1_VER >= SE1_107

#include "Patches/Ska.h"

// SKA models have been patched
static BOOL _bSkaPatched = FALSE;

void CPatches::ShadersPatches(void) {
#if CLASSICSPATCH_FIX_SKA

  // Already patched
  if (_bSkaPatched) return;

  _bSkaPatched = TRUE;

  // Create raw patches in memory
  void (*pDoFogHazeFunc)(BOOL) = &RM_DoFogAndHaze;
  NewRawPatch(pDoFogHazeFunc, &P_DoFogAndHaze, "RM_DoFogAndHaze(...)");

  void (*pDoFogPassFunc)(void) = &shaDoFogPass;
  CPatch::ForceRewrite(7); // Rewrite complex instruction
  NewRawPatch(pDoFogPassFunc, &P_shaDoFogPass, "shaDoFogPass(...)");

  void (*pSetWrappingFunc)(GfxWrap, GfxWrap) = &shaSetTextureWrapping;
  NewRawPatch(pSetWrappingFunc, &P_shaSetTextureWrapping, "shaSetTextureWrapping(...)");

#endif // CLASSICSPATCH_FIX_SKA
};

void CPatches::Ska(void) {
#if CLASSICSPATCH_FIX_SKA

  // Already patched
  if (_bSkaPatched) return;

  _bSkaPatched = TRUE;

  // Create patches in the registry
  void (*pFogHazeFunc)(BOOL) = &RM_DoFogAndHaze;
  NewPatch(pFogHazeFunc, &P_DoFogAndHaze, "RM_DoFogAndHaze(...)");

  void (*pFogPassFunc)(void) = &shaDoFogPass;
  CPatch::ForceRewrite(7); // Rewrite complex instruction
  NewPatch(pFogPassFunc, &P_shaDoFogPass, "shaDoFogPass(...)");

  void (*pSetWrappingFunc)(GfxWrap, GfxWrap) = &shaSetTextureWrapping;
  NewPatch(pSetWrappingFunc, &P_shaSetTextureWrapping, "shaSetTextureWrapping(...)");

#endif // CLASSICSPATCH_FIX_SKA
};

#endif

#include "Patches/SoundLibrary.h"

void CPatches::SoundLibrary(void) {
  void (CSoundLibrary::*pListen)(CSoundListener &) = &CSoundLibrary::Listen;
  NewPatch(pListen, &CSoundLibPatch::P_Listen, "CSoundLibrary::Listen(...)");

  void (CSoundObject::*pUpdate3DEffects)(void) = &CSoundObject::Update3DEffects;
  NewPatch(pUpdate3DEffects, &CSoundObjPatch::P_Update3DEffects, "CSoundObject::Update3DEffects()");
};

#include "Patches/Strings.h"

void CPatches::Strings(void) {
#if CLASSICSPATCH_FIX_STRINGS

  INDEX (CTString::*pVPrintF)(const char *, va_list) = &CTString::VPrintF;
  NewPatch(pVPrintF, &CStringPatch::P_VPrintF, "CTString::VPrintF(...)");

  CTString (CTString::*pUndecorated)(void) const = &CTString::Undecorated;
  NewPatch(pUndecorated, &CStringPatch::P_Undecorated, "CTString::Undecorated()");

#endif // CLASSICSPATCH_FIX_STRINGS
};

#include "Patches/Textures.h"

void CPatches::Textures(void) {
#if CLASSICSPATCH_EXTEND_TEXTURES

  void (CTextureData::*pCreateTex)(const CImageInfo *, MEX, INDEX, int) = &CTextureData::Create_t;
  NewPatch(pCreateTex, &CTexDataPatch::P_Create, "CTextureData::Create_t(...)");

  // Pointer to CTextureData::Write_t()
  void *pWriteTex = GetPatchAPI()->GetEngineSymbol("?Write_t@CTextureData@@UAEXPAVCTStream@@@Z");
  NewPatch(pWriteTex, &CTexDataPatch::P_Write, "CTextureData::Write_t(...)");

  void (*pProcessScript)(const CTFileName &) = &ProcessScript_t;
  NewPatch(pProcessScript, &P_ProcessTextureScript, "ProcessScript_t(...)");

  void (*pCreateTextureOut)(const CTFileName &, const CTFileName &, MEX, INDEX, int) = &CreateTexture_t;
  NewPatch(pCreateTextureOut, &P_CreateTextureOut, "CreateTexture_t(out)");

  void (*pCreateTexture)(const CTFileName &, MEX, INDEX, int) = &CreateTexture_t;
  NewPatch(pCreateTexture, &P_CreateTexture, "CreateTexture_t(...)");

#endif // CLASSICSPATCH_EXTEND_TEXTURES
};

#include "Patches/Worlds.h"

void CPatches::Worlds(void) {
  void (CWorld::*pWorldLoad)(const CTFileName &) = &CWorld::Load_t;
  NewPatch(pWorldLoad, &CWorldPatch::P_Load, "CWorld::Load_t(...)");

  void (CWorld::*pReadInfo)(CTStream *, BOOL) = &CWorld::ReadInfo_t;
  NewPatch(pReadInfo, &CWorldPatch::P_ReadInfo, "CWorld::ReadInfo_t(...)");

  CEntity *(CWorld::*pCreateEntity)(const CPlacement3D &, const CTFileName &) = &CWorld::CreateEntity_t;
  NewPatch(pCreateEntity, &CWorldPatch::P_CreateEntity, "CWorld::CreateEntity_t(...)");

  // Custom symbols
  _pShell->DeclareSymbol("user INDEX sam_bReinitWorld;", &_EnginePatches._bReinitWorld);
};

#include "Patches/UnpageStreams.h"

// Specific stream patching
static void PatchStreams(void) {
#if CLASSICSPATCH_FIX_STREAMPAGING

  // Streams have been patched
  static BOOL _bStreamsPatched = FALSE;

  if (_bStreamsPatched) return;
  _bStreamsPatched = TRUE;

  // CTStream
  void (CTStream::*pAllocMemory)(ULONG) = &CTStream::AllocateVirtualMemory;
  NewRawPatch(pAllocMemory, &CUnpageStreamPatch::P_AllocVirtualMemory, "CTStream::AllocateVirtualMemory(...)");

  void (CTStream::*pFreeBufferFunc)(void) = &CTStream::FreeBuffer;
  NewRawPatch(pFreeBufferFunc, &CUnpageStreamPatch::P_FreeBuffer, "CTStream::FreeBuffer()");

  // CTFileStream
  void (CTFileStream::*pCreateFunc)(const CTFileName &, CTStream::CreateMode) = &CTFileStream::Create_t;
  NewRawPatch(pCreateFunc, &CFileStreamPatch::P_Create, "CTFileStream::Create_t(...)");

  void (CTFileStream::*pOpenFunc)(const CTFileName &, CTStream::OpenMode) = &CTFileStream::Open_t;
  NewRawPatch(pOpenFunc, &CFileStreamPatch::P_Open, "CTFileStream::Open_t(...)");

  void (CTFileStream::*pCloseFunc)(void) = &CTFileStream::Close;
  NewRawPatch(pCloseFunc, &CFileStreamPatch::P_Close, "CTFileStream::Close()");

#endif // CLASSICSPATCH_FIX_STREAMPAGING
};

void CPatches::UnpageStreams(void) {
#if CLASSICSPATCH_FIX_STREAMPAGING

  // Streams have been unpaged
  static BOOL _bStreamsUnpaged = FALSE;

  if (_bStreamsUnpaged) return;
  _bStreamsUnpaged = TRUE;

  PatchStreams();

  // Dummy methods
  void (CTFileStream::*pPageFunc)(INDEX) = &CTStream::CommitPage;
  NewRawPatch(pPageFunc, &IDummy::PageFunc, "CTStream::CommitPage(...)");

  pPageFunc = &CTStream::DecommitPage;
  NewRawPatch(pPageFunc, &IDummy::PageFunc, "CTStream::DecommitPage(...)");

  pPageFunc = &CTStream::ProtectPageFromWritting;
  NewRawPatch(pPageFunc, &IDummy::PageFunc, "CTStream::ProtectPageFromWritting(...)");

  pPageFunc = &CTFileStream::WritePageToFile;
  NewRawPatch(pPageFunc, &IDummy::PageFunc, "CTFileStream::WritePageToFile(...)");

  pPageFunc = &CTFileStream::FileCommitPage;
  NewRawPatch(pPageFunc, &IDummy::PageFunc, "CTFileStream::FileCommitPage(...)");

  pPageFunc = &CTFileStream::FileDecommitPage;
  NewRawPatch(pPageFunc, &IDummy::PageFunc, "CTFileStream::FileDecommitPage(...)");

  void (CNetworkLibrary::*pFinishCRCFunc)(void) = &CNetworkLibrary::FinishCRCGather;
  NewRawPatch(pFinishCRCFunc, &IDummy::Void, "CNetworkLibrary::FinishCRCGather()");

  // Level remembering methods
  void (CSessionState::*pRemCurLevel)(const CTString &) = &CSessionState::RememberCurrentLevel;
  NewRawPatch(pRemCurLevel, &CRemLevelPatch::P_RememberCurrentLevel, "CSessionState::RememberCurrentLevel(...)");

  CRememberedLevel *(CSessionState::*pFindRemLevel)(const CTString &) = &CSessionState::FindRememberedLevel;
  NewRawPatch(pFindRemLevel, &CRemLevelPatch::P_FindRememberedLevel, "CSessionState::FindRememberedLevel(...)");

  void (CSessionState::*pRestoreOldLevel)(const CTString &) = &CSessionState::RestoreOldLevel;
  NewRawPatch(pRestoreOldLevel, &CRemLevelPatch::P_RestoreOldLevel, "CSessionState::RestoreOldLevel(...)");

  void (CSessionState::*pForgetOldLevels)(void) = &CSessionState::ForgetOldLevels;
  NewRawPatch(pForgetOldLevels, &CRemLevelPatch::P_ForgetOldLevels, "CSessionState::ForgetOldLevels()");

#endif // CLASSICSPATCH_FIX_STREAMPAGING
};

#include "Patches/FileSystem.h"

void CPatches::FileSystem(void) {
#if CLASSICSPATCH_EXTEND_FILESYSTEM

  // File system has been extended
  static BOOL _bFileSystemExtended = FALSE;

  if (_bFileSystemExtended) return;
  _bFileSystemExtended = TRUE;

  // Don't patch file system
  if (!CCoreAPI::Props().bExtendedFileSystem) return;

  PatchStreams();

  // CEntityClass
  void (CEntityClass::*pObtainComponents)(void) = &CEntityClass::ObtainComponents_t;
  NewRawPatch(pObtainComponents, &CEntityClassPatch::P_ObtainComponents, "CEntityClass::ObtainComponents_t()");

  void *pReadClass = CPatchAPI::GetEngineSymbolPortable("?Read_t@CEntityClass@@UAEXPAVCTStream@@@Z");
  NewRawPatch(pReadClass, &CEntityClassPatch::P_Read, "CEntityClass::Read_t(...)");

#if SE1_VER >= SE1_107
  // CShader
  void *pReadShader = CPatchAPI::GetEngineSymbolPortable("?Read_t@CShader@@UAEXPAVCTStream@@@Z");
  NewRawPatch(pReadShader, &CShaderPatch::P_Read, "CShader::Read_t(...)");
#endif

  // CTStream
  #if SE1_GAME != SS_REV
    void *pGetLine = CPatchAPI::GetEngineSymbolPortable("?GetLine_t@CTStream@@QAEXPADJD@Z");
  #else
    void *pGetLine = CPatchAPI::GetEngineSymbolPortable("?GetLine_t@CTStream@@UAEXPADJD@Z");
  #endif
  NewRawPatch(pGetLine, &CStreamPatch::P_GetLine, "CTStream::GetLine_t(...)");

  void (CTStream::*pReadDictionary)(SLONG) = &CTStream::ReadDictionary_intenal_t;
  NewRawPatch(pReadDictionary, &CStreamPatch::P_ReadDictionary_intenal, "CTStream::ReadDictionary_intenal_t(...)");

  // Global methods
  extern void (*pInitStreams)(void);
  pInitStreams = StructPtr(ADDR_INITSTREAMS)(&P_InitStreams);
  NewRawPatch(pInitStreams, &P_InitStreams, "::InitStreams()");

  void (*pMakeDirList)(CFileList &, const CTFileName &, const CTString &, ULONG) = &MakeDirList;
  NewRawPatch(pMakeDirList, &P_MakeDirList, "::MakeDirList(...)");

#if SE1_GAME != SS_REV
  INDEX (*pExpandFilePath)(EXPAND_PATH_ARGS(ULONG, const CTFileName &, CTFileName &, BOOL)) = &ExpandFilePath;
  NewRawPatch(pExpandFilePath, &P_ExpandFilePath, "::ExpandFilePath(...)");
#endif

#endif // CLASSICSPATCH_EXTEND_FILESYSTEM
};

#endif // CLASSICSPATCH_ENGINEPATCHES
