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

#include "Worlds.h"

#include <CoreLib/Base/Unzip.h>

void CWorldPatch::P_Load(const CTFileName &fnmWorld) {
  // Open the file
  wo_fnmFileName = fnmWorld;

  CTFileStream strmFile;
  strmFile.Open_t(fnmWorld);

  // Check engine build
  BOOL bNeedsReinit;
  _pNetwork->CheckVersion_t(strmFile, TRUE, bNeedsReinit);

  // Read the world
  Read_t(&strmFile);

  strmFile.Close();

  // Reinitialize and resave old levels, if needed
  if (bNeedsReinit) {
    SetProgressDescription(LOCALIZE("converting from old version"));
    CallProgressHook_t(0.0f);
    ReinitializeEntities();
    CallProgressHook_t(1.0f);

    SetProgressDescription(LOCALIZE("saving converted file"));
    CallProgressHook_t(0.0f);
    Save_t(fnmWorld);
    CallProgressHook_t(1.0f);
  }
};
