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

#if CLASSICSPATCH_CONVERT_MAPS

namespace IMapConverters {

// Reset map converters before using them
void Reset(void)
{
  #if TSE_FUSION_MODE
    if (_EnginePatches._bFirstEncounter) {
      IConvertTFE::Reset();
    }
  #endif
};

// Handle unknown entity property upon reading it via CEntity::ReadProperties_t()
void HandleProperty(CEntity *pen, ULONG ulType, ULONG ulID, void *pValue) {
  UnknownProp prop(ulType, ulID, pValue);

  #if TSE_FUSION_MODE
    if (_EnginePatches._bFirstEncounter) {
      IConvertTFE::HandleProperty(pen, prop);
    }
  #endif
};

}; // namespace

// Check if the entity state doesn't match
BOOL CheckEntityState(CRationalEntity *pen, SLONG slState, const char *strClass) {
  // Wrong entity class
  if (!IsOfClass(pen, strClass)) return FALSE;

  // No states at all, doesn't matter
  if (pen->en_stslStateStack.Count() <= 0) return FALSE;

  return pen->en_stslStateStack[0] != slState;
};

#endif // CLASSICSPATCH_CONVERT_MAPS
