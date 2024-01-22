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
#include "utils/LogUtils.h"

#include <string>

// Note: update this if CPU levels are added to OpenXR
#define XR_HIGHEST_CPU_PERF_LEVEL XR_PERF_SETTINGS_LEVEL_BOOST_EXT
#define XR_HIGHEST_CPU_PREFERENCE 4 // corresponds to XR_HIGHEST_CPU_PERF_LEVEL in quest logging

namespace VRSettings {

  static inline XrPerfSettingsLevelEXT CPUPrefToPerfSettingsLevel(const int32_t cpu_level_pref) {
    switch (cpu_level_pref) {
      case 1:
        return XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
      case 2:
        return XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
      case 3:
        return XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
      case 4:
        return XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
      default:
        FAIL("Invalid CPU level preference %d", cpu_level_pref);
    }
  }

struct Values {
  bool extra_performance_mode_enabled = false;
  int32_t vr_environment = 0;
  XrPerfSettingsLevelEXT cpu_level = XR_HIGHEST_CPU_PERF_LEVEL;
} extern values;

} // namespace VRSettings
