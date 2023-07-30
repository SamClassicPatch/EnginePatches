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

// Currently used converter
static IMapConverter *_pconvCurrent = NULL;

// Get map converter for a specific format
IMapConverter *IMapConverter::SetConverter(ELevelFormat eFormat)
{
  switch (eFormat) {
  #if TSE_FUSION_MODE
    case E_LF_TFE: _pconvCurrent = &_convTFE; break;
  #endif
    default: _pconvCurrent = NULL;
  }

  ASSERTMSG(_pconvCurrent != NULL, "No converter available for the desired level format!");
  return _pconvCurrent;
};

// Handle unknown entity property upon reading it via CEntity::ReadProperties_t()
void IMapConverter::HandleUnknownProperty(CEntity *pen, ULONG ulType, ULONG ulID, void *pValue)
{
  UnknownProp prop(ulType, ulID, pValue);
  _pconvCurrent->HandleProperty(pen, prop);
};

// Check if the entity state doesn't match
BOOL IMapConverter::CheckEntityState(CRationalEntity *pen, SLONG slState, INDEX iClassID) {
  // Wrong entity class
  if (!IsOfClassID(pen, iClassID)) return FALSE;

  // No states at all, doesn't matter
  if (pen->en_stslStateStack.Count() <= 0) return FALSE;

  return pen->en_stslStateStack[0] != slState;
};

#endif // CLASSICSPATCH_CONVERT_MAPS
