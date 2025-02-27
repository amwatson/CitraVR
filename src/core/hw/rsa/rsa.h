// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <vector>
#include "common/common_types.h"

namespace HW::RSA {

class RsaSlot {
public:
    RsaSlot() = default;
    RsaSlot(std::vector<u8> exponent, std::vector<u8> modulus)
        : init(true), exponent(std::move(exponent)), modulus(std::move(modulus)) {}

    std::vector<u8> ModularExponentiation(std::span<const u8> message,
                                          int out_size_bytes = -1) const;

    std::vector<u8> Sign(std::span<const u8> message) const;

    bool Verify(std::span<const u8> message, std::span<const u8> signature) const;

    explicit operator bool() const {
        // TODO(B3N30): Maybe check if exponent and modulus are vailid
        return init;
    }

    void SetExponent(const std::vector<u8>& e) {
        exponent = e;
    }

    const std::vector<u8>& GetExponent() const {
        return exponent;
    }

    void SetModulus(const std::vector<u8>& m) {
        modulus = m;
    }

    const std::vector<u8>& GetModulus() const {
        return modulus;
    }

    void SetPrivateD(const std::vector<u8>& d) {
        private_d = d;
    }

    const std::vector<u8>& GetPrivateD() const {
        return private_d;
    }

private:
    bool init = false;
    std::vector<u8> exponent;
    std::vector<u8> modulus;
    std::vector<u8> private_d;
};

void InitSlots();

const RsaSlot& GetSlot(std::size_t slot_id);

const RsaSlot& GetTicketWrapSlot();

const RsaSlot& GetSecureInfoSlot();
const RsaSlot& GetLocalFriendCodeSeedSlot();

} // namespace HW::RSA
