// SPDX-License-Identifier: MIT
#include "tone_noise_generator.h"
#include <algorithm>
#include "axel_f_phrase.h"
#include "brass_wavetables.h"

namespace tone_noise {
namespace {
// Generated from sin(2*pi*i/256), with linear interpolation. No borrowed table.
constexpr int16_t kSine[257] = {
    0,      804,    1608,   2410,   3212,   4011,   4808,   5602,   6393,   7179,   7962,   8739,
    9512,   10278,  11039,  11793,  12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
    18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,  23170,  23731,  24279,  24811,
    25329,  25832,  26319,  26790,  27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
    30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,  32137,  32285,  32412,  32521,
    32609,  32678,  32728,  32757,  32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
    32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,  30273,  29956,  29621,  29268,
    28898,  28510,  28105,  27683,  27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
    23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,  18204,  17530,  16846,  16151,
    15446,  14732,  14010,  13279,  12539,  11793,  11039,  10278,  9512,   8739,   7962,   7179,
    6393,   5602,   4808,   4011,   3212,   2410,   1608,   804,    0,      -804,   -1608,  -2410,
    -3212,  -4011,  -4808,  -5602,  -6393,  -7179,  -7962,  -8739,  -9512,  -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159,
    -20787, -21403, -22005, -22594, -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956, -30273, -30571, -30852, -31113,
    -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580,
    -31356, -31113, -30852, -30571, -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731, -23170, -22594, -22005, -21403,
    -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512,  -8739,  -7962,  -7179,  -6393,  -5602,  -4808,  -4011,
    -3212,  -2410,  -1608,  -804,   0};
constexpr double kPhaseScale = 18446744073709551616.0;
}  // namespace
const char* Generator::Validate(const Parameters& p, uint32_t rate) {
    if (rate < 16000 || rate > 48000)
        return "sample rate must be 16000..48000 Hz";
    if (p.kind != Kind::Tone && p.kind != Kind::Sweep && p.kind != Kind::Pink &&
        p.kind != Kind::Brass && p.kind != Kind::AxelF && p.kind != Kind::Ocean && p.kind != Kind::Saw)
        return "unknown sound kind";
    if (p.duration_ms != 0 && (p.duration_ms < 100 || p.duration_ms > 60000))
        return "duration must be 0 (bounded default) or 100..60000 ms";
    if (p.level_percent < 1 || p.level_percent > 100)
        return "level must be 1..100 percent; 100 uses current system volume";
    if ((p.kind == Kind::Tone || p.kind == Kind::Sweep || p.kind == Kind::Saw) &&
        (p.frequency_hz < 20 || p.frequency_hz > std::min<uint32_t>(11000, rate * 11 / 24)))
        return "frequency must be 20..min(11000,floor(sample_rate*11/24)) Hz";
    if (p.kind == Kind::Brass &&
        (p.frequency_hz < 55 || p.frequency_hz > std::min<uint32_t>(2000, rate / 8)))
        return "brass pitch must be 55..min(2000,sample_rate/8) Hz";
    if (p.kind == Kind::Sweep && (p.end_frequency_hz <= p.frequency_hz ||
                                  p.end_frequency_hz > std::min<uint32_t>(11000, rate * 11 / 24)))
        return "sweep end must be higher than start and within the sample-rate frequency ceiling";
    return nullptr;
}
bool Generator::Start(const Parameters& p, uint32_t rate, uint32_t seed) {
    if (Validate(p, rate))
        return false;
    *this = Generator();
    parameters_ = p;
    if (p.kind == Kind::AxelF) {
        const auto natural_samples =
            static_cast<uint32_t>(uint64_t(rate) * 60 * detail::kAxelTotalTicks /
                                  (detail::kAxelBpm * detail::kAxelTicksPerQuarter));
        const auto requested_samples =
            p.duration_ms == 0 ? natural_samples
                               : static_cast<uint32_t>(uint64_t(rate) * p.duration_ms / 1000);
        total_ = std::min(natural_samples, requested_samples);
    } else {
        const uint32_t duration =
            p.duration_ms == 0 ? (p.kind == Kind::Ocean ? 60000 : 5000) : p.duration_ms;
        total_ = static_cast<uint32_t>(uint64_t(rate) * duration / 1000);
    }
    ramp_ = rate / 200;  // 5 ms.
    random_ = seed ? seed : 1;
    if (p.kind == Kind::Tone || p.kind == Kind::Sweep || p.kind == Kind::Saw)
        increment_ = static_cast<uint64_t>(kPhaseScale * p.frequency_hz / rate);
    if (p.kind == Kind::Saw) {
        constexpr uint64_t limit = (uint64_t(1) << 32) * 45 / 100;
        while (brass_table_ < 6 && (increment_ >> 32) *
               detail::kBrassHarmonics[brass_table_ + 1] <= limit)
            ++brass_table_;
    }
    if (p.kind == Kind::Sweep)
        increment_step_ = static_cast<uint64_t>(
            kPhaseScale * (p.end_frequency_hz - p.frequency_hz) / rate / (total_ - 1));
    if (p.kind == Kind::Brass || p.kind == Kind::AxelF) {
        sample_rate_ = rate;
        brass_attack_ = rate / 200;
        brass_decay_ = rate * 150 / 1000;
        brass_release_ = rate / 100;
        brass_alpha_bright_ = static_cast<int32_t>(uint64_t(32768) * 28000 / (rate + 28000));
        brass_alpha_dark_ = static_cast<int32_t>(uint64_t(32768) * 5000 / (rate + 5000));
        if (p.kind == Kind::AxelF) {
            const auto& note = detail::kAxelPhrase[0];
            score_timeline_ticks_ = note.interval_ticks;
            next_note_sample_ =
                static_cast<uint32_t>(uint64_t(rate) * 60 * score_timeline_ticks_ /
                                      (detail::kAxelBpm * detail::kAxelTicksPerQuarter));
            StartBrassNote(note.frequency_hz, next_note_sample_ * 4 / 5);
        } else {
            StartBrassNote(p.frequency_hz, total_ - brass_release_);
        }
    }
    if (p.kind == Kind::Ocean) {
        sample_rate_ = rate;
        ocean_random_ = random_ ^ 0x9e3779b9u;
        if (!ocean_random_)
            ocean_random_ = 1;
        NextOceanCycle();
    }
    for (auto& row : rows_) {
        row = White();
        sum_ += row;
    }
    return true;
}
void Generator::StartBrassNote(uint32_t frequency_hz, uint32_t gate_samples) {
    const uint32_t center = static_cast<uint32_t>((uint64_t(frequency_hz) << 32) / sample_rate_);
    brass_step_[0] = center - center / 512;
    brass_step_[1] = center + center / 512;
    brass_table_ = 0;
    // Truncate against the highest detuned oscillator, with a 10% Nyquist margin.
    constexpr uint64_t phase_limit = (uint64_t(1) << 32) * 45 / 100;
    while (brass_table_ < 6 &&
           uint64_t(brass_step_[1]) * detail::kBrassHarmonics[brass_table_ + 1] <= phase_limit)
        ++brass_table_;
    brass_phase_[0] = brass_phase_[1] = 0;
    brass_filter_ = 0;
    brass_age_ = 0;
    brass_gate_ = gate_samples;
}
int32_t Generator::BrassHeldEnvelope(uint32_t age) const {
    if (age < brass_attack_)
        return static_cast<int32_t>(uint64_t(age) * 32767 / brass_attack_);
    if (age < brass_attack_ + brass_decay_)
        return 32767 - static_cast<int32_t>(uint64_t(age - brass_attack_) * 17767 / brass_decay_);
    return 15000;
}
int32_t Generator::RenderBrass() {
    if (parameters_.kind == Kind::AxelF && position_ >= next_note_sample_ &&
        score_index_ + 1u < detail::kAxelNoteCount) {
        const auto& note = detail::kAxelPhrase[++score_index_];
        score_timeline_ticks_ += note.interval_ticks;
        next_note_sample_ =
            static_cast<uint32_t>(uint64_t(sample_rate_) * 60 * score_timeline_ticks_ /
                                  (detail::kAxelBpm * detail::kAxelTicksPerQuarter));
        const uint32_t interval_samples = next_note_sample_ - position_;
        const uint32_t gate = score_index_ + 1u == detail::kAxelNoteCount
                                  ? interval_samples - brass_release_
                                  : interval_samples * 4 / 5;
        StartBrassNote(note.frequency_hz, gate);
    }
    if (brass_age_ >= brass_gate_ + brass_release_) {
        ++brass_age_;
        return 0;  // Score rests need no oscillator/filter work; next note retriggers state.
    }
    int32_t mix = 0;
    for (unsigned oscillator = 0; oscillator < 2; ++oscillator) {
        const uint32_t phase = brass_phase_[oscillator];
        const unsigned index = phase >> 24;
        const uint32_t fraction = (phase >> 8) & 0xffffu;
        const auto* table = detail::kBrassSaw[brass_table_];
        mix += table[index] +
               static_cast<int32_t>(int64_t(table[index + 1] - table[index]) * fraction / 65536);
        brass_phase_[oscillator] += brass_step_[oscillator];
    }
    mix /= 2;
    int32_t alpha = brass_alpha_dark_;
    if (brass_age_ < brass_decay_) {
        const uint32_t brightness = brass_decay_ - brass_age_;
        alpha += static_cast<int32_t>(int64_t(brass_alpha_bright_ - brass_alpha_dark_) *
                                      brightness / brass_decay_);
    }
    // A convex one-pole update preserves the table/mix amplitude bound.
    brass_filter_ += static_cast<int32_t>(int64_t(alpha) * (mix - brass_filter_) / 32768);
    int32_t amplitude = BrassHeldEnvelope(brass_age_);
    if (brass_age_ >= brass_gate_) {
        const uint32_t release_age = brass_age_ - brass_gate_;
        amplitude = release_age >= brass_release_
                        ? 0
                        : static_cast<int32_t>(int64_t(BrassHeldEnvelope(brass_gate_)) *
                                               (brass_release_ - release_age) / brass_release_);
    }
    ++brass_age_;
    return static_cast<int32_t>(int64_t(brass_filter_) * amplitude / 32767);
}
int32_t Generator::White() {
    random_ ^= random_ << 13;
    random_ ^= random_ >> 17;
    random_ ^= random_ << 5;
    return static_cast<int32_t>(random_ >> 16) - 32768;
}
void Generator::NextOceanCycle() {
    ocean_random_ ^= ocean_random_ << 13;
    ocean_random_ ^= ocean_random_ >> 17;
    ocean_random_ ^= ocean_random_ << 5;
    uint32_t samples =
        static_cast<uint32_t>(uint64_t(sample_rate_) * (8000 + ocean_random_ % 7001) / 1000);
    // Adjacent swells have different lengths; no fixed repeating surf loop.
    if (samples == ocean_cycle_)
        samples = samples == sample_rate_ * 15 ? sample_rate_ * 8 : samples + sample_rate_ / 1000;
    ocean_cycle_ = samples;
    ocean_position_ = 0;
}
int32_t Generator::RenderOcean(int32_t pink) {
    const uint32_t phase = static_cast<uint32_t>(uint64_t(ocean_position_) * 65536 / ocean_cycle_);
    const uint32_t triangle = phase <= 32768 ? phase : 65536 - phase;
    // Cubic smoothstep: zero slope at retreat and crest, continuous across cycles.
    const int32_t swell = static_cast<int32_t>(uint64_t(triangle) * triangle *
                                               (98304 - 2 * triangle) / (uint64_t(1) << 30));
    const int32_t gain = 4915 + swell * 22937 / 32768;  // 15..85%, before system volume.
    const int32_t cutoff = 180 + swell * 4820 / 32768;
    const int32_t alpha =
        static_cast<int32_t>(int64_t(32768) * 6 * cutoff / (sample_rate_ + 6 * cutoff));
    // Convex low-pass in Q15 keeps the filtered signal within the raw pink bounds.
    ocean_filter_ += static_cast<int32_t>((int64_t(pink) * 32768 - ocean_filter_) * alpha / 32768);
    if (++ocean_position_ == ocean_cycle_)
        NextOceanCycle();
    return static_cast<int32_t>(int64_t(ocean_filter_) * gain / (int64_t(32768) * 32768));
}
void Generator::Stop() {
    if (!active() || stop_total_ != 0)
        return;
    stop_left_ = stop_total_ = std::min(ramp_, total_ - position_);
}
size_t Generator::Render(int16_t* output, size_t capacity) {
    if (!output)
        return 0;
    size_t count = 0;
    while (count < capacity && active()) {
        int32_t value;
        if (parameters_.kind == Kind::Pink || parameters_.kind == Kind::Ocean) {
            ++counter_;
            // One octave row changes per sample. A trailing-zero count >= 10
            // leaves held rows alone. Independent white fills the top octave.
            uint32_t bits = counter_;
            unsigned row = 0;
            while (row < 10 && (bits & 1u) == 0) {
                ++row;
                bits >>= 1;
            }
            if (row < 10) {
                sum_ -= rows_[row];
                rows_[row] = White();
                sum_ += rows_[row];
            }
            value = (sum_ + White()) / 11;
            if (parameters_.kind == Kind::Ocean)
                value = RenderOcean(value);
        } else if (parameters_.kind == Kind::Brass || parameters_.kind == Kind::AxelF) {
            value = RenderBrass();
        } else {
            unsigned index = phase_ >> 24;
            uint32_t fraction = (phase_ >> 8) & 0xffffu;
            const auto* table = parameters_.kind == Kind::Saw ? detail::kBrassSaw[brass_table_] : kSine;
            value = table[index] +
                    static_cast<int32_t>(
                        (static_cast<int64_t>(table[index + 1] - table[index]) * fraction) / 65536);
            phase_ += static_cast<uint32_t>(increment_ >> 32);
            increment_ += increment_step_;
        }
        uint32_t envelope = std::min({ramp_, position_, total_ - 1 - position_});
        if (stop_total_)
            envelope = std::min(
                envelope, static_cast<uint32_t>(static_cast<uint64_t>(stop_left_ - 1) * ramp_ /
                                                std::max<uint32_t>(1, stop_total_ - 1)));
        output[count++] = static_cast<int16_t>(
            static_cast<int64_t>(value) * parameters_.level_percent * envelope / (100 * ramp_));
        ++position_;
        if (stop_total_)
            --stop_left_;
    }
    return count;
}
bool Playback::Request(const Parameters& p, uint32_t rate, uint32_t id) {
    if (Generator::Validate(p, rate) || id == 0)
        return false;
    pending_ = p;
    rate_ = rate;
    pending_id_ = id;
    if (active_) {
        generator_.Stop();
        state_ = State::Stopping;
    } else {
        active_ = true;
        state_ = State::Pending;
    }
    stopping_ = false;
    return true;
}
void Playback::Stop() {
    if (state_ == State::Pending)
        id_ = pending_id_;
    pending_id_ = 0;
    if (!active_)
        return;
    stopping_ = true;
    generator_.Stop();
    state_ = State::Stopping;
}
size_t Playback::Render(int16_t* output, size_t capacity) {
    if (!active_ || chunk_pending_)
        return 0;
    if (!generator_.active() && pending_id_) {
        generator_.Start(pending_, rate_);
        id_ = pending_id_;
        pending_id_ = 0;
        state_ = State::Playing;
    }
    size_t count = generator_.Render(output, capacity);
    chunk_pending_ = true;
    return count;
}
void Playback::Commit() {
    if (!chunk_pending_)
        return;
    chunk_pending_ = false;
    if (!generator_.active() && !pending_id_) {
        active_ = false;
        state_ = stopping_ ? State::Stopped : State::Completed;
    }
}
void Playback::Fail() {
    generator_ = Generator();
    pending_id_ = 0;
    chunk_pending_ = false;
    active_ = false;
    state_ = State::Error;
}
std::string Playback::StatusJson() const {
    return std::string("{\"state\":\"") + StateName(state_) +
           "\",\"request_id\":" + std::to_string(id_) +
           ",\"pending_id\":" + std::to_string(pending_id_) + "}";
}
const char* Playback::StateName(State state) {
    switch (state) {
        case State::Idle:
            return "idle";
        case State::Pending:
            return "pending";
        case State::Playing:
            return "playing";
        case State::Stopping:
            return "stopping";
        case State::Completed:
            return "completed";
        case State::Stopped:
            return "stopped";
        case State::Error:
            return "error";
    }
    return "unknown";
}
}  // namespace tone_noise
