/*******************************************************************************

Filename    :   SyspropUtils.h

Content     :   Utility functions for accessing system properties

Authors     :   Amanda M. Watson
License     :   Licensed under GPLv2 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <sys/system_properties.h>

namespace SyspropUtils {
static inline float GetSysPropAsFloat(const char* propertyName, const float defaultValue) {
    char value[PROP_VALUE_MAX];
    int32_t length = __system_property_get(propertyName, value);

    if (length > 0) {
        // Attempt to convert the string to a float
        char* end;
        float float_value = std::strtof(value, &end);

        // Check if the conversion was successful (end points to the end of the
        // number)
        if (end != value) {
            return float_value;
        }
    }

    // Return default value if property is not set or conversion failed
    return defaultValue;
}

static inline int32_t GetSysPropAsInt(const char* propertyName, const int32_t defaultValue) {
    char value[PROP_VALUE_MAX];
    int32_t length = __system_property_get(propertyName, value);

    if (length > 0) {
        // Attempt to convert the string to an int
        char* end;
        int32_t int_value = std::strtol(value, &end, 10);

        // Check if the conversion was successful (end points to the end of the
        // number)
        if (end != value) {
            return int_value;
        }
    }

    // Return default value if property is not set or conversion failed
    return defaultValue;
}

} // namespace SyspropUtils
