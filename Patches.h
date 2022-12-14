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

// Import library for use
#ifndef ENGINEPATCHES_EXPORTS
  #pragma comment(lib, "EnginePatches.lib")
#endif

// Available engine patches
class CPatches {
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

      // Patch for the game
      if (bGame) {
        Rendering();
        SoundLibrary();
      }

      // Patch for the server
      if (bServer) {
        NOTHING;
      }

      // Patch for the editor
      if (bEditor) {
        Rendering();
      }
    };

  // Specific patches
  public:

    // Enhance network library usage
    void Network(void);

    // Enhance rendering
    void Rendering(void);

    // Enhance sound library usage
    void SoundLibrary(void);

    // Enhance strings usage
    void Strings(void);

    // Enhance texture usage
    void Textures(void);

    // Don't use memory paging in streams (called before engine initialization!)
    void UnpageStreams(void);
};

// Singleton for patching
extern CPatches _EnginePatches;
