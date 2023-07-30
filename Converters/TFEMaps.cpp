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

// Classes that need to be converted
#include <EntitiesV/StdH/StdH.h>
#include <EntitiesV/AmmoPack.h>
#include <EntitiesV/Camera.h>
#include <EntitiesV/Devil.h>
#include <EntitiesV/DoorController.h>
#include <EntitiesV/EnemyBase.h>
#include <EntitiesV/EnemySpawner.h>
#include <EntitiesV/KeyItem.h>
#include <EntitiesV/Lightning.h>
#include <EntitiesV/MovingBrush.h>
#include <EntitiesV/PlayerMarker.h>
#include <EntitiesV/PyramidSpaceShip.h>
#include <EntitiesV/StormController.h>
#include <EntitiesV/Trigger.h>
#include <EntitiesV/Woman.h>
#include <EntitiesV/WorldSettingsController.h>

#if CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE

// Converter instance
IConvertTFE _convTFE;

// Reset the converter before loading a new world
void IConvertTFE::Reset(void) {
  // Clear the rain
  ClearRainVariables();
};

// Handle some unknown property
void IConvertTFE::HandleProperty(CEntity *pen, const UnknownProp &prop)
{
  if (IsOfClassID(pen, CWorldSettingsController_ClassID)) {
    RememberWSC(pen, prop);
  }
};

// Convert invalid weapon flag in a mask
void IConvertTFE::ConvertWeapon(INDEX &iFlags, INDEX iWeapon) {
  switch (iWeapon) {
    case IMapsTFE::WEAPON_LASER: iFlags |= WeaponFlag(IMapsTSE::WEAPON_LASER); break;
    case IMapsTFE::WEAPON_IRONCANNON: iFlags |= WeaponFlag(IMapsTSE::WEAPON_IRONCANNON); break;

    // Nonexistent weapons
    case 10: case 11: case 12: case 13: case 15: case 17: break;

    // Others
    default: iFlags |= WeaponFlag(iWeapon);
  }
};

// Convert invalid key types
void IConvertTFE::ConvertKeyType(INDEX &eKey) {
  switch (eKey) {
    // Dummy keys
    case 4: eKey = IMapsTSE::KIT_TABLESDUMMY; break; // Gold ankh
    case 15: eKey = IMapsTSE::KIT_TABLESDUMMY; break; // Metropolis scarab

    // Element keys
    case 5: eKey = IMapsTSE::KIT_CROSSWOODEN; break; // Earth
    case 6: eKey = IMapsTSE::KIT_CROSSMETAL; break; // Water
    case 7: eKey = IMapsTSE::KIT_CRYSTALSKULL; break; // Air
    case 8: eKey = IMapsTSE::KIT_CROSSGOLD; break; // Fire

    // Other keys
    case 0:  eKey = IMapsTSE::KIT_CROSSWOODEN; break;
    case 1:  eKey = IMapsTSE::KIT_CROSSMETAL; break;
    case 2:  eKey = IMapsTSE::KIT_CROSSGOLD; break;
    case 3:  eKey = IMapsTSE::KIT_KINGSTATUE; break;
    case 9:  eKey = IMapsTSE::KIT_HOLYGRAIL; break;
    case 10: eKey = IMapsTSE::KIT_BOOKOFWISDOM; break;
    case 12: eKey = IMapsTSE::KIT_BOOKOFWISDOM; break;
    case 13: eKey = IMapsTSE::KIT_STATUEHEAD03; break; // Metropolis scarab
    case 14: eKey = IMapsTSE::KIT_HOLYGRAIL; break;
    case 16: eKey = IMapsTSE::KIT_STATUEHEAD01; break; // Luxor pair
    case 17: eKey = IMapsTSE::KIT_STATUEHEAD02; break; // Luxor pair
    case 18: eKey = IMapsTSE::KIT_WINGEDLION; break; // Sacred Yards sphinx
    case 19: eKey = IMapsTSE::KIT_ELEPHANTGOLD; break; // Sacred Yards sphinx

    // Edge case
    default: eKey = IMapsTSE::KIT_KINGSTATUE; break;
  }
};

// Convert one specific entity without reinitializing it
BOOL IConvertTFE::ConvertEntity(CEntity *pen) {
  enum {
    EN_DOOR, EN_KEY, EN_START, EN_PACK, EN_STORM,
  } eEntity;

  // Check for the entities that need to be patched and remember their type
  if (IsOfClassID(pen, CDoorController_ClassID)) {
    eEntity = EN_DOOR;
  } else if (IsOfClassID(pen, CKeyItem_ClassID)) {
    eEntity = EN_KEY;
  } else if (IsOfClassID(pen, CPlayerMarker_ClassID)) {
    eEntity = EN_START;
  } else if (IsOfClassID(pen, CAmmoPack_ClassID)) {
    eEntity = EN_PACK;
  } else if (IsOfClassID(pen, CStormController_ClassID)) {
    eEntity = EN_STORM;

  // Invalid entity
  } else {
    return FALSE;
  }

  // Remove napalm and sniper bullets
  if (eEntity == EN_PACK) {
    // Retrieve CAmmoPack::m_iNapalm and CAmmoPack::m_iSniperBullets
    static CPropertyPtr pptrNapalm(pen);
    static CPropertyPtr pptrSniper(pen);

    if (pptrNapalm.ByVariable("CAmmoPack", "m_iNapalm")) {
      ENTITYPROPERTY(pen, pptrNapalm.Offset(), INDEX) = 0;
    }

    if (pptrSniper.ByVariable("CAmmoPack", "m_iSniperBullets")) {
      ENTITYPROPERTY(pen, pptrSniper.Offset(), INDEX) = 0;
    }

  // Adjust weapon masks
  } else if (eEntity == EN_START) {
    // Retrieve CPlayerMarker::m_iGiveWeapons and CPlayerMarker::m_iTakeWeapons
    static CPropertyPtr pptrGive(pen);
    static CPropertyPtr pptrTake(pen);

    // Properties don't exist
    if (!pptrGive.ByVariable("CPlayerMarker", "m_iGiveWeapons")) {
      return FALSE;
    }

    if (!pptrTake.ByVariable("CPlayerMarker", "m_iTakeWeapons")) {
      return FALSE;
    }

    INDEX &iGiveWeapons = ENTITYPROPERTY(pen, pptrGive.Offset(), INDEX);
    INDEX &iTakeWeapons = ENTITYPROPERTY(pen, pptrTake.Offset(), INDEX);

    INDEX iNewGive = 0x03; // Knife and Colt
    INDEX iNewTake = 0;

    for (INDEX i = 1; i < 18; i++) {
      // Replace the weapon if it exists
      if (iGiveWeapons & WeaponFlag(i)) {
        ConvertWeapon(iNewGive, i);
      }

      if (iTakeWeapons & WeaponFlag(i)) {
        ConvertWeapon(iNewTake, i);
      }
    }

    iGiveWeapons = iNewGive;
    iTakeWeapons = iNewTake;

  // Adjust keys
  } else if (eEntity == EN_KEY) {
    // Retrieve CKeyItem::m_kitType and CKeyItem::m_iSoundComponent
    static CPropertyPtr pptrType(pen);
    static CPropertyPtr pptrSound(pen);

    // Convert key type
    if (pptrType.ByVariable("CKeyItem", "m_kitType")) {
      ConvertKeyType(ENTITYPROPERTY(pen, pptrType.Offset(), INDEX));
    }

    // Fix sound component index (301 -> 300)
    if (pptrSound.ByVariable("CKeyItem", "m_iSoundComponent")) {
      ENTITYPROPERTY(pen, pptrSound.Offset(), INDEX) = (0x325 << 8) + 300;
    }

  // Adjust keys
  } else if (eEntity == EN_DOOR) {
    // Retrieve CDoorController::m_dtType and CDoorController::m_kitKey
    static CPropertyPtr pptrType(pen);
    static CPropertyPtr pptrKey(pen);

    // Property doesn't exist
    if (!pptrType.ByVariable("CDoorController", "m_dtType")) {
      return FALSE;
    }

    // Only for locked doors (DT_LOCKED)
    if (ENTITYPROPERTY(pen, pptrType.Offset(), INDEX) != 2) {
      return FALSE;
    }

    // Convert key type
    if (pptrKey.ByVariable("CDoorController", "m_kitKey")) {
      ConvertKeyType(ENTITYPROPERTY(pen, pptrKey.Offset(), INDEX));
    }

  // Adjust storm shade color
  } else if (eEntity == EN_STORM) {
    // Retrieve CStormController::m_colShadeStart and CStormController::m_colShadeStop
    static CPropertyPtr pptrShadeStart(pen);
    static CPropertyPtr pptrShadeStop(pen);

    // Properties don't exist
    if (!pptrShadeStart.ByVariable("CStormController", "m_colShadeStart")) {
      return FALSE;
    }

    if (!pptrShadeStop.ByVariable("CStormController", "m_colShadeStop")) {
      return FALSE;
    }

    // Matches gray scale in the 64-255 brightness range
    ENTITYPROPERTY(pen, pptrShadeStart.Offset(), COLOR) = C_WHITE | CT_OPAQUE;
    ENTITYPROPERTY(pen, pptrShadeStop.Offset(), COLOR) = C_dGRAY | CT_OPAQUE;

    // Add to the list of storm controllers
    cenStorms.Add(pen);

    // Proceed with reinitialization
    return FALSE;
  }

  return TRUE;
};

// Convert the entire world with possible entity reinitializations
void IConvertTFE::ConvertWorld(CWorld *pwo) {
  CEntities cEntities;

  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    CEntity *pen = iten;

    // Convert specific entities regardless of state first
    if (ConvertEntity(pen)) continue;

    // Remember triggers for future use
    if (IsOfClassID(pen, CTrigger_ClassID)) {
      cenTriggers.Add(pen);
      cEntities.Add(pen);
      continue;
    }

    // Reinitialize all spawners
    if (IsOfClassID(pen, CEnemySpawner_ClassID)) {
      cEntities.Add(pen);
      continue;
    }

    // Ignore entities without states
    if (!IsRationalEntity(pen)) continue;

    // Check TFE states of logical entities
    CRationalEntity *penRE = (CRationalEntity *)pen;

    if (CheckEntityState(penRE, 0x00DC000A, CCamera_ClassID)
     || CheckEntityState(penRE, 0x00DC000D, CCamera_ClassID)
     || CheckEntityState(penRE, 0x01300043, CEnemySpawner_ClassID)
     || CheckEntityState(penRE, 0x025F0009, CLightning_ClassID)
     || CheckEntityState(penRE, 0x00650014, CMovingBrush_ClassID)
     || CheckEntityState(penRE, 0x0261002E, CPyramidSpaceShip_ClassID)
     || CheckEntityState(penRE, 0x025E000C, CStormController_ClassID)
     || CheckEntityState(penRE, 0x014C013B, CDevil_ClassID)
     || CheckEntityState(penRE, 0x0140001B, CWoman_ClassID)) {
      cEntities.Add(pen);
      continue;
    }

    // Other TFE enemies
    if (IsDerivedFromID(pen, CEnemyBase_ClassID)) {
      if (penRE->en_stslStateStack.Count() > 0
       && penRE->en_stslStateStack[0] != 0x01360070) {
        cEntities.Add(pen);
        continue;
      }
    }
  }

  // Restore rain
  ApplyRainProperties();

  FOREACHINDYNAMICCONTAINER(cEntities, CEntity, itenReinit) {
    itenReinit->Reinitialize();
  }

  // Fix shadow issues
  CreateGlobalLight();
};

#endif // CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE
