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
    case E_LF_SSR: _pconvCurrent = &_convSSR; break;
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

  if (_pconvCurrent != NULL) {
    _pconvCurrent->HandleProperty(pen, prop);
  }
};

// Check if the entity state doesn't match
BOOL IMapConverter::CheckEntityState(CRationalEntity *pen, SLONG slState, INDEX iClassID) {
  // Wrong entity class
  if (!IsOfClassID(pen, iClassID)) return FALSE;

  // No states at all, doesn't matter
  if (pen->en_stslStateStack.Count() <= 0) return FALSE;

  return pen->en_stslStateStack[0] != slState;
};

// Create a global light entity to fix shadow issues with brush polygon layers
void IMapConverter::CreateGlobalLight(void) {
#if SE1_VER >= SE1_107
  // Create an invisible light that covers the whole map
  try {
    static const CTString strLightClass = "Classes\\Light.ecl";
    CEntity *penLight = IWorld::GetWorld()->CreateEntity_t(IDummy::plCenter, strLightClass);

    // Retrieve light properties
    static CPropertyPtr pptrType(penLight); // CLight::m_ltType
    static CPropertyPtr pptrFallOff(penLight); // CLight::m_rFallOffRange
    static CPropertyPtr pptrColor(penLight); // CLight::m_colColor

    if (pptrType   .ByVariable("CLight", "m_ltType")
     && pptrFallOff.ByVariable("CLight", "m_rFallOffRange")
     && pptrColor  .ByVariable("CLight", "m_colColor"))
    {
      ENTITYPROPERTY(penLight, pptrType.Offset(), INDEX) = 2; // LightType::LT_STRONG_AMBIENT
      ENTITYPROPERTY(penLight, pptrFallOff.Offset(), RANGE) = 10000.0f;
      ENTITYPROPERTY(penLight, pptrColor.Offset(), COLOR) = 0; // Black
    }

    penLight->Initialize();
    penLight->GetLightSource()->FindShadowLayers(FALSE);

  } catch (char *strError) {
    FatalError(TRANS("Cannot load %s class:\n%s"), "Light", strError);
  }
#endif
};

#endif // CLASSICSPATCH_CONVERT_MAPS
