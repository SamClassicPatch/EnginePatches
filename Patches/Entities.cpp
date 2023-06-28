/* Copyright (c) 2022-2023 Dreamy Cecil
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

#if CLASSICSPATCH_EXTEND_ENTITIES && CLASSICSPATCH_ENGINEPATCHES

#include "Entities.h"

#include <CoreLib/Interfaces/ResourceFunctions.h>

// Original function pointers
void (CEntity::*pSendEvent)(const CEntityEvent &) = NULL;
CEntityPatch::CReceiveItem pReceiveItem = NULL;

// Read entity property values
void CEntityPatch::P_ReadProperties(CTStream &istrm) {
  #define GET_PROP(_Type) ENTITYPROPERTY(this, pepProp->ep_slOffset, _Type)
  #define READ_PROP(_Type) istrm.Read_t(&GET_PROP(_Type), sizeof(_Type))
  #define SKIP_PROP(_Type) { _Type valSkip; istrm.Read_t(&valSkip, sizeof(_Type)); }

  istrm.ExpectID_t("PRPS"); // Properties

  CDLLEntityClass *pdecDLLClass = en_pecClass->ec_pdecDLLClass;

  // Read number of saved properties
  INDEX ctProperties;
  istrm >> ctProperties;

  while (--ctProperties >= 0) {
    // Packed identifier
    ULONG ulID;
    istrm >> ulID;

    // Unpack property type and property ID
    const CEntityProperty::PropertyType eptType = (CEntityProperty::PropertyType)(ulID & 0xFF);
    ulID >>= 8;

    // Find property for that type and ID
    CEntityProperty *pepProp = PropertyForTypeAndID(eptType, ulID);

    // Try searching for a translatable string, if a normal string is not found
    if (pepProp == NULL && eptType == CEntityProperty::EPT_STRING) {
      pepProp = PropertyForTypeAndID(CEntityProperty::EPT_STRINGTRANS, ulID);
    }

    // Skip non-existing properties
    if (pepProp == NULL)
    {
      switch (eptType)
      {
        case CEntityProperty::EPT_ENUM: case CEntityProperty::EPT_BOOL:
        case CEntityProperty::EPT_COLOR: case CEntityProperty::EPT_FLAGS:
        case CEntityProperty::EPT_INDEX: case CEntityProperty::EPT_FLOAT:
        case CEntityProperty::EPT_RANGE: case CEntityProperty::EPT_ANGLE:
        case CEntityProperty::EPT_ANIMATION: case CEntityProperty::EPT_ILLUMINATIONTYPE:
        case CEntityProperty::EPT_ENTITYPTR: {
          INDEX iDummy;
          istrm >> iDummy;
        } break;

        case CEntityProperty::EPT_STRINGTRANS:
          // [Cecil] "DTRS" in the DLL as is gets picked up by the Depend utility
          istrm.ExpectID_t((CTString("DT") + "RS").str_String);
        case CEntityProperty::EPT_FILENAMENODEP:
        case CEntityProperty::EPT_STRING: {
          CTString strDummy;
          istrm >> strDummy;
        } break;

        case CEntityProperty::EPT_FILENAME: {
          CTFileName fnmDummy;
          istrm >> fnmDummy;
        } break;

        case CEntityProperty::EPT_MODELOBJECT: {
          IRes::Models::Skip_t(istrm);
        } break;

        case CEntityProperty::EPT_MODELINSTANCE: {
          IRes::SKA::Skip_t(istrm);
        } break;

        case CEntityProperty::EPT_ANIMOBJECT: {
          IRes::Anims::Skip_t(istrm);
        } break;

        case CEntityProperty::EPT_SOUNDOBJECT: {
          CSoundObject soDummy;
          soDummy.Read_t(&istrm);
        } break;

        case CEntityProperty::EPT_FLOAT3D:       SKIP_PROP(FLOAT3D); break;
        case CEntityProperty::EPT_ANGLE3D:       SKIP_PROP(ANGLE3D); break;
        case CEntityProperty::EPT_PLACEMENT3D:   SKIP_PROP(CPlacement3D); break;
        case CEntityProperty::EPT_FLOATAABBOX3D: SKIP_PROP(FLOATaabbox3D); break;
        case CEntityProperty::EPT_FLOATplane3D:  SKIP_PROP(FLOATplane3D); break;
        case CEntityProperty::EPT_FLOATQUAT3D:   SKIP_PROP(FLOATquat3D); break;
        case CEntityProperty::EPT_FLOATMATRIX3D: SKIP_PROP(FLOATmatrix3D); break;

        default: ASSERTALWAYS("Unknown property type");
      }

      // Continue to the next property
      continue;
    }

    CEntityProperty::PropertyType eptLoad = pepProp->ep_eptType;

    // Need to load a translatable string but a normal one has been written
    if (eptLoad == CEntityProperty::EPT_STRINGTRANS
     && eptType == CEntityProperty::EPT_STRING) {
      eptLoad = CEntityProperty::EPT_STRING;
    }

    // depending on the property type
    switch (eptLoad)
    {
      // 32-bit long numerical values
      case CEntityProperty::EPT_ENUM: case CEntityProperty::EPT_BOOL:
      case CEntityProperty::EPT_COLOR: case CEntityProperty::EPT_FLAGS:
      case CEntityProperty::EPT_INDEX: case CEntityProperty::EPT_FLOAT:
      case CEntityProperty::EPT_RANGE: case CEntityProperty::EPT_ANGLE:
      case CEntityProperty::EPT_ANIMATION: case CEntityProperty::EPT_ILLUMINATIONTYPE: {
        istrm >> GET_PROP(INDEX);
      } break;

      // Entity pointer
      case CEntityProperty::EPT_ENTITYPTR: {
        ReadEntityPointer_t(&istrm, GET_PROP(CEntityPointer));
      } break;

      // Translatable and normal strings
      case CEntityProperty::EPT_STRINGTRANS:
        // [Cecil] "DTRS" in the DLL as is gets picked up by the Depend utility
        istrm.ExpectID_t((CTString("DT") + "RS").str_String);
      case CEntityProperty::EPT_FILENAMENODEP:
      case CEntityProperty::EPT_STRING: {
        istrm >> GET_PROP(CTString);
      } break;

      // Resources
      case CEntityProperty::EPT_FILENAME: {
        CTFileName &fnm = GET_PROP(CTFileName);
        istrm >> fnm;

        if (fnm == "") break;

        // Try to replace the file if it doesn't exist
        while (TRUE) {
          // Found existing file
          if (FileExists(fnm)) break;

          // Ask for a replacement file
          CTFileName fnmReplacement;

          if (IRes::GetReplacingFile(fnm, fnmReplacement, FILTER_ALL FILTER_END)) {
            fnm = fnmReplacement;

          } else {
            ThrowF_t(LOCALIZE("File '%s' does not exist"), fnm.str_String);
          }
        }
      } break;

      case CEntityProperty::EPT_MODELOBJECT: {
        IRes::Models::Read_t(istrm, GET_PROP(CModelObject));
      } break;

      case CEntityProperty::EPT_MODELINSTANCE: {
        IRes::SKA::Read_t(istrm, GET_PROP(CModelInstance));
      } break;

      case CEntityProperty::EPT_ANIMOBJECT: {
        IRes::Anims::Read_t(istrm, GET_PROP(CAnimObject));
      } break;

      case CEntityProperty::EPT_SOUNDOBJECT: {
        CSoundObject &so = GET_PROP(CSoundObject);
        so.Read_t(&istrm);
        so.so_penEntity = this;
      } break;

      // 3D environment
      case CEntityProperty::EPT_FLOAT3D:       READ_PROP(FLOAT3D); break;
      case CEntityProperty::EPT_ANGLE3D:       READ_PROP(ANGLE3D); break;
      case CEntityProperty::EPT_PLACEMENT3D:   READ_PROP(CPlacement3D); break;
      case CEntityProperty::EPT_FLOATAABBOX3D: READ_PROP(FLOATaabbox3D); break;
      case CEntityProperty::EPT_FLOATplane3D:  READ_PROP(FLOATplane3D); break;
      case CEntityProperty::EPT_FLOATQUAT3D:   READ_PROP(FLOATquat3D); break;
      case CEntityProperty::EPT_FLOATMATRIX3D: READ_PROP(FLOATmatrix3D); break;

      default: ASSERTALWAYS("Unknown property type");
    }
  }
};

// Send an event to this entity
void CEntityPatch::P_SendEvent(const CEntityEvent &ee)
{
  // Call event sending function for each plugin
  FOREACHPLUGINHANDLER(GetPluginAPI()->cListenerEvents, IListenerEvents, pEvents) {
    if ((IAbstractEvents *)pEvents == NULL) continue;

    pEvents->OnSendEvent(this, ee);
  }

  // Proceed to the original function
  (this->*pSendEvent)(ee);
};

// Receive item by a player entity
BOOL CEntityPatch::P_ReceiveItem(const CEntityEvent &ee)
{
  // Proceed to the original function
  BOOL bResult = (this->*pReceiveItem)(ee);

  // Call receive item function for each plugin
  FOREACHPLUGINHANDLER(GetPluginAPI()->cListenerEvents, IListenerEvents, pEvents) {
    if ((IAbstractEvents *)pEvents == NULL) continue;

    pEvents->OnReceiveItem(this, ee, bResult);
  }

  return bResult;
};

// Call a subautomaton
void CRationalEntityPatch::P_Call(SLONG slThisState, SLONG slTargetState, BOOL bOverride, const CEntityEvent &eeInput)
{
  // Call event functions for each plugin
  FOREACHPLUGINHANDLER(GetPluginAPI()->cListenerEvents, IListenerEvents, pEvents) {
    if ((IAbstractEvents *)pEvents == NULL) continue;

    pEvents->OnCallProcedure(this, eeInput);
  }

  // Original function code
  UnwindStack(slThisState);

  if (bOverride) {
    slTargetState = en_pecClass->ec_pdecDLLClass->GetOverridenState(slTargetState);
  }

  en_stslStateStack.Push() = slTargetState;
  HandleEvent(eeInput);
};

#endif // CLASSICSPATCH_EXTEND_ENTITIES
