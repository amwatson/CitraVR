// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/settings.h"

class INIReader;

class Config {
private:
    std::unique_ptr<INIReader> android_config;
    std::unique_ptr<INIReader> per_game_config;
    std::string android_config_loc;
    u64 program_id{};

    bool LoadINI(const std::string& default_contents = "", bool retry = true);
    void ReadValues();
    void ApplyPerGameValues();
    bool HasCustomGlobalValue(const std::string& group, const std::string& key) const;
    long ResolvePerGameInteger(const std::string& title_section, const std::string& group,
                               const std::string& key, long current_value) const;
    bool ResolvePerGameBoolean(const std::string& title_section, const std::string& group,
                               const std::string& key, bool current_value) const;

public:
    explicit Config(u64 program_id = 0);
    ~Config();

    void Reload();

private:
    /**
     * Applies a value read from the android_config to a Setting.
     *
     * @param group The name of the INI group
     * @param setting The yuzu setting to modify
     */
    template <typename Type, bool ranged>
    void ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting);
};
