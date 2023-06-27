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

#ifndef CECIL_INCL_PATCHES_FILESYSTEM_H
#define CECIL_INCL_PATCHES_FILESYSTEM_H

#ifdef PRAGMA_ONCE
  #pragma once
#endif

#include <Engine/Entities/EntityClass.h>

#if SE1_VER >= SE1_107
  #include <Engine/Graphics/Shader.h>
#endif

#include <CoreLib/Interfaces/FileFunctions.h>

#if CLASSICSPATCH_ENGINEPATCHES && CLASSICSPATCH_EXTEND_FILESYSTEM

class CEntityClassPatch : public CEntityClass {
  public:
    // Obtain components of the entity class
    void P_ObtainComponents(void);

    // Load entity class from a library
    void P_Read(CTStream *istr);
};

#if SE1_VER >= SE1_107

class CShaderPatch : public CShader {
  public:
    // Load shader from a library
    void P_Read(CTStream *istr);
};

#endif

// Initialize various file paths and load game content
void P_InitStreams(void);

// Make a list of all files in a directory
void P_MakeDirList(CFileList &afnmDir, const CTFileName &fnmDir, const CTString &strPattern, ULONG ulFlags);

// Expand a filename to absolute path
INDEX P_ExpandFilePath(ULONG ulType, const CTFileName &fnmFile, CTFileName &fnmExpanded);

#endif // CLASSICSPATCH_EXTEND_FILESYSTEM

#endif
