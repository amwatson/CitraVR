/*******************************************************************************

Filename    :   PassthroughLayer.cpp
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "PassthroughLayer.h"

#include "../OpenXR.h"

#define DECL_PFN(pfn) PFN_##pfn pfn = nullptr
#define INIT_PFN(pfn)                                                          \
    OXR(xrGetInstanceProcAddr(OpenXr::GetInstance(), #pfn,                     \
                              (PFN_xrVoidFunction*)(&pfn)))

namespace
{

DECL_PFN(xrCreatePassthroughFB);
DECL_PFN(xrDestroyPassthroughFB);
DECL_PFN(xrPassthroughStartFB);
DECL_PFN(xrPassthroughPauseFB);
DECL_PFN(xrCreatePassthroughLayerFB);
DECL_PFN(xrDestroyPassthroughLayerFB);
DECL_PFN(xrPassthroughLayerPauseFB);
DECL_PFN(xrPassthroughLayerResumeFB);

void InitOXRFunctions()
{
    INIT_PFN(xrCreatePassthroughFB);
    INIT_PFN(xrDestroyPassthroughFB);
    INIT_PFN(xrPassthroughStartFB);
    INIT_PFN(xrPassthroughPauseFB);
    INIT_PFN(xrCreatePassthroughLayerFB);
    INIT_PFN(xrDestroyPassthroughLayerFB);
    INIT_PFN(xrPassthroughLayerPauseFB);
    INIT_PFN(xrPassthroughLayerResumeFB);
}

static bool gAreOXRFunctionsInitialized = false;

} // anonymous namespace

PassthroughLayer::PassthroughLayer(const XrSession& session)
{

    if (!gAreOXRFunctionsInitialized)
    {
        InitOXRFunctions();
        gAreOXRFunctionsInitialized = true;
    }

    const XrPassthroughCreateInfoFB ptci = {XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
    OXR(xrCreatePassthroughFB(session, &ptci, &mPassthrough));

    const XrPassthroughLayerCreateInfoFB plci = {
        XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB, nullptr, mPassthrough,
        XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB,
        XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB};
    OXR(xrCreatePassthroughLayerFB(session, &plci, &mPassthroughLayer));

    OXR(xrPassthroughStartFB(mPassthrough));
    OXR(xrPassthroughLayerResumeFB(mPassthroughLayer));
}

PassthroughLayer::~PassthroughLayer()
{
    OXR(xrPassthroughLayerPauseFB(mPassthroughLayer));
    OXR(xrPassthroughPauseFB(mPassthrough));
    OXR(xrDestroyPassthroughLayerFB(mPassthroughLayer));
    OXR(xrDestroyPassthroughFB(mPassthrough));

    mPassthroughLayer = XR_NULL_HANDLE;
    mPassthrough      = XR_NULL_HANDLE;
}

void PassthroughLayer::Frame(XrCompositionLayerPassthroughFB& layer) const
{
    layer.type        = XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB;
    layer.layerHandle = mPassthroughLayer;
    layer.flags       = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.space       = XR_NULL_HANDLE;
}
