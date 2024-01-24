/*******************************************************************************

Filename    :   vr_settings.h

Content     :   VR-specific settings initialized in config.cpp

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#pragma once

#include "OpenXR.h"

#include <string>

// Note: update this if CPU levels are added to OpenXR
#define XR_HIGHEST_CPU_PERF_LEVEL XR_PERF_SETTINGS_LEVEL_BOOST_EXT
#define XR_HIGHEST_CPU_PREFERENCE 4 // corresponds to XR_HIGHEST_CPU_PERF_LEVEL in quest logging

namespace VRSettings {

enum class HMDType { UNKNOWN = 0, QUEST1, QUEST2, QUEST3, QUESTPRO };
enum class VREnvironmentType { PASSTHROUGH = 1, VOID = 2 };

// Given a CPU level preference, return the corresponding OpenXR performance level
XrPerfSettingsLevelEXT CPUPrefToPerfSettingsLevel(const int32_t cpu_level_pref);
// Return the HMD type as a string (queried from ro.product.model)
std::string GetHMDTypeStr();
// Given a string from GetHMDTypeStr(), return the corresponding HMDType
HMDType HmdTypeFromStr(const std::string& hmdType);

struct Values {
    bool extra_performance_mode_enabled = false;
    int32_t vr_environment = 0;
    XrPerfSettingsLevelEXT cpu_level = XR_HIGHEST_CPU_PERF_LEVEL;
    uint32_t resolution_factor = 0;
    HMDType hmd_type = HMDType::UNKNOWN;
} extern values;

} // namespace VRSettings
