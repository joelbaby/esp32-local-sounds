// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace local_audio {
// Implemented by independently installed local producers. Calls are serialized
// by AudioService's queue mutex; only its output task renders and commits.
// Source storage must outlive ownership, including an in-flight driver write.
class Source {
public:
    virtual ~Source() = default;
    virtual size_t Render(int16_t* output, size_t capacity) = 0;
    virtual void Commit() = 0; // Only after a complete successful driver write.
    virtual void Stop() = 0;   // Bounded graceful cancellation.
    virtual void Fail() = 0;   // Drop pending work; never report completion.
    virtual bool active() const = 0;
    virtual bool failed() const = 0;
    virtual std::string StatusJson() const = 0;
};
} // namespace local_audio
