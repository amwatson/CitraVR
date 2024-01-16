/*******************************************************************************

Filename    :   Swapchain.h

Content     :   A Swapchain.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once
#include <openxr/openxr.h>

struct Swapchain
{
    XrSwapchain Handle;
    uint32_t    Width;
    uint32_t    Height;
};
