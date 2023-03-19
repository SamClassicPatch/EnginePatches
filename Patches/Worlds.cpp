/* Copyright (c) 2023 Dreamy Cecil
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

#include "Worlds.h"
#include "../MapConversion.h"

#include <CoreLib/Base/Unzip.h>

void CWorldPatch::P_Load(const CTFileName &fnmWorld) {
  // Open the file
  wo_fnmFileName = fnmWorld;

  CTFileStream strmFile;
  strmFile.Open_t(fnmWorld);

  // Check engine build
  BOOL bNeedsReinit;
  _pNetwork->CheckVersion_t(strmFile, TRUE, bNeedsReinit);

  // Read the world
  Read_t(&strmFile);

  strmFile.Close();

  // [Cecil] Forced reinitialization
  BOOL bForceReinit = _EnginePatches._bReinitWorld;

  // [Cecil] Check if the level is being loaded from the TFE directory
  #if TSE_FUSION_MODE
    // Reset TFE state in TSE
    _EnginePatches._bFirstEncounter = FALSE;

    if (_fnmCDPath != "") {
      CTFileName fnmFull;

      // Try checking the archive path
      if (ExpandFilePath(EFP_READ, fnmWorld, fnmFull) == EFP_BASEZIP) {
        fnmFull = IUnzip::GetFileArchive(fnmWorld);
      }

      _EnginePatches._bFirstEncounter = fnmFull.HasPrefix(_fnmCDPath);
    }

    // Reinitialize TFE for TSE
    bForceReinit |= _EnginePatches._bFirstEncounter;

  #else
    // Already playing TFE in TFE
    _EnginePatches._bFirstEncounter = TRUE;
  #endif

  // [Cecil] Convert the world some specific way while in game
  if (!GetAPI()->IsEditorApp() && bForceReinit) {
    SetProgressDescription(LOCALIZE("converting from old version"));
    CallProgressHook_t(0.0f);

    // Must be in 24bit mode when managing entities
    CSetFPUPrecision FPUPrecision(FPT_24BIT);
    CTmpPrecachingNow tpn;

    #if TSE_FUSION_MODE
      // Make TFE worlds TSE-compatible
      if (_EnginePatches._bFirstEncounter) {
        IConvertTFE::ConvertWorld(this);
      }
    #endif

    // Reset every entity
    if (_EnginePatches._bReinitWorld) {
      FOREACHINDYNAMICCONTAINER(wo_cenEntities, CEntity, iten) {
        CallProgressHook_t((FLOAT)iten.GetIndex() / (FLOAT)wo_cenEntities.Count());

        // Reinitialize only rational entities
        if (IWorld::IsRationalEntity(iten)) {
          iten->Reinitialize();
        }
      }
    }

    CallProgressHook_t(1.0f);

  // Reinitialize and resave old levels, if needed
  } else if (bNeedsReinit) {
    SetProgressDescription(LOCALIZE("converting from old version"));
    CallProgressHook_t(0.0f);
    ReinitializeEntities();
    CallProgressHook_t(1.0f);

    SetProgressDescription(LOCALIZE("saving converted file"));
    CallProgressHook_t(0.0f);
    Save_t(fnmWorld);
    CallProgressHook_t(1.0f);
  }

  // [Cecil] Call API method after loading the world
  GetAPI()->OnWorldLoad(this, fnmWorld);
};
