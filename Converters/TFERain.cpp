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

#include "MapConversion.h"

#include <CoreLib/Objects/PropertyPtr.h>

#if CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE

// Structure with rain properties for a specific CWorldSettingsController
struct SRainProps {
  CEntity *penWSC; // Pointer to CWorldSettingsController

  CTFileName fnm; // CWorldSettingsController::m_fnHeightMap
  FLOATaabbox3D box; // CWorldSettingsController::m_boxHeightMap

  // Constructor
  SRainProps() : penWSC(NULL)
  {
  };
};

// List of rain properties for each controller in the world
static CStaticStackArray<SRainProps> _aRainProps;

// List of triggers and storm controllers in the world
CEntities _cWorldTriggers;
CEntities _cWorldStorms;

// First and last created environment particles holders
static CEntity *_penFirstEPH = NULL;
static CEntity *_penLastEPH = NULL;

namespace IRainTFE {

// Clear rain variables
void ClearRainVariables(void) {
  _aRainProps.PopAll();

  _cWorldTriggers.Clear();
  _cWorldStorms.Clear();
  
  _penFirstEPH = NULL;
  _penLastEPH = NULL;
};

// Remember rain properties of CWorldSettingsController
void RememberWSC(CEntity *penWSC, const IMapConverters::UnknownProp &prop) {
  // Find existing entry for this controller
  SRainProps *pRain = NULL;
  INDEX ct = _aRainProps.Count();

  for (INDEX iRain = 0; iRain < ct; iRain++) {
    SRainProps &rainCheck = _aRainProps[iRain];

    if (rainCheck.penWSC == penWSC) {
      pRain = &rainCheck;
      break;
    }
  }

  // Create new rain properties entry
  if (pRain == NULL) pRain = &_aRainProps.Push();

  pRain->penWSC = penWSC;

  // Remember height map texture and limits
  ULONG ulIndex = prop.ulID & 0xFF;

  if (prop.ulType == CEntityProperty::EPT_FILENAME && ulIndex == 28) {
    pRain->fnm = prop.Filename();

  } else if (prop.ulType == CEntityProperty::EPT_FLOATAABBOX3D && ulIndex == 30) {
    pRain->box = prop.Box();
  }
};

// Create a new trigger that targets storm & EPH
static CEntity *CreateStormTrigger(CPropertyPtr *apProps, INDEX iEvent, CEntity *penStorm)
{
  CEntity *penTrigger = NULL;

  try {
    static const CTString strEnvClass = "Classes\\Trigger.ecl";
    penTrigger = IWorld::GetWorld()->CreateEntity_t(IDummy::plCenter, strEnvClass);

    // Set first two targets and target events
    ENTITYPROPERTY(penTrigger, apProps[0].Offset(), CEntityPointer) = penStorm;
    ENTITYPROPERTY(penTrigger, apProps[1].Offset(), CEntityPointer) = _penFirstEPH;

    ENTITYPROPERTY(penTrigger, apProps[10].Offset(), INDEX) = iEvent;

    // Enable instant rain for EPH
    if (iEvent == 6) {
      iEvent = 0; // EventEType::EET_ENVIRONMENTSTART -> EventEType::EET_START
    }

    ENTITYPROPERTY(penTrigger, apProps[11].Offset(), INDEX) = iEvent;

    penTrigger->Initialize();

  } catch (char *strError) {
    FatalError(TRANS("Cannot load %s class:\n%s"), "Trigger", strError);
  }

  return penTrigger;
};

// Apply remembered rain properties from controllers
void ApplyRainProperties(void) {
  // Pair environment particles with each world settings controller
  INDEX ct = _aRainProps.Count();

  for (INDEX iRain = 0; iRain < ct; iRain++) {
    const SRainProps &rain = _aRainProps[iRain];
    CEntity *penWSC = rain.penWSC;

    try {
      static const CTString strEnvClass = "Classes\\EnvironmentParticlesHolder.ecl";
      CEntity *penEnv = IWorld::GetWorld()->CreateEntity_t(IDummy::plCenter, strEnvClass);

      // Set first EPH
      if (_penFirstEPH == NULL) _penFirstEPH = penEnv;

      // Retrieve environment properties
      static CPropertyPtr pptrTex(penEnv); // CEnvironmentParticlesHolder::m_fnHeightMap
      static CPropertyPtr pptrArea(penEnv); // CEnvironmentParticlesHolder::m_boxHeightMap
      static CPropertyPtr pptrType(penEnv); // CEnvironmentParticlesHolder::m_eptType
      static CPropertyPtr pptrNext(penEnv); // CEnvironmentParticlesHolder::m_penNextHolder
      static CPropertyPtr pptrRain(penEnv); // CEnvironmentParticlesHolder::m_fRainAppearLen

      // Setup rain
      if (pptrTex.ByNameOrId(CEntityProperty::EPT_FILENAME, "Height map", (0xED << 8) + 2)
       && pptrArea.ByNameOrId(CEntityProperty::EPT_FLOATAABBOX3D, "Height map box", (0xED << 8) + 3)
       && pptrType.ByNameOrId(CEntityProperty::EPT_ENUM, "Type", (0xED << 8) + 4)
       && pptrRain.ByNameOrId(CEntityProperty::EPT_FLOAT, "Rain start duration", (0xED << 8) + 70))
      {
        ENTITYPROPERTY(penEnv, pptrTex.Offset(), CTFileName) = rain.fnm;
        ENTITYPROPERTY(penEnv, pptrArea.Offset(), FLOATaabbox3D) = rain.box;
        ENTITYPROPERTY(penEnv, pptrType.Offset(), INDEX) = 2; // EnvironmentParticlesHolderType::EPTH_RAIN
        ENTITYPROPERTY(penEnv, pptrRain.Offset(), FLOAT) = 3.0f; // Average duration for the rain to start/stop
      }

      // Initialize and start the rain
      penEnv->Initialize();

      // Connect last EPH with this one
      if (pptrNext.ByNameOrId(CEntityProperty::EPT_ENTITYPTR, "Next env. particles holder", (0xED << 8) + 5)) {
        if (_penLastEPH != NULL) {
          ENTITYPROPERTY(_penLastEPH, pptrNext.Offset(), CEntityPointer) = penEnv;
        }
      }

      _penLastEPH = penEnv;

      // Set pointer to the new environment particles holder
      static CPropertyPtr pptrEnvPtr(penWSC); // CWorldSettingsController::m_penEnvPartHolder

      if (pptrEnvPtr.ByNameOrId(CEntityProperty::EPT_ENTITYPTR, "Environment Particles Holder", (0x25D << 8) + 28)) {
        ENTITYPROPERTY(penWSC, pptrEnvPtr.Offset(), CEntityPointer) = _penFirstEPH;
      }

    } catch (char *strError) {
      FatalError(TRANS("Cannot load %s class:\n%s"), "EnvironmentParticlesHolder", strError);
    }
  }

  // No rain particles
  if (_penFirstEPH == NULL) return;

  // Find triggers that target storm controllers
  FOREACHINDYNAMICCONTAINER(_cWorldTriggers, CEntity, itenTrigger) {
    CEntity *pen = itenTrigger;

    static CPropertyPtr apProps[20] = {
      // CTrigger::m_penTarget1 .. CTrigger::m_penTarget10
      CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen),
      CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen),

      // CTrigger::m_eetEvent1 .. CTrigger::m_eetEvent10
      CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen),
      CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen), CPropertyPtr(pen),
    };

    // Names of target & event entity properties
    static const char *astrTargets[10] = {
      "Target 01", "Target 02", "Target 03", "Target 04", "Target 05",
      "Target 06", "Target 07", "Target 08", "Target 09", "Target 10",
    };

    static const char *astrEvents[10] = {
      "Event type Target 01", "Event type Target 02", "Event type Target 03", "Event type Target 04", "Event type Target 05",
      "Event type Target 06", "Event type Target 07", "Event type Target 08", "Event type Target 09", "Event type Target 10",
    };

    // Indices of target & event entity properties
    static const INDEX aiTargets[10] = {
      (0xCD << 8) +  3, (0xCD << 8) +  4, (0xCD << 8) +  5, (0xCD << 8) +  6, (0xCD << 8) +  7,
      (0xCD << 8) + 20, (0xCD << 8) + 21, (0xCD << 8) + 22, (0xCD << 8) + 23, (0xCD << 8) + 24,
    };

    static const INDEX aiEvents[10] = {
      (0xCD << 8) +  8, (0xCD << 8) +  9, (0xCD << 8) + 10, (0xCD << 8) + 11, (0xCD << 8) + 12,
      (0xCD << 8) + 50, (0xCD << 8) + 51, (0xCD << 8) + 52, (0xCD << 8) + 53, (0xCD << 8) + 54,
    };

    // Go through all target properties
    for (INDEX i = 0; i < 10; i++) {
      // Find properties
      if (!apProps[i].ByNameOrId(CEntityProperty::EPT_ENTITYPTR, astrTargets[i], aiTargets[i])) continue;
      if (!apProps[i + 10].ByNameOrId(CEntityProperty::EPT_ENUM, astrEvents[i], aiEvents[i])) continue;

      // Check if it points at some storm controller
      CEntityPointer &penTarget = ENTITYPROPERTY(pen, apProps[i].Offset(), CEntityPointer);
      INDEX &iEvent = ENTITYPROPERTY(pen, apProps[i + 10].Offset(), INDEX);

      FOREACHINDYNAMICCONTAINER(_cWorldStorms, CEntity, itenStorm) {
        if (penTarget != &itenStorm.Current()) continue;

        // Replace with a new trigger that points at both storm & EPH
        penTarget = CreateStormTrigger(apProps, iEvent, penTarget.ep_pen);
        iEvent = 2; // EventEType::EET_TRIGGER
        break;
      }
    }
  }

  // Clear the rain
  ClearRainVariables();
};

}; // namespace

#endif // CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE
