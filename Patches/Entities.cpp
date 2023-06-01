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

// Original function pointers
void (CEntity::*pSendEvent)(const CEntityEvent &) = NULL;
CEntityPatch::CReceiveItem pReceiveItem = NULL;

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
