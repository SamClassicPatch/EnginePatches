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

#ifndef CECIL_INCL_TFEMAPS_H
#define CECIL_INCL_TFEMAPS_H

#ifdef PRAGMA_ONCE
  #pragma once
#endif

#if CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE

// Interface for converting worlds from The First Encounter
class PATCHES_API IConvertTFE {
  public:
    // Types from TSE's PlayerWeapons
    enum EWeaponTSE {
      E_WPN_NONE = 0,
      E_WPN_KNIFE = 1,
      E_WPN_COLT = 2,
      E_WPN_DOUBLECOLT = 3,
      E_WPN_SINGLESHOTGUN = 4,
      E_WPN_DOUBLESHOTGUN = 5,
      E_WPN_TOMMYGUN = 6,
      E_WPN_MINIGUN = 7,
      E_WPN_ROCKETLAUNCHER = 8,
      E_WPN_GRENADELAUNCHER = 9,
      E_WPN_CHAINSAW = 10,
      E_WPN_FLAMER = 11,
      E_WPN_LASER = 12,
      E_WPN_SNIPER = 13,
      E_WPN_IRONCANNON = 14,
      E_WPN_LAST = 15,
    };

    // Types from TSE's KeyItem
    enum EKeyTSE {
      E_KEY_BOOKOFWISDOM = 0,
      E_KEY_CROSSWOODEN = 1,
      E_KEY_CROSSMETAL = 2,
      E_KEY_CROSSGOLD = 3,
      E_KEY_JAGUARGOLDDUMMY = 4,
      E_KEY_HAWKWINGS01DUMMY = 5,
      E_KEY_HAWKWINGS02DUMMY = 6,
      E_KEY_HOLYGRAIL = 7,
      E_KEY_TABLESDUMMY = 8,
      E_KEY_WINGEDLION = 9,
      E_KEY_ELEPHANTGOLD = 10,
      E_KEY_STATUEHEAD01 = 11,
      E_KEY_STATUEHEAD02 = 12,
      E_KEY_STATUEHEAD03 = 13,
      E_KEY_KINGSTATUE = 14,
      E_KEY_CRYSTALSKULL = 15,
    };

  public:
    // Reset the converter before loading a new world
    static void Reset(void);

    // Handle some unknown property
    static void HandleProperty(CEntity *pen, const IMapConverters::UnknownProp &prop);

    // Convert TFE weapon flags into TSE weapon flags
    static void ConvertWeapon(INDEX &iFlags, INDEX iWeapon);

    // Convert TFE keys to TSE keys
    static void ConvertKeyType(EKeyTSE &eKey);

    // Convert one TFE entity into TSE
    static BOOL ConvertEntity(CEntity *pen);

    // Make entire world TSE-compatible
    static void ConvertWorld(CWorld *pwo);
};

// Rain handling methods
namespace IRainTFE {

// Clear rain variables
void ClearRainVariables(void);

// Remember rain properties of CWorldSettingsController
void RememberWSC(CEntity *penWSC, const IMapConverters::UnknownProp &prop);

// Apply remembered rain properties from controllers
void ApplyRainProperties(void);

}; // namespace

#endif // CLASSICSPATCH_CONVERT_MAPS && TSE_FUSION_MODE

#endif
