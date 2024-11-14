// Copyright 2024 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <vector>
#include "common/common_types.h"
#include "common/hacks/hack_list.h"

namespace Common::Hacks {

enum class HackAllowMode {
    ALLOW,
    DISALLOW,
    FORCE,
};

struct HackEntry {
    HackAllowMode mode{};
    std::vector<u64> affected_title_ids{};
    UserHackData* hack_data{nullptr};
};

struct HackManager {
    const HackEntry* GetHack(const HackType& type, u64 title_id);

    HackAllowMode GetHackAllowMode(const HackType& type, u64 title_id,
                                   HackAllowMode default_mode = HackAllowMode::ALLOW) {
        const HackEntry* hack = GetHack(type, title_id);
        return (hack != nullptr) ? hack->mode : default_mode;
    }

    std::multimap<HackType, HackEntry> entries;
};

extern HackManager hack_manager;

} // namespace Common::Hacks