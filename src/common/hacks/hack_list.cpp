// Copyright 2024 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/hacks/hack_manager.h"

namespace Common::Hacks {

HackManager hack_manager = {
    .entries = {

        // The following games cannot use the right eye disable hack due to the way they
        // handle rendering.
        {HackType::RIGHT_EYE_DISABLE,
         HackEntry{
             .mode = HackAllowMode::DISALLOW,
             .affected_title_ids =
                 {
                     // Luigi's Mansion
                     0x00040000001D1900,
                     0x00040000001D1A00,

                     // Luigi's Mansion 2
                     0x0004000000055F00,
                     0x0004000000076500,
                     0x0004000000076400,
                     0x00040000000D0000,

                     // Rayman Origins
                     0x000400000005A500,
                     0x0004000000084400,
                     0x0004000000057600,
                 },
         }},

        // The following games require accurate multiplication to render properly.
        {HackType::ACCURATE_MULTIPLICATION,
         HackEntry{
             .mode = HackAllowMode::FORCE,
             .affected_title_ids =
                 {
                     // The Legend of Zelda: Ocarina of Time 3D
                     0x0004000000033400, // JAP
                     0x0004000000033500, // USA
                     0x0004000000033600, // EUR
                     0x000400000008F800, // KOR
                     0x000400000008F900, // CHI

                     // Mario & Luigi: Superstar Saga + Bowsers Minions
                     0x00040000001B8F00, // USA
                     0x00040000001B9000, // EUR
                     0x0004000000194B00, // JAP

                     // Mario & Luigi: Bowsers Inside Story + Bowser Jrs Journey
                     0x00040000001D1400, // USA
                     0x00040000001D1500, // EUR
                     0x00040000001CA900, // JAP
                 },
         }},

        {HackType::DECRYPTION_AUTHORIZED,
         HackEntry{
             .mode = HackAllowMode::ALLOW,
             .affected_title_ids =
                 {
                     // NIM
                     0x0004013000002C02, // Normal
                     0x0004013000002C03, // Safe mode
                     0x0004013020002C03, // New 3DS safe mode
                 },
         }},

    }};
}