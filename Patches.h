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

// Import library for use
#ifndef ENGINEPATCHES_EXPORTS
  #pragma comment(lib, "ClassicsPatches.lib")

  #define PATCHES_API __declspec(dllimport)
#else
  #define PATCHES_API __declspec(dllexport)
#endif

// Check if the fusion mode is available (only for TSE)
#define TSE_FUSION_MODE (SE1_GAME != SS_TFE)

// Available engine patches
class PATCHES_API CPatches {
  public:
    // Rendering
    INDEX _bAdjustForAspectRatio;
    INDEX _bUseVerticalFOV;
    FLOAT _fCustomFOV;
    FLOAT _fThirdPersonFOV;
    INDEX _bCheckFOV;

    // Sound library
    BOOL _bNoListening; // Don't listen to in-game sounds

    // Unpage streams
    ULONG _ulMaxWriteMemory; // Enough memory for writing

    // Worlds
    INDEX _bFirstEncounter; // Set if the last loaded map is from the TFE directory
    INDEX _bReinitWorld; // Force entity reinitialization

  public:
    // Constructor
    CPatches();

    // Apply core patches (called after Core initialization!)
    void CorePatches(void) {
      const BOOL bGame = GetAPI()->IsGameApp();
      const BOOL bServer = GetAPI()->IsServerApp();
      const BOOL bEditor = GetAPI()->IsEditorApp();

      // Patch for everything
      Network();
      Strings();
      Textures();

      // Patch for the game and the editor
      if (bGame || bEditor) {
        Rendering();

        #if SE1_VER >= SE1_107
          Ska();
        #endif

        if (bGame) {
          SoundLibrary();
          Worlds();
        }
      }

      // Patch for the server
      if (bServer) {
        NOTHING;
      }
    };

    #if SE1_VER >= SE1_107
      // Apply patches upon loading the shaders library
      void ShadersPatches(void);
    #endif

  // Patches after Serious Engine and Core initializations
  public:

    // Enhance network library usage
    void Network(void);

    // Enhance rendering
    void Rendering(void);

    #if SE1_VER >= SE1_107
      // Fix SKA models
      void Ska(void);
    #endif

    // Enhance sound library usage
    void SoundLibrary(void);

    // Enhance strings usage
    void Strings(void);

    // Enhance texture usage
    void Textures(void);

    // Enhance worlds
    void Worlds(void);

  // Patches before Serious Engine and Core initialization
  public:

    // Customize core file handling in the engine
    void FileSystem(void);

    // Don't use memory paging in streams
    void UnpageStreams(void);
};

// Singleton for patching
PATCHES_API extern CPatches _EnginePatches;
