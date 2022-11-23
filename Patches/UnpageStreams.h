/* Copyright (c) 2022 Dreamy Cecil
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

#include <Engine/Network/LevelChange.h>

// CTStream patches
class CStreamPatch : public CTStream {
  public:
    // Allocate memory normally
    void P_AllocVirtualMemory(ULONG ulBytesToAllocate);

    // Free memory normally
    void P_FreeBuffer(void);
};

// CTFileStream patches
class CFileStreamPatch : public CStreamPatch {
  // Copies of private fields
  public:
    FILE *fstrm_pFile;
    INDEX fstrm_iZipHandle;
    INDEX fstrm_iLastAccessedPage;
    BOOL fstrm_bReadOnly;

  public:
    // Create a new file
    void P_Create(const CTFileName &fnFileName, CTStream::CreateMode cm);

    // Open a file
    void P_Open(const CTFileName &fnFileName, CTStream::OpenMode om);

    // Close opened file
    void P_Close(void);
};

// CRememberedLevel clone that saves session state into itself
class CRemLevel : public CStreamPatch {
  public:
    CListNode rl_lnInSessionState; // Node in the remembered levels list
    CTString rl_strFileName; // World filename

  // CTMemoryStream method replacements
  public:
    // Constructor
    CRemLevel(void);

    // Destructor
    ~CRemLevel(void);

    // Always interactable
    BOOL IsReadable(void)  { return TRUE; };
    BOOL IsWriteable(void) { return TRUE; };
    BOOL IsSeekable(void)  { return TRUE; };

    // Dummy
    void HandleAccess(INDEX, BOOL) {};
};

// Remembered levels without CTMemoryStream
class CRemLevelPatch : public CSessionState {
  public:
    // Remember current level by its filename
    void P_RememberCurrentLevel(const CTString &strFileName);

    // Fine remembered level by its filename
    CRemLevel *P_FindRememberedLevel(const CTString &strFileName);

    // Restore old level by its filename
    void P_RestoreOldLevel(const CTString &strFileName);

    // Forget all remembered levels
    void P_ForgetOldLevels(void);
};
