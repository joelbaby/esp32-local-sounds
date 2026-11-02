// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <string>
namespace local_audio {
// A bounded canonical fingerprint suppresses repeated old STT during automatic
// resume. It is not a recognizer and stores no conversation text. ASCII letters
// are folded and punctuation ignored; UTF-8 bytes remain part of the key.
inline uint64_t TextFingerprint(const std::string& text) {
    uint64_t key = 14695981039346656037ull;
    bool any = false;
    for (unsigned char c : text) {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c < 128 && !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) continue;
        any = true;
        key = (key ^ c) * 1099511628211ull;
    }
    return any ? key : 0;
}
// No sample is changed. At 16 kHz, require 120 ms quiet followed by 60 ms
// sustained activity; this avoids expiring the guard on silence/noise alone.
// Existing AFE speech-onset callbacks provide the boundary when available.
class InputSpeechBoundary {
public:
    void Reset() { quiet_ = voiced_ = 0; armed_ = false; }
    bool Observe(const int16_t* pcm, size_t count) {
        if (!pcm || count == 0) return false;
        uint64_t magnitude = 0;
        for (size_t i = 0; i < count; ++i) {
            const int32_t value = pcm[i];
            magnitude += value < 0 ? -value : value;
        }
        const auto mean = magnitude / count;
        if (mean <= 128) {
            quiet_ = static_cast<uint32_t>(std::min<size_t>(1920, quiet_ + count));
            voiced_ = 0;
            if (quiet_ == 1920) armed_ = true;
        } else if (armed_ && mean >= 512) {
            voiced_ = static_cast<uint32_t>(std::min<size_t>(960, voiced_ + count));
            if (voiced_ == 960) { Reset(); return true; }
        } else {
            voiced_ = 0;
            if (!armed_) quiet_ = 0;
        }
        return false;
    }
private:
    uint32_t quiet_ = 0, voiced_ = 0;
    bool armed_ = false;
};

}
