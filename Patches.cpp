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

// Singleton for patching
CPatches _EnginePatches;

// Constructor
CPatches::CPatches() {
  _bAdjustForAspectRatio = TRUE;
  _bUseVerticalFOV = 1;
  _fCustomFOV = -1.0f;
  _fThirdPersonFOV = -1.0f;
  _bCheckFOV = FALSE;

  _bNoListening = FALSE;

  _ulMaxWriteMemory = (1 << 20) * 128; // 128 MB

  _bFirstEncounter = FALSE;
  _bReinitWorld = FALSE;
};

#include "Patches/Network.h"

static void UpdateMaskGUIDs(void *pSymbol) {
  BOOL &bMask = *(BOOL *)pSymbol;

  // Cannot change the state of the variable while running the game as a server
  if (_pNetwork->IsServer()) {
    CPutString(TRANS("Cannot change the state of GUID masking while the server is running!\n"));

    // Restore value
    bMask = IProcessPacket::_bMaskGUIDs;
    return;
  }

  IProcessPacket::_bMaskGUIDs = bMask;
};

void CPatches::Network(void) {
  // CCommunicationInterface
  void (CCommunicationInterface::*pEndWindock)(void) = &CCommunicationInterface::EndWinsock;
  NewPatch(pEndWindock, &CComIntPatch::P_EndWinsock, "CCommunicationInterface::EndWinsock()");

  extern void (CCommunicationInterface::*pServerInit)(void);
  pServerInit = &CCommunicationInterface::Server_Init_t;
  NewPatch(pServerInit, &CComIntPatch::P_ServerInit, "CCommunicationInterface::Server_Init_t()");

  extern void (CCommunicationInterface::*pServerClose)(void);
  pServerClose = &CCommunicationInterface::Server_Close;
  NewPatch(pServerClose, &CComIntPatch::P_ServerClose, "CCommunicationInterface::Server_Close()");

  // CMessageDispatcher
  BOOL (CMessageDispatcher::*pRecFromClient)(INDEX, CNetworkMessage &) = &CMessageDispatcher::ReceiveFromClient;
  NewPatch(pRecFromClient, &CMessageDisPatch::P_ReceiveFromClient, "CMessageDispatcher::ReceiveFromClient(...)");

  BOOL (CMessageDispatcher::*pRecFromClientReliable)(INDEX, CNetworkMessage &) = &CMessageDispatcher::ReceiveFromClientReliable;
  NewPatch(pRecFromClientReliable, &CMessageDisPatch::P_ReceiveFromClientReliable, "CMessageDispatcher::ReceiveFromClientReliable(...)");

  // CNetworkLibrary
  extern void (CNetworkLibrary::*pChangeLevel)(void);
  pChangeLevel = &CNetworkLibrary::ChangeLevel_internal;
  NewPatch(pChangeLevel, &CNetworkPatch::P_ChangeLevelInternal, "CNetworkLibrary::ChangeLevel_internal()");

  // CSessionState
  extern void (CSessionState::*pFlushPredictions)(void);
  pFlushPredictions = &CSessionState::FlushProcessedPredictions;
  NewPatch(pFlushPredictions, &CSessionStatePatch::P_FlushProcessedPredictions, "CSessionState::FlushProcessedPredictions()");

  void (CSessionState::*pProcGameStreamBlock)(CNetworkMessage &) = &CSessionState::ProcessGameStreamBlock;
  NewPatch(pProcGameStreamBlock, &CSessionStatePatch::P_ProcessGameStreamBlock, "CSessionState::ProcessGameStreamBlock(...)");

  void (CSessionState::*pStopSession)(void) = &CSessionState::Stop;
  NewPatch(pStopSession, &CSessionStatePatch::P_Stop, "CSessionState::Stop()");

  void (CSessionState::*pMakeSyncCheck)(void) = &CSessionState::MakeSynchronisationCheck;
  NewPatch(pMakeSyncCheck, &CSessionStatePatch::P_MakeSynchronisationCheck, "CSessionState::MakeSynchronisationCheck()");

  // CPlayerEntity
  void (CPlayerEntity::*pPlayerWrite)(CTStream *) = NULL;
  pPlayerWrite = StructPtr(ADDR_PLAYER_WRITE)(&CPlayerEntity::Write_t);
  NewPatch(pPlayerWrite, &CPlayerEntityPatch::P_Write, "CPlayerEntity::Write_t(...)");

  void (CPlayerEntity::*pChecksumForSync)(ULONG &, INDEX) = NULL;
  pChecksumForSync = StructPtr(ADDR_PLAYER_CHECKSUM)(&CPlayerEntity::ChecksumForSync);
  NewPatch(pChecksumForSync, &CPlayerEntityPatch::P_ChecksumForSync, "CPlayerEntity::ChecksumForSync(...)");

  // Custom symbols
  static BOOL bMaskGUIDsCommand = TRUE;
  _pShell->DeclareSymbol("user void UpdateMaskGUIDs(INDEX);", &UpdateMaskGUIDs);
  _pShell->DeclareSymbol("user INDEX ser_bMaskGUIDs post:UpdateMaskGUIDs;", &bMaskGUIDsCommand);
};

#include "Patches/Rendering.h"

void CPatches::Rendering(void) {
  void (*pRenderView)(CWorld &, CEntity &, CAnyProjection3D &, CDrawPort &) = &RenderView;
  NewPatch(pRenderView, &P_RenderView, "::RenderView(...)");

  // Pointer to the virtual table of CPerspectiveProjection3D
  size_t *pVFTable = *(size_t **)&CPerspectiveProjection3D();

  // Pointer to CPerspectiveProjection3D::Prepare()
  typedef void (CPerspectiveProjection3D::*CPrepareFunc)(void);
  CPrepareFunc pPrepare = *(CPrepareFunc *)(pVFTable + 0);

  NewPatch(pPrepare, &CProjectionPatch::P_Prepare, "CPerspectiveProjection3D::Prepare()");

  // Custom symbols
  _pShell->DeclareSymbol("persistent user INDEX sam_bAdjustForAspectRatio;", &_EnginePatches._bAdjustForAspectRatio);
  _pShell->DeclareSymbol("persistent user INDEX sam_bUseVerticalFOV;", &_EnginePatches._bUseVerticalFOV);
  _pShell->DeclareSymbol("persistent user FLOAT sam_fCustomFOV;",      &_EnginePatches._fCustomFOV);
  _pShell->DeclareSymbol("persistent user FLOAT sam_fThirdPersonFOV;", &_EnginePatches._fThirdPersonFOV);
  _pShell->DeclareSymbol("           user INDEX sam_bCheckFOV;",       &_EnginePatches._bCheckFOV);
};

#if SE1_VER >= SE1_107

#include "Patches/Ska.h"

// SKA models have been patched
static BOOL _bSkaPatched = FALSE;

void CPatches::ShadersPatches(void) {
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
};

void CPatches::Ska(void) {
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
};

#endif

#include "Patches/SoundLibrary.h"

void CPatches::SoundLibrary(void) {
  void (CSoundLibrary::*pListen)(CSoundListener &) = &CSoundLibrary::Listen;
  NewPatch(pListen, &CSoundLibPatch::P_Listen, "CSoundLibrary::Listen(...)");
};

#include "Patches/Strings.h"

void CPatches::Strings(void) {
  INDEX (CTString::*pVPrintF)(const char *, va_list) = &CTString::VPrintF;
  NewPatch(pVPrintF, &CStringPatch::P_VPrintF, "CTString::VPrintF(...)");

  CTString (CTString::*pUndecorated)(void) const = &CTString::Undecorated;
  NewPatch(pUndecorated, &CStringPatch::P_Undecorated, "CTString::Undecorated()");
};

#include "Patches/Textures.h"

void CPatches::Textures(void) {
  void (CTextureData::*pCreateTex)(const CImageInfo *, MEX, INDEX, int) = &CTextureData::Create_t;
  NewPatch(pCreateTex, &CTexDataPatch::P_Create, "CTextureData::Create_t(...)");

  // Pointer to the virtual table of CTextureData
  size_t *pVFTable = *(size_t **)&CTextureData();

  // Pointer to CTextureData::Write_t()
  typedef void (CTextureData::*CWriteTexFunc)(CTStream *);
  CWriteTexFunc pWriteTex = *(CWriteTexFunc *)(pVFTable + 4);

  NewPatch(pWriteTex, &CTexDataPatch::P_Write, "CTextureData::Write_t(...)");

  void (*pProcessScript)(const CTFileName &) = &ProcessScript_t;
  NewPatch(pProcessScript, &P_ProcessTextureScript, "ProcessScript_t(...)");

  void (*pCreateTextureOut)(const CTFileName &, const CTFileName &, MEX, INDEX, int) = &CreateTexture_t;
  NewPatch(pCreateTextureOut, &P_CreateTextureOut, "CreateTexture_t(out)");

  void (*pCreateTexture)(const CTFileName &, MEX, INDEX, int) = &CreateTexture_t;
  NewPatch(pCreateTexture, &P_CreateTexture, "CreateTexture_t(...)");
};

#include "Patches/Worlds.h"

void CPatches::Worlds(void) {
  void (CWorld::*pWorldLoad)(const CTFileName &) = &CWorld::Load_t;
  NewPatch(pWorldLoad, &CWorldPatch::P_Load, "CWorld::Load_t(...)");

  // Custom symbols
  _pShell->DeclareSymbol("user INDEX sam_bReinitWorld;", &_EnginePatches._bReinitWorld);
};

#include "Patches/UnpageStreams.h"

// Specific stream patching
static void PatchStreams(void) {
  // Streams have been patched
  static BOOL _bStreamsPatched = FALSE;

  if (_bStreamsPatched) return;
  _bStreamsPatched = TRUE;

  // CTStream
  void (CTStream::*pAllocMemory)(ULONG) = &CTStream::AllocateVirtualMemory;
  NewRawPatch(pAllocMemory, &CStreamPatch::P_AllocVirtualMemory, "CTStream::AllocateVirtualMemory(...)");

  void (CTStream::*pFreeBufferFunc)(void) = &CTStream::FreeBuffer;
  NewRawPatch(pFreeBufferFunc, &CStreamPatch::P_FreeBuffer, "CTStream::FreeBuffer()");

  // CTFileStream
  void (CTFileStream::*pCreateFunc)(const CTFileName &, CTStream::CreateMode) = &CTFileStream::Create_t;
  NewRawPatch(pCreateFunc, &CFileStreamPatch::P_Create, "CTFileStream::Create_t(...)");

  void (CTFileStream::*pOpenFunc)(const CTFileName &, CTStream::OpenMode) = &CTFileStream::Open_t;
  NewRawPatch(pOpenFunc, &CFileStreamPatch::P_Open, "CTFileStream::Open_t(...)");

  void (CTFileStream::*pCloseFunc)(void) = &CTFileStream::Close;
  NewRawPatch(pCloseFunc, &CFileStreamPatch::P_Close, "CTFileStream::Close()");
};

void CPatches::UnpageStreams(void) {
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
};

#include "Patches/FileSystem.h"

void CPatches::FileSystem(void) {
  #ifdef _DEBUG
    PatchStreams();
  #endif

  extern void (*pInitStreams)(void);
  pInitStreams = StructPtr(ADDR_INITSTREAMS)(&P_InitStreams);
  NewRawPatch(pInitStreams, &P_InitStreams, "::InitStreams()");

  void (*pMakeDirList)(CFileList &, const CTFileName &, const CTString &, ULONG) = &MakeDirList;
  NewRawPatch(pMakeDirList, &P_MakeDirList, "::MakeDirList(...)");

  INDEX (*pExpandFilePath)(ULONG, const CTFileName &, CTFileName &) = &ExpandFilePath;
  NewRawPatch(pExpandFilePath, &P_ExpandFilePath, "::ExpandFilePath(...)");
};
