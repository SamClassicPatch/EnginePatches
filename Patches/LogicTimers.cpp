/* Copyright (c) 2022-2024 Dreamy Cecil
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

#if CLASSICSPATCH_FIX_LOGICTIMERS && CLASSICSPATCH_ENGINEPATCHES

#include "LogicTimers.h"

// Set next timer event to occur after some time
void CRationalEntityTimerPatch::P_SetTimerAfter(TIME tmDelta) {
  // [Cecil] Fix timers as a gameplay extension
  if (CoreGEX().bFixTimers) {
    // Minus 4/5 of a tick for TIME_EPSILON == 0.0001 (as if TIME_EPSILON became 0.0401)
    tmDelta -= 0.04f;
  }

  // Set to execute some time after of the current tick
  SetTimerAt(_pTimer->CurrentTick() + tmDelta);
};

#endif // CLASSICSPATCH_FIX_LOGICTIMERS
