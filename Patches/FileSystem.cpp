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

#if CLASSICSPATCH_ENGINEPATCHES

#include "FileSystem.h"
#include "../MapConversion.h"

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

// Replace nonexistent vanilla classes
static inline void ReplaceMissingClasses(CTString &strClassName, CTFileName &fnmDLL) {
  // Classes available in ExtraEntities library
  static const char *aExtras[] = {
    // Alpha enemies
    "CAirWave",
    "CCatman",
    "CCyborg",
    "CCyborgBike",
    "CDragonman",
    "CFishman",
    "CHuanman",
    "CMamut",
    "CMamutman",
    "CMantaman",
    "CRobotDriving",
    "CRobotFixed",
    "CRobotFlying",
    "CTerrainEntity",

    // Revolution entities (commented ones aren't finished)
    "CAchievementEntity",
    "CControlZoneEntity",
    "CDestroyer",
    "CFlagItem",
    //"CModelHolder2Movable",
    //"CPostProcessingEffect",
    "CSpectatorCamera",
    "CUghzy",
    //"CVehicle",
    "CWorldInfoEntity",
    NULL,
  };

  // Replace classes with something from ExtraEntities
  if (LoadClassFromExtras(strClassName, fnmDLL, aExtras)) return;

  // It should only reach this point when custom mod is disabled,
  // which means that in Revolution these entities already exist
#if SE1_GAME != SS_REV

  // Not a Revolution map
  if (_EnginePatches._eWorldFormat != E_LF_SSR) return;

  // Replace some vanilla enemies with those from ExtraEntities library
  static const char *aRevEnemies[] = {
    "CElemental",
    "CHeadman",
    "CWalker",
    NULL,
  };

  if (LoadClassFromExtras(strClassName, fnmDLL, aRevEnemies)) return;

  // Replace classes from Revolution
  static ClassReplacementPair aRevReplace[] = {
    // Alpha enemies
    { "CCatman",               "CGrunt" },
    { "CCyborg",               "CWalker" },
    { "CDragonman",            "CWoman" },
    { "CFishman",              "CHeadman" },
    { "CHuanman",              "CGrunt" },
    { "CMamut",                "CWerebull" },
    { "CMamutman",             "CHeadman" },
    { "CMantaman",             "CFish" },
    { "CRobotDriving",         "CGrunt" },
    { "CRobotFlying",          "CWoman" },

    // Revolution entities
    { "CAchievementEntity",    "CTrigger" },
    { "CControlZoneEntity",    "CTrigger" },
    { "CDestroyer",            "CDemon" },
    { "CFlagItem",             "CHealthItem" },
    { "CPostProcessingEffect", "CMarker" },
    { "CSpectatorCamera",      "CMarker" },
    { "CUghzy",                "CGuffy" },
    { "CWorldInfoEntity",      "CMarker" },
    { NULL, NULL },
  };

  ReplaceClassFromTable(strClassName, aRevReplace);

#endif // SE1_GAME != SS_REV
};

// Load entity class from a library
void CEntityClassPatch::P_Read(CTStream *istr) {
  // Read library filename and class name
  CTFileName fnmDLL;
  fnmDLL.ReadFromText_t(*istr, "Package: ");

  CTString strClassName;
  strClassName.ReadFromText_t(*istr, "Class: ");

  // [Cecil] Replace nonexistent vanilla classes
  const BOOL bVanillaEntities = (fnmDLL == "Bin\\Entities.dll");

  if (bVanillaEntities) {
    // [Cecil] NOTE: fnmDLL may be changed but 'bVanillaEntities' should NOT be recalculated!
    ReplaceMissingClasses(strClassName, fnmDLL);
  }

  // [Cecil] Construct full path to the entities library
  const CTString strLibName = fnmDLL.FileName();
  const CTString strLibExt = fnmDLL.FileExt();

  // Find appropriate default entities library
  if (CCoreAPI::IsCustomModActive() && bVanillaEntities) {
    fnmDLL = CCoreAPI::AppPath() + CCoreAPI::FullLibPath(strLibName + _strModExt, strLibExt);

  // Use original path to the library
  } else {
    // Mod extension for mods or vanilla extension for entity packs
    const CTString &strCurrentExt = (_fnmMod != "" ? _strModExt : CCoreAPI::GetVanillaExt());

    CTFileName fnmExpand = fnmDLL.FileDir() + CCoreAPI::GetLibFile(strLibName + strCurrentExt, strLibExt);
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

// Read the dictionary from a given offset
void CStreamPatch::P_ReadDictionary_intenal(SLONG slOffset) {
  // Remember last position
  const SLONG slContinue = GetPos_t();

  // Start dictionary processing from a new position
  SetPos_t(slOffset);
  strm_dmDictionaryMode = DM_PROCESSING;

  ExpectID_t("DICT"); // Dictionary

  // Read number of new filenames
  INDEX ctOld = strm_afnmDictionary.Count();

  INDEX ctNew;
  *this >> ctNew;

  // If there are any new filenames
  if (ctNew > 0) {
    // Make space and read them
    strm_afnmDictionary.Push(ctNew);

    for (INDEX i = ctOld; i < ctOld + ctNew; i++) {
      // Read path
      CTFileName &fnm = strm_afnmDictionary[i];
      *this >> fnm;

      // [Cecil] Fix Revolution directories
      IFiles::FixRevPath(fnm);
    }
  }

  ExpectID_t("DEND"); // Dictionary end

  // Remember dictionary end position and return back
  strm_slDictionaryPos = GetPos_t();
  SetPos_t(slContinue);
};

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
  BOOL bRev = FALSE;

#if TSE_FUSION_MODE
  // Setup other game directories
  if (CCoreAPI::Props().bMountTFE) {
    SetupGameDir(GAME_DIR_TFE, CCoreAPI::Props().strTFEDir, CONFIG_DEFAULT_DIR_TFE);
  }
  if (CCoreAPI::Props().bMountSSR) {
    bRev = SetupGameDir(GAME_DIR_SSR, CCoreAPI::Props().strSSRDir, CONFIG_DEFAULT_DIR_SSR);
  }
#endif

  if (CCoreAPI::Props().bMountSSRWorkshop) {
    // Specify path to Revolution workshop relative to the Steam game
    CTString strWorkshop = CCoreAPI::Props().strSSRWorkshop;

    if (bRev && strWorkshop == "") {
      strWorkshop = GAME_DIR_SSR + CONFIG_DEFAULT_DIR_WORKSHOP;
    }

    // Verify workshop directory and load workshop files
    if (strWorkshop != "") {
      IFiles::SetFullDirectory(strWorkshop);

      DWORD dwAttrib = GetFileAttributesA(strWorkshop.str_String);

      if (dwAttrib != -1 && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        CFileList aWorkshop;
        IFiles::ListInDir(strWorkshop, aWorkshop, "", "*.gro", TRUE, NULL, NULL);
        IFiles::ListInDir(strWorkshop, aWorkshop, "", "*_legacy.bin", TRUE, NULL, NULL); // Legacy archives

        // Add directories with GRO packages from workshop
        const INDEX ctDirs = aWorkshop.Count();

        for (INDEX iDir = 0; iDir < ctDirs; iDir++) {
          // Add path to the workshop directory
          CTFileName fnmDir = strWorkshop + aWorkshop[iDir].FileDir();
          _aContentDirs.Push() = fnmDir;
        }
      }
    }
  }

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
    LoadPackages(fnmDir, "*_legacy.bin"); // Legacy SSR workshop archives
  }

  // Proceed to the original function
  pInitStreams();

  // Sort files in ZIP archives by content directory
  IUnzip::SortEntries();

  // Set custom mod extension to utilize Entities & Game libraries from the patch
  BOOL bChangeExtension = FALSE;

  if (CCoreAPI::Props().bCustomMod) {
    // Change by default
    bChangeExtension = TRUE;

    // Check if a mod has its own libraries under the current extension
    if (_fnmMod != "") {
      const CTString strModBin = CCoreAPI::AppPath() + _fnmMod + "Bin\\";

      // Search specifically under "Bin" because that's how all classic mods are structured
      const CTString strEntities = strModBin + CCoreAPI::GetLibFile("Entities" + _strModExt);
      const CTString strGameLib  = strModBin + CCoreAPI::GetLibFile("Game" + _strModExt);

      // Safe to change if no mod libraries
      bChangeExtension = (!IFiles::IsReadable(strEntities.str_String) && !IFiles::IsReadable(strGameLib.str_String));
    }

  // Custom mod is disabled but the mod might still use its libraries (e.g. ClassicsPatchMod)
  } else if (_fnmMod != "" && _strModExt == CLASSICSPATCH_SUFFIX) {
    // Make sure the libraries are not located in the mod folder
    const CTString strEntities = CCoreAPI::FullLibPath("Entities" CLASSICSPATCH_SUFFIX);
    const CTString strGameLib  = CCoreAPI::FullLibPath("Game" CLASSICSPATCH_SUFFIX);

    // Safe to change if no mod libraries
    bChangeExtension = (!strEntities.HasPrefix(_fnmMod) && !strGameLib.HasPrefix(_fnmMod));
  }

  // Change mod extension for the base game or for mods with no libraries
  if (bChangeExtension) {
    _strModExt = CLASSICSPATCH_SUFFIX;
    CCoreAPI::SetCustomMod(TRUE);

  } else {
    CCoreAPI::SetCustomMod(FALSE);
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
  BOOL bPreferZips = (symptr.Exists() ? symptr.GetIndex() : FALSE);

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
INDEX P_ExpandFilePath(EXPAND_PATH_ARGS(ULONG ulType, const CTFileName &fnmFile, CTFileName &fnmExpanded, BOOL bUseRPH))
{
  CTFileName fnmFileAbsolute = fnmFile;

  #if SE1_GAME == SS_REV
    // [Cecil] Rev: Fix path formatting and use replace history
    fnmFileAbsolute.FixFormatting();

    if (bUseRPH) {
      rphPassReplace(fnmFileAbsolute);
    }

  #else
    // [Cecil] Reading resources from Revolution worlds
    const BOOL bRevWorld = (_EnginePatches._eWorldFormat == E_LF_SSR);

    // [Cecil] Fix formatting of Revolution paths
    if (bRevWorld) {
      IFiles::FixRevPath(fnmFileAbsolute);
    }
  #endif

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

  #if SE1_GAME != SS_REV
    // [Cecil] Try remapping Revolution paths, if can't find a file
    if (bRevWorld)
    {
      // 1. Try converting spaces
      if (iRes == EFP_NONE) {
        CTFileName fnmCopy = fnmFileAbsolute;
        IData::ReplaceChar(fnmCopy.str_String, ' ', '_');

        iRes = ExpandPathForReading(ulType, fnmCopy, fnmExpanded);
      }

      // 2. Discard the last result and try searching under remapped directories
      if (iRes == EFP_NONE) {
        CTFileName fnmRemap;

        #define REMAP_PATH(_Old, _New) if (fnmFileAbsolute.RemovePrefix(_Old)) fnmRemap = _New + fnmFileAbsolute

        REMAP_PATH("TexturesMP\\", "Textures\\");
        else
        REMAP_PATH("SoundsMP\\", "Sounds\\");
        else
        REMAP_PATH("MusicMP\\", "Music\\");
        else
        REMAP_PATH("ModelsMP\\", "Models\\");
        else
        REMAP_PATH("Levels\\LevelsMP\\", "Levels\\");
        else
        REMAP_PATH("DataMP\\", "Data\\");
        else
        REMAP_PATH("AnimationsMP\\", "Animations\\");
        else
        REMAP_PATH("Chaos_Studios\\", "Textures\\Chaos_Studios\\");
        else
        REMAP_PATH("NifransTextures\\", "Textures\\NifransTextures\\");

        if (fnmRemap != "") {
          fnmFileAbsolute = fnmRemap;
          iRes = ExpandPathForReading(ulType, fnmFileAbsolute, fnmExpanded);

          // 3. Try converting spaces again under the remapped directory
          if (iRes == EFP_NONE) {
            IData::ReplaceChar(fnmFileAbsolute.str_String, ' ', '_');
            iRes = ExpandPathForReading(ulType, fnmFileAbsolute, fnmExpanded);
          }
        }
      }
    }
  #endif

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
