// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include "local_audio_source.h"

namespace tone_noise {
enum class Kind { Tone, Sweep, Pink, Brass, AxelF, Ocean, Saw };
struct Parameters {
    Kind kind = Kind::Tone;
    uint32_t frequency_hz = 440;
    uint32_t end_frequency_hz = 4000;
    // 0: bounded default (5000 ms; Ocean 60000 ms; AxelF natural score end). Positive: 100..60000
    // ms.
    uint32_t duration_ms = 0;
    uint32_t level_percent = 100;  // 100: unity PCM gain, through current codec/system volume.
};

// Tone/sweep: 20..min(11000,floor(sample_rate*11/24)) Hz, below Nyquist.
// Brass: 55..2000 Hz, two oscillators symmetric about the requested pitch.
// Caller owns output storage. No allocation, locks, device, cloud, or RTOS dependency.
class Generator {
public:
    static const char* Validate(const Parameters& parameters, uint32_t sample_rate);
    bool Start(const Parameters& parameters, uint32_t sample_rate, uint32_t seed = 1);
    void Stop();  // Finish within 5 ms of further rendering, with a release ramp.
    size_t Render(int16_t* output, size_t capacity);
    bool active() const { return position_ < total_ && stop_left_ != 0; }
    uint32_t samples_rendered() const { return position_; }
    uint32_t total_samples() const { return total_; }

private:
    int32_t White();
    void NextOceanCycle();
    int32_t RenderOcean(int32_t pink);
    void StartBrassNote(uint32_t frequency_hz, uint32_t gate_samples);
    int32_t RenderBrass();
    int32_t BrassHeldEnvelope(uint32_t age) const;
    Parameters parameters_{};
    uint32_t total_ = 0, position_ = 0, ramp_ = 0;
    uint32_t stop_left_ = UINT32_MAX, stop_total_ = 0;
    uint32_t phase_ = 0, random_ = 1, counter_ = 0;
    uint64_t increment_ = 0, increment_step_ = 0;
    int32_t rows_[10] = {}, sum_ = 0;
    uint32_t ocean_random_ = 1, ocean_cycle_ = 0, ocean_position_ = 0;
    int32_t ocean_filter_ = 0;
    uint32_t brass_phase_[2] = {}, brass_step_[2] = {};
    uint32_t brass_age_ = 0, brass_gate_ = 0, sample_rate_ = 0;
    uint32_t brass_attack_ = 0, brass_decay_ = 0, brass_release_ = 0;
    uint32_t next_note_sample_ = 0, score_timeline_ticks_ = 0;
    int32_t brass_filter_ = 0, brass_alpha_bright_ = 0, brass_alpha_dark_ = 0;
    uint8_t brass_table_ = 0, score_index_ = 0;
};

// Externally serialized by the playback owner. Render is followed by Commit
// AFTER that chunk has been written. Latest replacement wins; one pending slot.
class Playback : public local_audio::Source {
public:
    enum class State { Idle, Pending, Playing, Stopping, Completed, Stopped, Error };
    struct Status {
        State state;
        uint32_t request_id;
        uint32_t pending_id;
    };
    bool Request(const Parameters& parameters, uint32_t sample_rate, uint32_t request_id);
    void Stop();
    size_t Render(int16_t* output, size_t capacity);
    void Commit();
    void Fail();  // Output driver reported a failed/short write; discard pending work.
    bool active() const { return active_; }
    bool failed() const override { return state_ == State::Error; }
    std::string StatusJson() const override;
    Status status() const { return {state_, id_, pending_id_}; }
    static const char* StateName(State state);

private:
    Generator generator_;
    Parameters pending_{};
    uint32_t rate_ = 0, pending_id_ = 0, id_ = 0;
    State state_ = State::Idle;
    bool active_ = false, stopping_ = false, chunk_pending_ = false;
};
}  // namespace tone_noise
