/*******************************************************************************

Filename    :   vr_settings.cpp
Content     :   VR-specific settings initialized in config.cpp

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "vr_settings.h"

#include "utils/LogUtils.h"
#include "utils/SyspropUtils.h"

#include <string>

namespace VRSettings
{
Values values = {};

HMDType HmdTypeFromStr(const std::string& hmdType)
{
    if (hmdType == "Quest") { return HMDType::QUEST1; }
    else if (hmdType == "Quest" || hmdType == "Quest 2" || hmdType == "Miramar")
    {
        return HMDType::QUEST2;
    }
    else if (hmdType == "Quest 3") { return HMDType::QUEST3; }
    else if (hmdType == "Quest Pro") { return HMDType::QUESTPRO; }
    return HMDType::UNKNOWN;
}

std::string GetHMDTypeStr()
{
    return SyspropUtils::GetSysPropAsString("ro.product.model", "Unknown");
}

XrPerfSettingsLevelEXT CPUPrefToPerfSettingsLevel(const int32_t cpu_level_pref)
{
    switch (cpu_level_pref)
    {
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

} // namespace VRSettings
