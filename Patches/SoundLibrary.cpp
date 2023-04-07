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

#if CLASSICSPATCH_ENGINEPATCHES

#include "SoundLibrary.h"

void CSoundLibPatch::P_Listen(CSoundListener &sl)
{
  // Ignore sound listener
  if (_EnginePatches._bNoListening) {
    return;
  }

  // Original function code
  if (sl.sli_lnInActiveListeners.IsLinked()) {
    sl.sli_lnInActiveListeners.Remove();
  }

  sl_lhActiveListeners.AddTail(sl.sli_lnInActiveListeners);
};

#endif // CLASSICSPATCH_ENGINEPATCHES
