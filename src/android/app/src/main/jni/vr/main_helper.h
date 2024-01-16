/*******************************************************************************

Filename    :   main_helper.h

Content     :   Used by other parts of the project to set the high-pri TID.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once
#include "OpenXR.h"
namespace vr
{

XrSession& GetSession();
void       PrioritizeTid(const int tid);
} // namespace vr
