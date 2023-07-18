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

#if CLASSICSPATCH_ENGINEPATCHES

#include "FileSystem.h"

#include <STLIncludesBegin.h>
#include <fstream>
#include <sstream>
#include <STLIncludesEnd.h>

#if CLASSICSPATCH_EXTEND_FILESYSTEM

// Obtain components of the entity class
void CEntityClassPatch::P_ObtainComponents(void)
{
  static CSymbolPtr piPrecachePolicy("gam_iPrecachePolicy");
  const INDEX iPolicy = piPrecachePolicy.GetIndex();

  for (INDEX i = 0; i < ec_pdecDLLClass->dec_ctComponents; i++) {
    CEntityComponent &ec = ec_pdecDLLClass->dec_aecComponents[i];

    // If not precaching everything
    if (iPolicy < PRECACHE_ALL) {
      // Skip non-class components
      if (ec.ec_ectType != ECT_CLASS) continue;
    }

    // Try to obtain the component
    try {
      ec.Obtain_t();

    } catch (char *) {
      // Fail if in paranoia mode
      if (iPolicy == PRECACHE_PARANOIA) throw;
    }
  }
};

// Load entity class from a library
void CEntityClassPatch::P_Read(CTStream *istr) {
  // Read library filename and class name
  CTFileName fnmDLL;
  fnmDLL.ReadFromText_t(*istr, "Package: ");

  CTString strClassName;
  strClassName.ReadFromText_t(*istr, "Class: ");

  // [Cecil] Construct full path to the entities library
  const CTString strLibName = fnmDLL.FileName();
  const CTString strLibExt = fnmDLL.FileExt();

#if SE1_VER >= SE1_107
  // Find appropriate default entities library
  if (fnmDLL == "Bin\\Entities.dll") {
    fnmDLL = CCoreAPI::AppPath() + CCoreAPI::FullLibPath(strLibName + _strModExt, strLibExt);

  } else
#endif
  // Use original path to the library
  {
    CTFileName fnmExpand = fnmDLL.FileDir() + CCoreAPI::GetLibFile(strLibName + CCoreAPI::GetVanillaExt(), strLibExt);
    ExpandFilePath(EFP_READ, fnmExpand, fnmDLL);
  }

  // Load class library
  ec_hiClassDLL = CCoreAPI::LoadLib(fnmDLL.str_String);
  ec_fnmClassDLL = fnmDLL;

  // Get pointer to the library class structure
  ec_pdecDLLClass = (CDLLEntityClass *)GetProcAddress(ec_hiClassDLL, (strClassName + "_DLLClass").str_String);

  // Class structure is not found
  if (ec_pdecDLLClass == NULL) {
    BOOL bSuccess = FreeLibrary(ec_hiClassDLL);
    ASSERT(bSuccess);

    ec_hiClassDLL = NULL;
    ec_fnmClassDLL.Clear();

    ThrowF_t(LOCALIZE("Class '%s' not found in entity class package file '%s'"), strClassName, fnmDLL);
  }

  // Obtain all components needed by the class
  {
    CTmpPrecachingNow tpn;
    P_ObtainComponents();
  }

  // Attach the library
  ec_pdecDLLClass->dec_OnInitClass();

  // Make sure that the properties have been properly declared
  CheckClassProperties();
};

#if SE1_VER >= SE1_107

// Load shader from a library
void CShaderPatch::P_Read(CTStream *istrFile) {
  // Read library filename and function names
  CTFileName fnmDLL;
  CTString strShaderFunc, strShaderInfo;

  fnmDLL.ReadFromText_t(*istrFile, "Package: ");
  strShaderFunc.ReadFromText_t(*istrFile, "Name: ");
  strShaderInfo.ReadFromText_t(*istrFile, "Info: ");

  // [Cecil] Construct full path to the shaders library
  fnmDLL = CCoreAPI::AppPath() + CCoreAPI::FullLibPath(fnmDLL.FileName(), fnmDLL.FileExt());

  // Load shader library
  const UINT iOldErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

  hLibrary = CCoreAPI::LoadLib(fnmDLL.str_String);

  SetErrorMode(iOldErrorMode);

  // Make sure it's loaded
  if (hLibrary == NULL) istrFile->Throw_t("Error loading '%s' library", fnmDLL.str_String);

  // Get shader rendering function
  ShaderFunc = (void (*)(void))GetProcAddress(hLibrary, strShaderFunc.str_String);
  if (ShaderFunc == NULL) istrFile->Throw_t("GetProcAddress 'ShaderFunc' Error");

  // Get shader info function
  GetShaderDesc = (void (*)(ShaderDesc &))GetProcAddress(hLibrary, strShaderInfo.str_String);
  if (GetShaderDesc == NULL) istrFile->Throw_t("GetProcAddress 'ShaderDesc' Error");
};

#endif

// List of extra content directories
static CFileList _aContentDirs;

// Original function pointer
void (*pInitStreams)(void) = NULL;

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

// Setup some game directory
static BOOL SetupGameDir(CTString &strGameDir, CTString &strDirProperty, const CTString &strDefaultPath) {
  // Set directory from the property if it's not set yet
  if (strGameDir == "" && strDirProperty != "") {
    strGameDir = strDirProperty;
  }

  // Set default path and update the property, if it's empty
  if (strGameDir == "") {
    strGameDir = strDefaultPath;
    strDirProperty = strGameDir;
  }

  // Abort if still no path
  if (strGameDir == "") return FALSE;

  // Make it into a full path
  IFiles::SetFullDirectory(strGameDir);

  // Reset if the directory doesn't exist
  DWORD dwAttrib = GetFileAttributesA(strGameDir.str_String);

  if (dwAttrib == -1 || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
    strGameDir = "";
    return FALSE;

  // Otherwise add it as a content directory for loading extra GRO packages from
  } else {
    _aContentDirs.Push() = strGameDir;
  }

  return TRUE;
};

// Initialize various file paths and load game content
void P_InitStreams(void) {
  #if TSE_FUSION_MODE
    // Setup other game directories
    SetupGameDir(GAME_DIR_TFE, CCoreAPI::Props().strTFEDir, "..\\Serious Sam Classic The First Encounter\\");
  #endif

  // Read list of content directories without engine's streams
  const CTFileName fnmDirList = CCoreAPI::AppPath() + "Data\\ContentDir.lst";

  if (IFiles::IsReadable(fnmDirList.str_String)) {
    std::ifstream inDirList(fnmDirList.str_String);
    std::string strLine;

    while (std::getline(inDirList, strLine)) {
      // Skip empty lines
      if (strLine.find_first_not_of(" \r\n\t\\") == std::string::npos) {
        continue;
      }

      _aContentDirs.Push() = CTString(strLine.c_str());
    }
  }

  // Load extra GRO packages from specified content directories
  const INDEX ctDirs = _aContentDirs.Count();

  for (INDEX iDir = 0; iDir < ctDirs; iDir++) {
    // Make directory into a full path
    CTFileName fnmDir = _aContentDirs[iDir];
    IFiles::SetFullDirectory(fnmDir);

    // Skip if the directory doesn't exist
    DWORD dwAttrib = GetFileAttributesA(fnmDir.str_String);

    if (dwAttrib == -1 || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
      continue;
    }

    LoadPackages(fnmDir, "*.gro");
  }

  // Proceed to the original function
  pInitStreams();

  // Sort files in ZIP archives by content directory
  IUnzip::SortEntries();

  // Set custom mod extension to utilize Entities & Game libraries from the patch
  if (CCoreAPI::Props().bCustomMod) {
    BOOL bChangeExtension = TRUE;

    // Check if a mod has its own libraries under the current extension
    if (_fnmMod != "") {
      const CTString strModBin = CCoreAPI::AppPath() + _fnmMod + "Bin\\";

      // Search specifically under "Bin" because that's how all classic mods are structured
      const CTString strEntities = strModBin + CCoreAPI::GetLibFile("Entities" + _strModExt);
      const CTString strGameLib  = strModBin + CCoreAPI::GetLibFile("Game" + _strModExt);

      // Safe to change if no mod libraries
      bChangeExtension = (!IFiles::IsReadable(strEntities.str_String) && !IFiles::IsReadable(strGameLib.str_String));
    }

    // Change mod extension for the base game or for mods with no libraries
    if (bChangeExtension) {
      _strModExt = "_Custom";

      // Activate custom mod
      CCoreAPI::bCustomMod = TRUE;
    }
  }
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

// [Cecil] Check if file exists at a specific path (relative or absolute)
static inline BOOL CheckFileAt(const CTString &strBaseDir, const CTFileName &fnmFile, CTFileName &fnmExpanded) {
  if (fnmFile.HasPrefix(strBaseDir)) {
    fnmExpanded = fnmFile;
  } else {
    fnmExpanded = strBaseDir + fnmFile;
  }

  return IFiles::IsReadable(fnmExpanded);
};

static INDEX ExpandPathForReading(ULONG ulType, const CTFileName &fnmFile, CTFileName &fnmExpanded) {
  // Search for the file in archives
  const INDEX iFileInZip = IUnzip::GetFileIndex(fnmFile);
  const BOOL bFoundInZip = (iFileInZip >= 0);

  static CSymbolPtr symptr("fil_bPreferZips");
  BOOL bPreferZips = symptr.GetIndex();

  // [Cecil] Check file at a specific directory and return if it exists
  #define RETURN_FILE_AT(_Dir) \
    { if (CheckFileAt(_Dir, fnmFile, fnmExpanded)) return EFP_FILE; }

  // If a mod is active
  if (_fnmMod != "") {
    // Try mod directory before archives
    if (!bPreferZips) {
      RETURN_FILE_AT(CCoreAPI::AppPath() + _fnmMod);
    }

    // If allowing archives
    if (!(ulType & EFP_NOZIPS)) {
      // Use if it exists in the mod archive
      if (bFoundInZip && IUnzip::IsFileAtIndexMod(iFileInZip)) {
        fnmExpanded = fnmFile;
        return EFP_MODZIP;
      }
    }

    // Try mod directory after archives
    if (bPreferZips) {
      RETURN_FILE_AT(CCoreAPI::AppPath() + _fnmMod);
    }
  }

  // Try game root directory before archives
  if (!bPreferZips) {
    RETURN_FILE_AT(CCoreAPI::AppPath());
  }

  // If allowing archives
  if (!(ulType & EFP_NOZIPS)) {
    // Use it if exists in any archive
    if (bFoundInZip) {
      fnmExpanded = fnmFile;
      return EFP_BASEZIP;
    }
  }

  // Try game root directory after archives
  if (bPreferZips) {
    RETURN_FILE_AT(CCoreAPI::AppPath());
  }

  // [Cecil] Try searching other game directories after the main one
  for (INDEX iDir = GAME_DIRECTORIES_CT - 1; iDir >= 0; iDir--) {
    const CTString &strDir = _astrGameDirs[iDir];

    // No game directory
    if (strDir == "") continue;

    RETURN_FILE_AT(strDir);
  }

  // Finally, try the CD path
  if (_fnmCDPath != "") {
    // Prioritize the mod directory
    if (_fnmMod != "") {
      RETURN_FILE_AT(_fnmCDPath + _fnmMod);
    }

    RETURN_FILE_AT(_fnmCDPath);
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
      fnmExpanded = CCoreAPI::AppPath() + _fnmMod + fnmFileAbsolute;
      IFiles::SetAbsolutePath(fnmExpanded);
      return EFP_FILE;

    // Otherwise write into the root directory
    } else {
      fnmExpanded = CCoreAPI::AppPath() + fnmFileAbsolute;
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
    fnmExpanded = CCoreAPI::AppPath() + fnmFileAbsolute;
    IFiles::SetAbsolutePath(fnmExpanded);
    return EFP_FILE;
  }

  fnmExpanded = CCoreAPI::AppPath() + fnmFileAbsolute;
  IFiles::SetAbsolutePath(fnmExpanded);
  return EFP_NONE;
};

#endif // CLASSICSPATCH_EXTEND_FILESYSTEM

#endif // CLASSICSPATCH_ENGINEPATCHES
