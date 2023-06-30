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

#if CLASSICSPATCH_ENGINEPATCHES

#include "Worlds.h"
#include "../MapConversion.h"

void CWorldPatch::P_Load(const CTFileName &fnmWorld) {
  // Open the file
  wo_fnmFileName = fnmWorld;

  // [Cecil] Determine forced reinitialization
  BOOL bForceReinit = _EnginePatches._bReinitWorld;

  // [Cecil] Check if the level is being loaded from the TFE directory
  _EnginePatches._bFirstEncounter = _EnginePatches.IsMapFromTFE(fnmWorld);

  // [Cecil] Reinitialize TFE for TSE
  #if TSE_FUSION_MODE
    bForceReinit |= _EnginePatches._bFirstEncounter;
  #endif

  // [Cecil] Reset map converters
  #if CLASSICSPATCH_CONVERT_MAPS
    IMapConverters::Reset();
  #endif

  CTFileStream strmFile;
  strmFile.Open_t(fnmWorld);

  // Check engine build
  BOOL bNeedsReinit;
  _pNetwork->CheckVersion_t(strmFile, TRUE, bNeedsReinit);

  // Read the world
  Read_t(&strmFile);

  strmFile.Close();

  // [Cecil] Convert the world some specific way while in game
  if (!GetAPI()->IsEditorApp() && bForceReinit) {
    SetProgressDescription(LOCALIZE("converting from old version"));
    CallProgressHook_t(0.0f);

    // Must be in 24bit mode when managing entities
    CSetFPUPrecision FPUPrecision(FPT_24BIT);
    CTmpPrecachingNow tpn;

    #if CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE
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

#endif // CLASSICSPATCH_ENGINEPATCHES
