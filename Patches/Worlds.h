/* Copyright (c) 2023-2024 Dreamy Cecil
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

#ifndef CECIL_INCL_PATCHES_WORLDS_H
#define CECIL_INCL_PATCHES_WORLDS_H

#ifdef PRAGMA_ONCE
  #pragma once
#endif

#if CLASSICSPATCH_ENGINEPATCHES

class CWorldPatch : public CWorld {
  public:
    void P_Load(const CTFileName &fnmWorld);

    // Read world information
    void P_ReadInfo(CTStream *strm, BOOL bMaybeDescription);

    // Create a new entity of a given class
    CEntity *P_CreateEntity(const CPlacement3D &plPlacement, const CTFileName &fnmClass);
};

#endif // CLASSICSPATCH_ENGINEPATCHES

#endif
