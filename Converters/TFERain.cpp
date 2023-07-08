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
      static CPropertyPtr pptrHght(penEnv); // CEnvironmentParticlesHolder::m_fnHeightMap
      static CPropertyPtr pptrArea(penEnv); // CEnvironmentParticlesHolder::m_boxHeightMap
      static CPropertyPtr pptrType(penEnv); // CEnvironmentParticlesHolder::m_eptType
      static CPropertyPtr pptrNext(penEnv); // CEnvironmentParticlesHolder::m_penNextHolder
      static CPropertyPtr pptrRain(penEnv); // CEnvironmentParticlesHolder::m_fRainAppearLen

      // Setup rain
      if (pptrHght.ByVariable("CEnvironmentParticlesHolder", "m_fnHeightMap")
       && pptrArea.ByVariable("CEnvironmentParticlesHolder", "m_boxHeightMap")
       && pptrType.ByVariable("CEnvironmentParticlesHolder", "m_eptType")
       && pptrRain.ByVariable("CEnvironmentParticlesHolder", "m_fRainAppearLen"))
      {
        ENTITYPROPERTY(penEnv, pptrHght.Offset(), CTFileName) = rain.fnm;
        ENTITYPROPERTY(penEnv, pptrArea.Offset(), FLOATaabbox3D) = rain.box;
        ENTITYPROPERTY(penEnv, pptrType.Offset(), INDEX) = 2; // EnvironmentParticlesHolderType::EPTH_RAIN
        ENTITYPROPERTY(penEnv, pptrRain.Offset(), FLOAT) = 3.0f; // Average duration for the rain to start/stop
      }

      // Initialize and start the rain
      penEnv->Initialize();

      // Connect last EPH with this one
      if (pptrNext.ByVariable("CEnvironmentParticlesHolder", "m_penNextHolder")) {
        if (_penLastEPH != NULL) {
          ENTITYPROPERTY(_penLastEPH, pptrNext.Offset(), CEntityPointer) = penEnv;
        }
      }

      _penLastEPH = penEnv;

      // Set pointer to the new environment particles holder
      static CPropertyPtr pptrEnvPtr(penWSC); // CWorldSettingsController::m_penEnvPartHolder

      if (pptrEnvPtr.ByVariable("CWorldSettingsController", "m_penEnvPartHolder")) {
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
    static const char *astrPropVars[20] = {
      "m_penTarget1", "m_penTarget2", "m_penTarget3", "m_penTarget4", "m_penTarget5",
      "m_penTarget6", "m_penTarget7", "m_penTarget8", "m_penTarget9", "m_penTarget10",
      "m_eetEvent1",  "m_eetEvent2",  "m_eetEvent3",  "m_eetEvent4",  "m_eetEvent5",
      "m_eetEvent6",  "m_eetEvent7",  "m_eetEvent8",  "m_eetEvent9",  "m_eetEvent10",
    };

    // Go through all target properties
    for (INDEX i = 0; i < 10; i++) {
      // Find properties
      INDEX iProp = i;

      if (!apProps[iProp].ByVariable("CTrigger", astrPropVars[iProp])) continue;
      CEntityPointer &penTarget = ENTITYPROPERTY(pen, apProps[iProp].Offset(), CEntityPointer);

      iProp += 10;

      if (!apProps[iProp].ByVariable("CTrigger", astrPropVars[iProp])) continue;
      INDEX &iEvent = ENTITYPROPERTY(pen, apProps[iProp].Offset(), INDEX);

      // Check if it points at some storm controller
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
