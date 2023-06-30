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

// Convert TFE weapon flags into TSE weapon flags
void IConvertTFE::ConvertWeapon(INDEX &iFlags, INDEX iWeapon) {
  switch (iWeapon) {
    // Laser
    case 14: iFlags |= WeaponFlag(E_WPN_LASER); break;

    // Cannon
    case 16: iFlags |= WeaponFlag(E_WPN_IRONCANNON); break;

    // Nonexistent weapons
    case 10: case 11: case 12: case 13: case 15: case 17: break;

    // Others
    default: iFlags |= WeaponFlag(iWeapon);
  }
};

// Convert TFE keys into TSE keys
void IConvertTFE::ConvertKeyType(EKeyTSE &eKey) {
  switch (eKey) {
    // Dummy keys
    case 4: eKey = E_KEY_JAGUARGOLDDUMMY; break; // Gold ankh
    case 15: eKey = E_KEY_TABLESDUMMY; break; // Metropolis scarab

    // Element keys
    case 5: eKey = E_KEY_CROSSWOODEN; break; // Earth
    case 6: eKey = E_KEY_CROSSMETAL; break; // Water
    case 7: eKey = E_KEY_CRYSTALSKULL; break; // Air
    case 8: eKey = E_KEY_CROSSGOLD; break; // Fire

    // Other keys
    case 0:  eKey = E_KEY_CROSSWOODEN; break;
    case 1:  eKey = E_KEY_CROSSMETAL; break;
    case 2:  eKey = E_KEY_CROSSGOLD; break;
    case 3:  eKey = E_KEY_KINGSTATUE; break;
    case 9:  eKey = E_KEY_HOLYGRAIL; break;
    case 10: eKey = E_KEY_BOOKOFWISDOM; break;
    case 12: eKey = E_KEY_BOOKOFWISDOM; break;
    case 13: eKey = E_KEY_STATUEHEAD03; break; // Metropolis scarab
    case 14: eKey = E_KEY_HOLYGRAIL; break;
    case 16: eKey = E_KEY_STATUEHEAD01; break; // Luxor pair
    case 17: eKey = E_KEY_STATUEHEAD02; break; // Luxor pair
    case 18: eKey = E_KEY_WINGEDLION; break; // Sacred Yards sphinx
    case 19: eKey = E_KEY_ELEPHANTGOLD; break; // Sacred Yards sphinx

    // Edge case
    default: eKey = E_KEY_KINGSTATUE; break;
  }
};

// Convert one TFE entity into TSE
BOOL IConvertTFE::ConvertEntity(CEntity *pen) {
  enum {
    EN_DOOR, EN_KEY, EN_START, EN_PACK, EN_STORM,
  } eEntity;

  // Check for the entities that need to be patched and remember their type
  if (IsOfClass(pen, "DoorController")) {
    eEntity = EN_DOOR;
  } else if (IsOfClass(pen, "KeyItem")) {
    eEntity = EN_KEY;
  } else if (IsOfClass(pen, "Player Marker")) {
    eEntity = EN_START;
  } else if (IsOfClass(pen, "Ammo Pack")) {
    eEntity = EN_PACK;
  } else if (IsOfClass(pen, "Storm controller")) {
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

    if (pptrNapalm.ByNameOrId(CEntityProperty::EPT_INDEX, "Napalm", (0x326 << 8) + 14)) {
      ENTITYPROPERTY(pen, pptrNapalm.Offset(), INDEX) = 0;
    }

    if (pptrSniper.ByNameOrId(CEntityProperty::EPT_INDEX, "Sniper bullets", (0x326 << 8) + 17)) {
      ENTITYPROPERTY(pen, pptrSniper.Offset(), INDEX) = 0;
    }

  // Adjust weapon masks
  } else if (eEntity == EN_START) {
    // Retrieve CPlayerMarker::m_iGiveWeapons and CPlayerMarker::m_iTakeWeapons
    static CPropertyPtr pptrGive(pen);
    static CPropertyPtr pptrTake(pen);

    // Properties don't exist
    if (!pptrGive.ByNameOrId(CEntityProperty::EPT_INDEX, "Give Weapons", (0x194 << 8) + 3)) {
      return FALSE;
    }

    if (!pptrTake.ByNameOrId(CEntityProperty::EPT_INDEX, "Take Weapons", (0x194 << 8) + 4)) {
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
    if (pptrType.ByNameOrId(CEntityProperty::EPT_ENUM, "Type", (0x325 << 8) + 1)) {
      ConvertKeyType(ENTITYPROPERTY(pen, pptrType.Offset(), EKeyTSE));
    }

    // Fix sound component index (301 -> 300)
    if (pptrSound.ByIdOrOffset(CEntityProperty::EPT_INDEX, (0x325 << 8) + 3, 0x3B0)) {
      ENTITYPROPERTY(pen, pptrSound.Offset(), INDEX) = (0x325 << 8) + 300;
    }

  // Adjust keys
  } else if (eEntity == EN_DOOR) {
    // Retrieve CDoorController::m_dtType and CDoorController::m_kitKey
    static CPropertyPtr pptrType(pen);
    static CPropertyPtr pptrKey(pen);

    // Property doesn't exist
    if (!pptrType.ByNameOrId(CEntityProperty::EPT_ENUM, "Type", (0xDD << 8) + 8)) {
      return FALSE;
    }

    // Only for locked doors (DT_LOCKED)
    if (ENTITYPROPERTY(pen, pptrType.Offset(), INDEX) != 2) {
      return FALSE;
    }

    // Convert key type
    if (pptrKey.ByNameOrId(CEntityProperty::EPT_ENUM, "Key", (0xDD << 8) + 12)) {
      ConvertKeyType(ENTITYPROPERTY(pen, pptrKey.Offset(), EKeyTSE));
    }

  // Adjust storm shade color
  } else if (eEntity == EN_STORM) {
    // Retrieve CStormController::m_colShadeStart and CStormController::m_colShadeStop
    static CPropertyPtr pptrShadeStart(pen);
    static CPropertyPtr pptrShadeStop(pen);

    // Properties don't exist
    if (!pptrShadeStart.ByNameOrId(CEntityProperty::EPT_COLOR, "Color shade start", (0x25E << 8) + 52)) {
      return FALSE;
    }

    if (!pptrShadeStop.ByNameOrId(CEntityProperty::EPT_COLOR, "Color shade stop", (0x25E << 8) + 53)) {
      return FALSE;
    }

    // Matches gray scale in the 64-255 brightness range
    ENTITYPROPERTY(pen, pptrShadeStart.Offset(), COLOR) = C_WHITE | CT_OPAQUE;
    ENTITYPROPERTY(pen, pptrShadeStop.Offset(), COLOR) = C_dGRAY | CT_OPAQUE;

    // Proceed with reinitialization
    return FALSE;
  }

  return TRUE;
};

// Make entire world TSE-compatible
void IConvertTFE::ConvertWorld(CWorld *pwo) {
  CEntities cEntities;

  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    CEntity *pen = iten;

    // Convert specific entities regardless of state first
    if (ConvertEntity(pen)) continue;

    // Reinitialize all spawners
    if (IsOfClass(pen, "Enemy Spawner")) {
      cEntities.Add(pen);
      continue;
    }

    // Ignore entities without states
    if (!IWorld::IsRationalEntity(pen)) continue;

    // Check TFE states of logical entities
    CRationalEntity *penRE = (CRationalEntity *)pen;

    if (CheckEntityState(penRE, 0x00DC000A, "Camera")
     || CheckEntityState(penRE, 0x00DC000D, "Camera")
     || CheckEntityState(penRE, 0x01300043, "Enemy Spawner")
     || CheckEntityState(penRE, 0x025F0009, "Lightning")
     || CheckEntityState(penRE, 0x00650014, "Moving Brush")
     || CheckEntityState(penRE, 0x0261002E, "PyramidSpaceShip")
     || CheckEntityState(penRE, 0x025E000C, "Storm controller")
     || CheckEntityState(penRE, 0x014C013B, "Devil")
     || CheckEntityState(penRE, 0x0140001B, "Woman")) {
      cEntities.Add(pen);
      continue;
    }

    // Other TFE enemies
    if (IsDerivedFromClass(pen, "Enemy Base")) {
      if (penRE->en_stslStateStack.Count() > 0
       && penRE->en_stslStateStack[0] != 0x01360070) {
        cEntities.Add(pen);
        continue;
      }
    }
  }

  FOREACHINDYNAMICCONTAINER(cEntities, CEntity, itenReinit) {
    itenReinit->Reinitialize();
  }

  // Create an invisible light to fix shadow issues with brush polygon layers
  const CPlacement3D pl(FLOAT3D(0.0f, 0.0f, 0.0f), ANGLE3D(0.0f, 0.0f, 0.0f));

  try {
    static const CTString strLightClass = "Classes\\Light.ecl";
    CEntity *penLight = IWorld::GetWorld()->CreateEntity_t(pl, strLightClass);

    // Retrieve light properties
    static CPropertyPtr pptrType(penLight); // CLight::m_ltType
    static CPropertyPtr pptrFallOff(penLight); // CLight::m_rFallOffRange
    static CPropertyPtr pptrColor(penLight); // CLight::m_colColor

    // Set strong ambient type that covers the whole map
    if (pptrType.ByNameOrId(CEntityProperty::EPT_ENUM, "Type", (0xC8 << 8) + 8)
     && pptrFallOff.ByNameOrId(CEntityProperty::EPT_RANGE, "Fall-off", (0xC8 << 8) + 1)
     && pptrColor.ByNameOrId(CEntityProperty::EPT_COLOR, "Color", (0xC8 << 8) + 2))
    {
      ENTITYPROPERTY(penLight, pptrType.Offset(), INDEX) = 2; // LightType::LT_STRONG_AMBIENT
      ENTITYPROPERTY(penLight, pptrFallOff.Offset(), RANGE) = 10000.0f;
      ENTITYPROPERTY(penLight, pptrColor.Offset(), COLOR) = 0; // Black
    }

    penLight->Initialize();
    penLight->GetLightSource()->FindShadowLayers(FALSE);

  } catch (char *strError) {
    FatalError(TRANS("Cannot load Light class:\n%s"), strError);
  }
};

#endif // CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE
