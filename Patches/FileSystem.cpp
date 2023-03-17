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

#include "FileSystem.h"

// Original function pointer
extern void (*pInitStreams)(void) = NULL;

// Load ZIP packages under an absolute directory that match a filename mask
static void LoadPackages(const CTString &strDirectory, const CTString &strMatchFiles) {
  _finddata_t fdFile;

  // Find first file in the directory
  long hFile = _findfirst(strDirectory + strMatchFiles, &fdFile);
  BOOL bOK = (hFile != -1);

  while (bOK) {
    // Add file to the active set if the name matches the mask
    if (CTString(fdFile.name).Matches(strMatchFiles)) {
      IUnzip::AddArchive(strDirectory + fdFile.name);
    }

    bOK = (_findnext(hFile, &fdFile) == 0);
  }

  _findclose(hFile);
};

// Initialize various file paths and load game content
void P_InitStreams(void) {
  #if TSE_FUSION_MODE
    // If no CD path has been set
    if (_fnmCDPath == "") {
      // Go outside the game directory
      _fnmCDPath = _fnmApplicationPath + "..\\";
      IFiles::SetAbsolutePath(_fnmCDPath);

      // Enter TFE directory nearby
      _fnmCDPath += "Serious Sam Classic The First Encounter\\";

      // Reset if the directory doesn't exist
      DWORD dwAttrib = GetFileAttributesA(_fnmCDPath.str_String);

      if (dwAttrib == -1 || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        _fnmCDPath = CTString("");
      }
    }
  #endif

  // Proceed to the original function
  pInitStreams();
};

// Make a list of all files in a directory
void P_MakeDirList(CFileList &afnmDir, const CTFileName &fnmDir, const CTString &strPattern, ULONG ulFlags) {
  // Include two first original flags and search the mod directory
  IFiles::ListGameFiles(afnmDir, fnmDir, strPattern, (ulFlags & 3) | IFiles::FLF_SEARCHMOD);
};

// Check for file extensions that can be substituted
static BOOL SubstituteExtension(CTFileName &fnmFullFileName)
{
  CTString strExt = fnmFullFileName.FileExt();

  if (strExt == ".mp3") {
    fnmFullFileName = fnmFullFileName.NoExt() + ".ogg";
    return TRUE;

  } else if (strExt == ".ogg") {
    fnmFullFileName = fnmFullFileName.NoExt() + ".mp3";
    return TRUE;
  }

  // [Cecil] Was TRUE in 1.10 for some reason
  return FALSE;
};

static INDEX ExpandPathForReading(ULONG ulType, const CTFileName &fnmFile, CTFileName &fnmExpanded) {
  // Search for the file in archives
  INDEX iFileInZip = IUnzip::GetFileIndex(fnmFile);

  static CSymbolPtr symptr("fil_bPreferZips");
  BOOL bPreferZips = symptr.GetIndex();

  // If a mod is active
  if (_fnmMod != "") {
    // Try mod directory before archives
    if (!bPreferZips) {
      fnmExpanded = _fnmApplicationPath + _fnmMod + fnmFile;

      if (IFiles::IsReadable(fnmExpanded)) {
        return EFP_FILE;
      }
    }

    // If allowing archives
    if (!(ulType & EFP_NOZIPS)) {
      // Use if it exists in the mod archive
      if (iFileInZip >= 0 && IUnzip::IsFileAtIndexMod(iFileInZip)) {
        fnmExpanded = fnmFile;
        return EFP_MODZIP;
      }
    }

    // Try mod directory after archives
    if (bPreferZips) {
      fnmExpanded = _fnmApplicationPath + _fnmMod + fnmFile;

      if (IFiles::IsReadable(fnmExpanded)) {
        return EFP_FILE;
      }
    }
  }

  // Try game root directory before archives
  if (!bPreferZips) {
    CTFileName fnmAppPath = _fnmApplicationPath;
    IFiles::SetAbsolutePath(fnmAppPath);

    if (fnmFile.HasPrefix(fnmAppPath)) {
      fnmExpanded = fnmFile;
    } else {
      fnmExpanded = _fnmApplicationPath + fnmFile;
    }

    if (IFiles::IsReadable(fnmExpanded)) {
      return EFP_FILE;
    }
  }

  // If allowing archives
  if (!(ulType & EFP_NOZIPS)) {
    // Use it if exists in any archive
    if (iFileInZip >= 0) {
      fnmExpanded = fnmFile;
      return EFP_BASEZIP;
    }
  }

  // Try game root directory after archives
  if (bPreferZips) {
    fnmExpanded = _fnmApplicationPath + fnmFile;

    if (IFiles::IsReadable(fnmExpanded)) {
      return EFP_FILE;
    }
  }

  // Finally, try the CD path
  if (_fnmCDPath != "") {
    // Prioritize the mod directory
    if (_fnmMod != "") {
      fnmExpanded = _fnmCDPath + _fnmMod + fnmFile;

      if (IFiles::IsReadable(fnmExpanded)) {
        return EFP_FILE;
      }
    }

    fnmExpanded = _fnmCDPath + fnmFile;

    if (IFiles::IsReadable(fnmExpanded)) {
      return EFP_FILE;
    }
  }

  return EFP_NONE;
};

// Expand a filename to absolute path
INDEX P_ExpandFilePath(ULONG ulType, const CTFileName &fnmFile, CTFileName &fnmExpanded)
{
  CTFileName fnmFileAbsolute = fnmFile;
  IFiles::SetAbsolutePath(fnmFileAbsolute);

  // If writing
  if (ulType & EFP_WRITE) {
    // If should write into the mod directory
    if (_fnmMod != "" && (IFiles::MatchesList(IFiles::aBaseWriteInc, fnmFileAbsolute) == -1
                       || IFiles::MatchesList(IFiles::aBaseWriteExc, fnmFileAbsolute) != -1))
    {
      fnmExpanded = _fnmApplicationPath + _fnmMod + fnmFileAbsolute;
      IFiles::SetAbsolutePath(fnmExpanded);
      return EFP_FILE;

    // Otherwise write into the root directory
    } else {
      fnmExpanded = _fnmApplicationPath + fnmFileAbsolute;
      IFiles::SetAbsolutePath(fnmExpanded);
      return EFP_FILE;
    }

  // If reading
  } else if (ulType & EFP_READ) {
    // Check for expansions
    INDEX iRes = ExpandPathForReading(ulType, fnmFileAbsolute, fnmExpanded);

    // If not found
    if (iRes == EFP_NONE) {
      // Check for some file extensions that can be substituted
      CTFileName fnmReplace = fnmFileAbsolute;

      if (SubstituteExtension(fnmReplace)) {
        iRes = ExpandPathForReading(ulType, fnmReplace, fnmExpanded);
      }
    }

    if (iRes != EFP_NONE) {
      IFiles::SetAbsolutePath(fnmExpanded);
      return iRes;
    }

  // If unknown
  } else {
    ASSERT(FALSE);
    fnmExpanded = _fnmApplicationPath + fnmFileAbsolute;
    IFiles::SetAbsolutePath(fnmExpanded);
    return EFP_FILE;
  }

  fnmExpanded = _fnmApplicationPath + fnmFileAbsolute;
  IFiles::SetAbsolutePath(fnmExpanded);
  return EFP_NONE;
};
