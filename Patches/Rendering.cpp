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

#include "StdH.h"

#include "Rendering.h"

// Coordinate conversion function
static inline PIX PIXCoord(FLOAT f) {
  static FLOAT fDiff;
  static SLONG slTmp;

  PIX pixRet;

  __asm {
    fld    dword ptr [f]
    fist   dword ptr [slTmp]
    fisubr dword ptr [slTmp]
    fstp   dword ptr [fDiff]
    mov    eax, dword ptr [slTmp]
    mov    edx, dword ptr [fDiff]
    add    edx, 0x7FFFFFFF
    adc    eax, 0
    mov    dword ptr [pixRet], eax
  }

  return pixRet;
};

// Define unexported method
void CRenderer::InitClippingRectangle(PIX pixMinI, PIX pixMinJ, PIX pixSizeI, PIX pixSizeJ) {
  re_pspoFirst = NULL;
  re_pspoFirstTranslucent = NULL;
  re_pspoFirstBackground = NULL;
  re_pspoFirstBackgroundTranslucent = NULL;

  const FLOAT fClipMarginAdd = -0.5f;
  const FLOAT fClipMarginSub = 0.5f;

  re_fMinJ = pixMinJ;
  re_fMaxJ = pixMinJ + pixSizeJ;
  re_pixSizeI = pixSizeI;
  re_fbbClipBox = FLOATaabbox2D(FLOAT2D((FLOAT)pixMinI + fClipMarginAdd, (FLOAT)pixMinJ + fClipMarginAdd),
                                FLOAT2D((FLOAT)pixMinI + pixSizeI - fClipMarginSub, (FLOAT)pixMinJ + pixSizeJ - fClipMarginSub));
  re_pixTopScanLineJ = PIXCoord(pixMinJ + fClipMarginAdd);
  re_ctScanLines = PIXCoord(pixSizeJ - fClipMarginSub) - PIXCoord(fClipMarginAdd);
  re_pixBottomScanLineJ = re_pixTopScanLineJ + re_ctScanLines;
};

// Pointer to CRenderer::Render()
static StructPtr _pRenderFunc(ADDR_RENDERER_RENDER);

// RenderView() copy
static void RenderViewCopy(CWorld &woWorld, CEntity &enViewer, CAnyProjection3D &apr, CDrawPort &dp) {
  if (woWorld.wo_pecWorldBaseClass != NULL
   && woWorld.wo_pecWorldBaseClass->ec_pdecDLLClass != NULL
   && woWorld.wo_pecWorldBaseClass->ec_pdecDLLClass->dec_OnWorldRender != NULL) {
    woWorld.wo_pecWorldBaseClass->ec_pdecDLLClass->dec_OnWorldRender(&woWorld);
  }

  if (_wrpWorldRenderPrefs.GetShadowsType() == CWorldRenderPrefs::SHT_FULL) {
    woWorld.CalculateNonDirectionalShadows();
  }

  // Retrieve renderer from '_areRenderers[0]'
  CRenderer &re = *ADDR_RENDERER_ARRAY;

  re.re_penViewer = &enViewer;
  re.re_pcspoViewPolygons = NULL;
  re.re_pwoWorld = &woWorld;
  re.re_prProjection = apr;
  re.re_pdpDrawPort = &dp;

  re.InitClippingRectangle(0, 0, dp.GetWidth(), dp.GetHeight());
  apr->ScreenBBoxL() = FLOATaabbox2D(FLOAT2D(0.0f, 0.0f), FLOAT2D(dp.GetWidth(), dp.GetHeight()));

  re.re_bRenderingShadows = FALSE;
  re.re_ubLightIllumination = 0;

  // Call CRenderer::Render() from the pointer
  void (CRenderer::*pDummy)(void);
  (re.*_pRenderFunc(pDummy))();

  // Call API method after rendering the world
  GetAPI()->OnRenderView(woWorld, &enViewer, apr, &dp);
};

// Patched function
void P_RenderView(CWorld &woWorld, CEntity &enViewer, CAnyProjection3D &apr, CDrawPort &dp)
{
  // Set wide adjustment based on current aspect ratio
  if (_EnginePatches._bAdjustForAspectRatio) {
    dp.dp_fWideAdjustment = ((FLOAT)dp.GetHeight() / (FLOAT)dp.GetWidth()) * (4.0f / 3.0f);
  } else {
    dp.dp_fWideAdjustment = 1.0f;
  }

  // Not a perspective projection
  if (!apr.IsPerspective()) {
    // Proceed to the original function
    RenderViewCopy(woWorld, enViewer, apr, dp);
    return;
  }

  BOOL bPlayer = IsDerivedFromClass(&enViewer, "PlayerEntity");
  BOOL bView = IsOfClass(&enViewer, "Player View");

  // Change FOV for the player view
  if (bPlayer || bView) {
    CPerspectiveProjection3D &ppr = *((CPerspectiveProjection3D *)(CProjection3D *)apr);

    FLOAT2D vScreen(dp.GetWidth(), dp.GetHeight());

    // Adjust clip distance according to the aspect ratio
    ppr.FrontClipDistanceL() *= (vScreen(2) / vScreen(1)) * (4.0f / 3.0f);

    FLOAT &fNewFOV = ppr.ppr_FOVWidth;

    // Set custom FOV if not zooming in
    if (fNewFOV > 80.0f) {
      // Set different FOV for the third person view
      if (bView && _EnginePatches._fThirdPersonFOV > 0.0f) {
        fNewFOV = Clamp(_EnginePatches._fThirdPersonFOV, 60.0f, 110.0f);

      // Set first person view FOV
      } else if (_EnginePatches._fCustomFOV > 0.0f) {
        fNewFOV = Clamp(_EnginePatches._fCustomFOV, 60.0f, 110.0f);
      }
    }

    // Display current FOV values
    if (_EnginePatches._bCheckFOV) {
      FLOAT fCheckFOV = fNewFOV;

      // Starting horizontal FOV
      CPrintF("View HFOV: %.2f\n", fCheckFOV);

      // Starting vertical FOV
      FLOAT fVFOV = fCheckFOV;
      IRender::AdjustVFOV(vScreen, fVFOV);

      CPrintF("View VFOV: %.2f\n", fVFOV);

      // FOV patch
      if (_EnginePatches._bUseVerticalFOV) {
        IRender::AdjustHFOV(vScreen, fCheckFOV);
      }

      // Proper horizontal FOV after the patch
      CPrintF("New HFOV:  %.2f\n", fCheckFOV);

      // Proper vertical FOV after the patch
      fVFOV = fCheckFOV;
      IRender::AdjustVFOV(vScreen, fVFOV);

      CPrintF("New VFOV:  %.2f\n", fVFOV);
    }

    _EnginePatches._bCheckFOV = FALSE;
  }

  // Proceed to the original function
  RenderViewCopy(woWorld, enViewer, apr, dp);
};

// Prepare the perspective projection
void CProjectionPatch::P_Prepare(void) {
  // Original function code
  FLOATmatrix3D t3dObjectStretch;
  FLOATmatrix3D t3dObjectRotation;

  MakeRotationMatrixFast(t3dObjectRotation, pr_ObjectPlacement.pl_OrientationAngle);
  MakeInverseRotationMatrixFast(pr_ViewerRotationMatrix, pr_ViewerPlacement.pl_OrientationAngle);
  t3dObjectStretch.Diagonal(pr_ObjectStretch);

  pr_vViewerPosition = pr_ViewerPlacement.pl_PositionVector;

  BOOL bXInverted = pr_ObjectStretch(1) < 0.0f;
  BOOL bYInverted = pr_ObjectStretch(2) < 0.0f;
  BOOL bZInverted = pr_ObjectStretch(3) < 0.0f;

  pr_bInverted = bXInverted != bYInverted != bZInverted;

  if (pr_bMirror) {
    ReflectPositionVectorByPlane(pr_plMirror, pr_vViewerPosition);
    ReflectRotationMatrixByPlane_rows(pr_plMirror, pr_ViewerRotationMatrix);

    pr_plMirrorView = pr_plMirror;
    pr_plMirrorView -= pr_vViewerPosition;
    pr_plMirrorView *= pr_ViewerRotationMatrix;
    pr_bInverted = !pr_bInverted;

  } else if (pr_bWarp) {
    pr_plMirrorView = pr_plMirror;
  }

  if (pr_bFaceForward) {
    if (pr_bHalfFaceForward) {
      FLOAT3D vY(t3dObjectRotation(1, 2), t3dObjectRotation(2, 2), t3dObjectRotation(3, 2));

      FLOAT3D vViewerZ(pr_ViewerRotationMatrix(3, 1), pr_ViewerRotationMatrix(3, 2), pr_ViewerRotationMatrix(3, 3));
      FLOAT3D vX = (-vViewerZ) * vY;
      vX.Normalize();

      FLOAT3D vZ = vY*vX;
      t3dObjectRotation(1, 1) = vX(1); t3dObjectRotation(1, 2) = vY(1); t3dObjectRotation(1, 3) = vZ(1);
      t3dObjectRotation(2, 1) = vX(2); t3dObjectRotation(2, 2) = vY(2); t3dObjectRotation(2, 3) = vZ(2);
      t3dObjectRotation(3, 1) = vX(3); t3dObjectRotation(3, 2) = vY(3); t3dObjectRotation(3, 3) = vZ(3);

      pr_mDirectionRotation = pr_ViewerRotationMatrix * t3dObjectRotation;
      pr_RotationMatrix = pr_mDirectionRotation * t3dObjectStretch;

    } else {
      FLOATmatrix3D mBanking;
      MakeRotationMatrixFast(mBanking, ANGLE3D(0.0f, 0.0f, pr_ObjectPlacement.pl_OrientationAngle(3)));
      pr_mDirectionRotation = mBanking;
      pr_RotationMatrix = mBanking * t3dObjectStretch;
    }

  } else {
    pr_mDirectionRotation = pr_ViewerRotationMatrix * t3dObjectRotation;
    pr_RotationMatrix = pr_mDirectionRotation * t3dObjectStretch;
  }

  pr_TranslationVector = pr_ObjectPlacement.pl_PositionVector - pr_vViewerPosition;
  pr_TranslationVector = pr_TranslationVector * pr_ViewerRotationMatrix;
  pr_TranslationVector -= pr_vObjectHandle * pr_RotationMatrix;

  FLOAT2D vMin, vMax;

  if (ppr_fMetersPerPixel > 0.0f) {
    FLOAT fFactor = ppr_fViewerDistance / ppr_fMetersPerPixel;
    ppr_PerspectiveRatios(1) = -fFactor;
    ppr_PerspectiveRatios(2) = -fFactor;
    pr_ScreenCenter = -pr_ScreenBBox.Min();

    vMin = pr_ScreenBBox.Min();
    vMax = pr_ScreenBBox.Max();

  // Unified two 'else' blocks for normal or sub-drawport projection
  } else {
    FLOAT2D v2dScreenSize = pr_ScreenBBox.Size();
    pr_ScreenCenter = pr_ScreenBBox.Center();

    ANGLE aHalfHor;
    ANGLE aHalfVer;

    // Adjust FOV for wider resolutions (preserve vertical FOV instead of horizontal)
    if (_EnginePatches._bUseVerticalFOV) {
      // Calculate VFOV from HFOV on 4:3 resolution (e.g. 90 -> ~73.74)
      aHalfVer = ATan(Tan(ppr_FOVWidth * 0.5f) * 3.0f * pr_AspectRatio / 4.0f);

      // Recalculate HFOV from VFOV (e.g. ~73.74 -> 90 on 4:3 / ~106.26 on 16:9)
      aHalfHor = ATan(Tan(aHalfVer) * v2dScreenSize(1) * pr_AspectRatio / v2dScreenSize(2));

    } else {
      // Original function code
      aHalfHor = ppr_FOVWidth * 0.5f;
      aHalfVer = ATan(Tan(aHalfHor) * v2dScreenSize(2) * pr_AspectRatio / v2dScreenSize(1));
    }

    ppr_PerspectiveRatios(1) = -v2dScreenSize(1) / (2.0f * Tan(aHalfHor)) * pr_fViewStretch;
    ppr_PerspectiveRatios(2) = -v2dScreenSize(2) / (2.0f * Tan(aHalfVer)) * pr_fViewStretch;

    // For normal projection
    if (ppr_boxSubScreen.IsEmpty()) {
      vMin = pr_ScreenBBox.Min() - pr_ScreenCenter;
      vMax = pr_ScreenBBox.Max() - pr_ScreenCenter;

    // For sub-drawport projection
    } else {
      vMin = ppr_boxSubScreen.Min() - pr_ScreenCenter;
      vMax = ppr_boxSubScreen.Max() - pr_ScreenCenter;

      pr_ScreenCenter -= ppr_boxSubScreen.Min();
    }
  }

  const FLOAT fMinI = vMin(1); FLOAT fMinJ = vMin(2);
  const FLOAT fMaxI = vMax(1); FLOAT fMaxJ = vMax(2);
  const FLOAT &fRatioX = ppr_PerspectiveRatios(1);
  const FLOAT &fRatioY = ppr_PerspectiveRatios(2);

  const FLOAT fDZ = -1.0f;
  const FLOAT fDXL =  fDZ * fMinI / fRatioX;
  const FLOAT fDXR =  fDZ * fMaxI / fRatioX;
  const FLOAT fDYU = -fDZ * fMinJ / fRatioY;
  const FLOAT fDYD = -fDZ * fMaxJ / fRatioY;

  FLOAT fNLX = -fDZ;
  FLOAT fNLZ = +fDXL;
  FLOAT fOoNL = 1.0f / (FLOAT)sqrt(fNLX * fNLX + fNLZ * fNLZ);
  fNLX *= fOoNL; fNLZ *= fOoNL;

  FLOAT fNRX = +fDZ;
  FLOAT fNRZ = -fDXR;
  FLOAT fOoNR = 1.0f / (FLOAT)sqrt(fNRX * fNRX + fNRZ * fNRZ);
  fNRX *= fOoNR; fNRZ *= fOoNR;

  FLOAT fNDY = -fDZ;
  FLOAT fNDZ = +fDYD;
  FLOAT fOoND = 1.0f / (FLOAT)sqrt(fNDY * fNDY + fNDZ * fNDZ);
  fNDY *= fOoND; fNDZ *= fOoND;

  FLOAT fNUY = +fDZ;
  FLOAT fNUZ = -fDYU;
  FLOAT fOoNU = 1.0f / (FLOAT)sqrt(fNUY * fNUY + fNUZ * fNUZ);
  fNUY *= fOoNU; fNUZ *= fOoNU;

  pr_plClipU = FLOATplane3D(FLOAT3D(0.0f, fNUY, fNUZ), 0.0f);
  pr_plClipD = FLOATplane3D(FLOAT3D(0.0f, fNDY, fNDZ), 0.0f);
  pr_plClipL = FLOATplane3D(FLOAT3D(fNLX, 0.0f, fNLZ), 0.0f);
  pr_plClipR = FLOATplane3D(FLOAT3D(fNRX, 0.0f, fNRZ), 0.0f);

  pr_Prepared = TRUE;

  pr_fDepthBufferFactor = -pr_NearClipDistance;
  pr_fDepthBufferMul = pr_fDepthBufferFar - pr_fDepthBufferNear;
  pr_fDepthBufferAdd = pr_fDepthBufferNear;

  // Fix mip distances
  if (_EnginePatches._bUseVerticalFOV) {
    // Rely on height ratio instead of width ratio
    ppr_fMipRatio = pr_ScreenBBox.Size()(2) / (ppr_PerspectiveRatios(2) * 480.0f);

  } else {
    // Original function code
    ppr_fMipRatio = pr_ScreenBBox.Size()(1) / (ppr_PerspectiveRatios(1) * 640.0f);
  }
};
