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

#ifndef CECIL_INCL_CONVERTERS_COMMON_H
#define CECIL_INCL_CONVERTERS_COMMON_H

#ifdef PRAGMA_ONCE
  #pragma once
#endif

#if CLASSICSPATCH_CONVERT_MAPS

// TFE data
namespace IMapsTFE {
  // PlayerWeapons types
  enum EWeapon {
    WEAPON_NONE = 0,
    WEAPON_KNIFE = 1,
    WEAPON_COLT = 2,
    WEAPON_DOUBLECOLT = 3,
    WEAPON_SINGLESHOTGUN = 4,
    WEAPON_DOUBLESHOTGUN = 5,
    WEAPON_TOMMYGUN = 6,
    WEAPON_MINIGUN = 7,
    WEAPON_ROCKETLAUNCHER = 8,
    WEAPON_GRENADELAUNCHER = 9,
    WEAPON_LASER = 14,
    WEAPON_IRONCANNON = 16,
    WEAPON_LAST = 17,
  };

  // KeyItem types
  enum EKey {
    KIT_ANKHWOOD = 0,
    KIT_ANKHROCK = 1,
    KIT_ANKHGOLD = 2,
    KIT_AMONGOLD = 3,
    KIT_ANKHGOLDDUMMY = 4,
    KIT_ELEMENTEARTH = 5,
    KIT_ELEMENTWATER = 6,
    KIT_ELEMENTAIR = 7,
    KIT_ELEMENTFIRE = 8,
    KIT_RAKEY = 9,
    KIT_MOONKEY = 10,
    KIT_EYEOFRA = 12,
    KIT_SCARAB = 13,
    KIT_COBRA = 14,
    KIT_SCARABDUMMY = 15,
    KIT_HEART = 16,
    KIT_FEATHER = 17,
    KIT_SPHINX1 = 18,
    KIT_SPHINX2 = 19,
  };
};

// TSE data
namespace IMapsTSE {
  // PlayerWeapons types
  enum EWeapon {
    WEAPON_NONE = 0,
    WEAPON_KNIFE = 1,
    WEAPON_COLT = 2,
    WEAPON_DOUBLECOLT = 3,
    WEAPON_SINGLESHOTGUN = 4,
    WEAPON_DOUBLESHOTGUN = 5,
    WEAPON_TOMMYGUN = 6,
    WEAPON_MINIGUN = 7,
    WEAPON_ROCKETLAUNCHER = 8,
    WEAPON_GRENADELAUNCHER = 9,
    WEAPON_CHAINSAW = 10,
    WEAPON_FLAMER = 11,
    WEAPON_LASER = 12,
    WEAPON_SNIPER = 13,
    WEAPON_IRONCANNON = 14,
    WEAPON_LAST = 15,
  };

  // KeyItem types
  enum EKey {
    KIT_BOOKOFWISDOM = 0,
    KIT_CROSSWOODEN = 1,
    KIT_CROSSMETAL = 2,
    KIT_CROSSGOLD = 3,
    KIT_JAGUARGOLDDUMMY = 4,
    KIT_HAWKWINGS01DUMMY = 5,
    KIT_HAWKWINGS02DUMMY = 6,
    KIT_HOLYGRAIL = 7,
    KIT_TABLESDUMMY = 8,
    KIT_WINGEDLION = 9,
    KIT_ELEPHANTGOLD = 10,
    KIT_STATUEHEAD01 = 11,
    KIT_STATUEHEAD02 = 12,
    KIT_STATUEHEAD03 = 13,
    KIT_KINGSTATUE = 14,
    KIT_CRYSTALSKULL = 15,
  };
};

// SSR data
namespace IMapsSSR {
  // PlayerWeapons types
  enum EWeapon {
    WEAPON_NONE = 0,
    WEAPON_KNIFE = 1,
    WEAPON_COLT = 2,
    WEAPON_DOUBLECOLT = 3,
    WEAPON_SINGLESHOTGUN = 4,
    WEAPON_DOUBLESHOTGUN = 5,
    WEAPON_TOMMYGUN = 6,
    WEAPON_MINIGUN = 7,
    WEAPON_ROCKETLAUNCHER = 8,
    WEAPON_GRENADELAUNCHER = 9,
    WEAPON_CHAINSAW = 10,
    WEAPON_FLAMER = 11,
    WEAPON_LASER = 12,
    WEAPON_SNIPER = 13,
    WEAPON_IRONCANNON = 14,
    WEAPON_GHOSTBUSTER = 15,
    WEAPON_PLASMATHROWER = 16,
    WEAPON_MINELAYER = 17,
    WEAPON_LAST = 18,
  };

  // KeyItem types
  enum EKey {
    KIT_ANKHWOOD = 0,
    KIT_ANKHROCK = 1,
    KIT_ANKHGOLD = 2,
    KIT_AMONGOLD = 3,
    KIT_ANKHGOLDDUMMY = 4,
    KIT_ELEMENTEARTH = 5,
    KIT_ELEMENTWATER = 6,
    KIT_ELEMENTAIR = 7,
    KIT_ELEMENTFIRE = 8,
    KIT_RAKEY = 9,
    KIT_MOONKEY = 10,
    KIT_EYEOFRA = 11,
    KIT_SCARAB = 12,
    KIT_COBRA = 13,
    KIT_SCARABDUMMY = 14,
    KIT_HEART = 15,
    KIT_FEATHER = 16,
    KIT_SPHINX1 = 17,
    KIT_SPHINX2 = 18,
    KIT_BOOKOFWISDOM = 19,
    KIT_CROSSWOODEN = 20,
    KIT_CROSSMETAL = 21,
    KIT_CROSSGOLD = 22,
    KIT_JAGUARGOLDDUMMY = 23,
    KIT_HAWKWINGS01DUMMY = 24,
    KIT_HAWKWINGS02DUMMY = 25,
    KIT_HOLYGRAIL = 26,
    KIT_TABLESDUMMY = 27,
    KIT_WINGEDLION = 28,
    KIT_ELEPHANTGOLD = 29,
    KIT_STATUEHEAD01 = 30,
    KIT_STATUEHEAD02 = 31,
    KIT_STATUEHEAD03 = 32,
    KIT_KINGSTATUE = 33,
    KIT_CRYSTALSKULL = 34,
  };
};

#endif // CLASSICSPATCH_CONVERT_MAPS

#endif
