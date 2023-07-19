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

  // [Cecil] Loading from the current game directory
  _EnginePatches._eWorldFormat = E_LF_CURRENT;

  // [Cecil] Check if the level is being loaded from TFE
  #if TSE_FUSION_MODE
    if (IsFileFromDir(GAME_DIR_TFE, fnmWorld)) {
      _EnginePatches._eWorldFormat = E_LF_TFE;
    }
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

  // [Cecil] Patched methods may set a different value to CPatches::_eWorldFormat
  // Read the world
  Read_t(&strmFile);

  strmFile.Close();

  // [Cecil] Determine forced reinitialization and forcefully convert other world types
  BOOL bForceReinit = _EnginePatches._bReinitWorld;
  bForceReinit |= (_EnginePatches._eWorldFormat != E_LF_CURRENT);

  // [Cecil] Convert the world some specific way while in game
  if (!GetAPI()->IsEditorApp() && bForceReinit) {
    SetProgressDescription(LOCALIZE("converting from old version"));
    CallProgressHook_t(0.0f);

    // Must be in 24bit mode when managing entities
    CSetFPUPrecision FPUPrecision(FPT_24BIT);
    CTmpPrecachingNow tpn;

    #if CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE
      // Make TFE worlds TSE-compatible
      if (_EnginePatches._eWorldFormat == E_LF_TFE) {
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

// Read world information
void CWorldPatch::P_ReadInfo(CTStream *strm, BOOL bMaybeDescription) {
  // Read entire world info
  if (strm->PeekID_t() == CChunkID("WLIF")) {
    strm->ExpectID_t(CChunkID("WLIF"));

    // [Cecil] "DTRS" in the EXE as is gets picked up by the Depend utility
    static const CChunkID chnkDTRS(CTString("DT") + "RS");

    if (strm->PeekID_t() == chnkDTRS) {
      strm->ExpectID_t(chnkDTRS);
    }

    // [Cecil] Rev: Read new world info
    {
      CTString strLeaderboard = "";
      ULONG aulExtra[3] = { 0, 0, 0 };

      // Read leaderboard
      if (strm->PeekID_t() == CChunkID("LDRB")) {
        strm->ExpectID_t("LDRB");
        *strm >> strLeaderboard;

        _EnginePatches._eWorldFormat = E_LF_SSR;
      }

      // Read unknown values
      if (strm->PeekID_t() == CChunkID("Plv0")) {
        strm->ExpectID_t("Plv0");
        *strm >> aulExtra[0];
        *strm >> aulExtra[1];
        *strm >> aulExtra[2];

        _EnginePatches._eWorldFormat = E_LF_SSR;
      }

      // Set default or read values
      #if SE1_GAME == SS_REV
        wo_strLeaderboard = strLeaderboard;
        wo_ulExtra3 = aulExtra[0];
        wo_ulExtra4 = aulExtra[1];
        wo_ulExtra5 = aulExtra[2];
      #endif
    }

    // Read display name
    *strm >> wo_strName;

    // Read flags
    *strm >> wo_ulSpawnFlags;

    // [Cecil] Rev: Read special gamemode chunk
    if (strm->PeekID_t() == CChunkID("SpGM")) {
      strm->ExpectID_t("SpGM");
      _EnginePatches._eWorldFormat = E_LF_SSR;

    } else {
      // Otherwise remove some flags
      #if SE1_GAME == SS_REV
        wo_ulSpawnFlags &= ~0xE00000;
      #endif
    }

    // Read world description
    *strm >> wo_strDescription;

  // Only read description
  } else if (bMaybeDescription) {
    *strm >> wo_strDescription;
  }
};

#endif // CLASSICSPATCH_ENGINEPATCHES
