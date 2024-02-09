/*******************************************************************************

Filename    :   Swapchain.h

Content     :   A Swapchain.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once
#include <openxr/openxr.h>

struct Swapchain
{
    XrSwapchain mHandle;
    uint32_t    mWidth;
    uint32_t    mHeight;
};
