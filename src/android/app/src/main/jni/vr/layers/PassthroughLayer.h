/*******************************************************************************

Filename    :   PassthroughLayer.h
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/
#pragma once

#include <openxr/openxr.h>

class PassthroughLayer {
public:
    PassthroughLayer(const XrSession& session);
    ~PassthroughLayer();
    void Frame(XrCompositionLayerPassthroughFB& layer) const;

private:
    XrPassthroughFB mPassthrough = XR_NULL_HANDLE;
    XrPassthroughLayerFB mPassthroughLayer = XR_NULL_HANDLE;
}; // class PassthroughLayer
