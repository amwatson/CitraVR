/*******************************************************************************

Filename    :   main_helper.h

Content     :   Used by other parts of the project to set the high-pri TID.

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once
#include "OpenXR.h"
namespace vr {

XrSession& GetSession();
void       PrioritizeTid(const int tid);
void       SetCitraReady();
// Records the tid of the frontend thread that presents the game surface
// (Choreographer/doFrame). The VR thread registers it with the XR scheduler
// once the session is running. Safe to call every frame; cheap store.
void SetPresentThreadTid(const int tid);
} // namespace vr
