// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <streambuf>

#include "common/common_types.h"

namespace Common {

/**
 * Output stream buffer that writes into one contiguous, growable block of memory.
 *
 * std::ostringstream is slow for very large outputs such as save states: it grows by repeatedly
 * reallocating and copying, and str() then makes one more full copy. This buffer grows by
 * doubling at most, and Data() gives direct access to the written bytes. Capacity that is reserved
 * but never written is not touched, so reserving generously is cheap.
 */
class MemoryOutputStreamBuf final : public std::streambuf {
public:
    explicit MemoryOutputStreamBuf(std::size_t initial_capacity = 0) {
        Reserve(std::max<std::size_t>(initial_capacity, MIN_CAPACITY));
    }

    /// The bytes written so far. Invalidated by further writes.
    std::span<const u8> Data() const {
        return {reinterpret_cast<const u8*>(pbase()), static_cast<std::size_t>(pptr() - pbase())};
    }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }
        Reserve(Used() + 1);
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
        return ch;
    }

    std::streamsize xsputn(const char* s, std::streamsize count) override {
        if (count <= 0) {
            return 0;
        }
        Reserve(Used() + static_cast<std::size_t>(count));
        std::memcpy(pptr(), s, static_cast<std::size_t>(count));
        pbump(static_cast<int>(count));
        return count;
    }

private:
    static constexpr std::size_t MIN_CAPACITY = 4096;

    std::size_t Used() const {
        return static_cast<std::size_t>(pptr() - pbase());
    }

    /// Makes room for at least `required` bytes in total.
    void Reserve(std::size_t required) {
        if (required <= capacity) {
            return;
        }
        // The put area is addressed with an int offset.
        if (required > static_cast<std::size_t>(INT_MAX)) {
            throw std::length_error("MemoryOutputStreamBuf is limited to 2 GiB");
        }
        const std::size_t new_capacity =
            std::min<std::size_t>(std::max(required, capacity * 2), INT_MAX);
        // Not value-initialized on purpose: untouched pages are never faulted in.
        auto new_data = std::make_unique_for_overwrite<char[]>(new_capacity);
        const std::size_t used = data ? Used() : 0;
        if (used) {
            std::memcpy(new_data.get(), data.get(), used);
        }
        data = std::move(new_data);
        capacity = new_capacity;
        setp(data.get(), data.get() + capacity);
        pbump(static_cast<int>(used));
    }

    std::unique_ptr<char[]> data;
    std::size_t capacity = 0;
};

/**
 * Input stream buffer that reads directly from a block of memory without copying it, unlike
 * std::istringstream which takes its own copy of the whole string.
 * The memory must outlive the buffer.
 */
class MemoryInputStreamBuf final : public std::streambuf {
public:
    explicit MemoryInputStreamBuf(std::span<const u8> source) {
        // The get area is never written through.
        auto* begin = const_cast<char*>(reinterpret_cast<const char*>(source.data()));
        setg(begin, begin, begin + source.size());
    }
};

} // namespace Common
