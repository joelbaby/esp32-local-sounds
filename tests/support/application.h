#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <deque>
#include "tone_noise_generator.h"
#include "local_audio_session.h"
#define MAIN_EVENT_GENERATED_AUDIO 32768
inline unsigned events = 0;
inline void xEventGroupSetBits(int, unsigned bits) { events |= bits; }
enum DeviceState {
    kDeviceStateIdle,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateConnecting,
    kDeviceStateUpgrading
};
enum AbortReason { kAbortReasonNone };
enum ListeningMode { kListeningModeAutoStop };
struct Protocol {
    bool open = true;
    unsigned stops = 0, starts = 0, opens = 0;
    std::function<void()> on_open, on_start;
    bool IsAudioChannelOpened() { return open; }
    void SendStopListening() { ++stops; }
};
struct AudioService {
    tone_noise::Playback playback;
    bool mic = true, wake = true, available = true;
    unsigned resets = 0;
    void EnableVoiceProcessing(bool b) { mic = b; }
    void EnableWakeWordDetection(bool b) { wake = b; }
    void* PopPacketFromSendQueue() { return nullptr; }
    local_audio::Source* owner = nullptr;
    bool ControlLocalAudio(local_audio::Source& source, const std::function<bool()>& update) {
        ++resets;
        if (!available || (owner && owner != &source) || !update()) return false;
        owner = &source;
        return true;
    }
    bool IsAudioProcessorRunning() { return mic; }
    void StopLocalAudio(local_audio::Source* expected = nullptr) {
        if (owner && (!expected || expected == owner)) owner->Stop();
    }
    bool LocalAudioFailed() { return owner && owner->failed(); }
    std::function<void()> on_release;
    void ReleaseLocalAudio(local_audio::Source* expected = nullptr) {
        if (expected && owner != expected) return;
        if (on_release) on_release();
        owner = nullptr;
    }
    bool IsLocalAudioActive() { return owner && owner->active(); }
    std::string LocalAudioStatus(const local_audio::Source& source) { return source.StatusJson(); }
    void OutputChunk() {
        int16_t pcm[240];
        if (owner) { owner->Render(pcm, 240); owner->Commit(); }
    }

};
class Application {
public:
    static Application& GetInstance() {
        static Application app;
        return app;
    }
    std::string RequestGeneratedAudio(const tone_noise::Parameters&);
    std::string StopGeneratedAudio();
    std::string GeneratedAudioStatus();
    void HandleLocalPlaybackEvent();
    void HandleGeneratedAudioEvent() { HandleLocalPlaybackEvent(); }
    std::string RequestLocalPlayback(local_audio::Source&, std::function<bool(uint32_t,uint32_t)>);
    std::string StopLocalPlayback(local_audio::Source* expected = nullptr);
    std::string LocalPlaybackStatus(const local_audio::Source&);
    local_audio::Source* local_command_source_ = nullptr;
    local_audio::Source* local_active_source_ = nullptr;
    std::function<bool(uint32_t,uint32_t)> local_configure_;
    void ResumeListeningAfterLocalPlayback(uint32_t);
    void ObserveLocalVoiceActivity();
    void ObserveLocalInputActivity(const std::vector<int16_t>& pcm);
    std::atomic<uint32_t> local_fresh_voice_epoch_{UINT32_MAX};
    uint32_t local_observed_input_epoch_ = UINT32_MAX;
    local_audio::InputSpeechBoundary local_input_boundary_;
    void HandleLocalUserText(const std::string&, uint32_t);
    std::deque<std::function<void()>> tasks;
    void Schedule(std::function<void()> fn) { tasks.push_back(std::move(fn)); }
    void Pump() { while (!tasks.empty()) { auto fn = std::move(tasks.front()); tasks.pop_front(); fn(); } }
    ListeningMode GetDefaultListeningMode() { return kListeningModeAutoStop; }
    void SetListeningMode(ListeningMode) {
        if (!protocol_) return;
        state = kDeviceStateListening;
        if (!generated_auto_listening_transition_) local_awaiting_user_.store(false);
        ++protocol_->starts;
        if (protocol_->on_start) protocol_->on_start();
        if (generated_resume_allowed_) audio_service_.mic = true;
    }
    void ContinueOpenAudioChannel(ListeningMode mode) {
        ++protocol_->opens;
        if (protocol_->on_open) protocol_->on_open();
        if (!generated_resume_allowed_) return;
        protocol_->open = true;
        SetListeningMode(mode);
    }
    DeviceState GetDeviceState() { return state; }
    bool SetDeviceState(DeviceState s) {
        state = s;
        return true;
    }
    void AbortSpeaking(AbortReason) { ++aborts; }
    // Test fixture controls stand in for the real RTOS/Protocol/Board only.
    DeviceState state = kDeviceStateListening;
    unsigned aborts = 0;
    std::mutex generated_command_mutex_;

    tone_noise::Playback generated_playback_;
    std::atomic<bool> generated_resume_allowed_{false}, local_awaiting_user_{false};
    bool generated_auto_listening_transition_ = false;
    std::atomic<uint32_t> recognized_user_turn_{0}, finalized_user_turn_{UINT32_MAX};
    uint64_t local_guard_text_ = 0, last_recognized_text_ = 0;
    uint32_t last_recognized_turn_ = UINT32_MAX;
    std::string last_error_message_;
    uint32_t generated_request_id_ = 0;
    bool generated_command_pending_ = false, generated_stop_pending_ = false;
    std::atomic<bool> generated_audio_session_{false};
    std::atomic<uint32_t> generated_audio_epoch_{0};
    std::string generated_error_;
    int event_group_ = 1;
    bool pending_listening_start_ = true, play_popup_on_listening_ = true;
    std::unique_ptr<Protocol> protocol_ = std::make_unique<Protocol>();
    AudioService audio_service_;
};
