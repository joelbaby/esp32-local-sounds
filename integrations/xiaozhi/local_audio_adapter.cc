// SPDX-License-Identifier: MIT
#include "application.h"
#include "board.h"
#include "local_audio_session.h"

std::string Application::RequestLocalPlayback(local_audio::Source& source,
    std::function<bool(uint32_t, uint32_t)> configure) {
    std::lock_guard<std::mutex> lock(generated_command_mutex_);
    if ((local_active_source_ && local_active_source_ != &source) ||
        (generated_command_pending_ && local_command_source_ != &source))
        return "{\"state\":\"rejected\",\"error\":\"another local producer owns playback; stop it first\"}";
    if (++generated_request_id_ == 0) ++generated_request_id_;
    local_command_source_ = &source;
    local_configure_ = std::move(configure);
    generated_command_pending_ = true;
    generated_stop_pending_ = false;
    xEventGroupSetBits(event_group_, MAIN_EVENT_GENERATED_AUDIO);
    return "{\"state\":\"accepted\",\"request_id\":" + std::to_string(generated_request_id_) + "}";
}
std::string Application::StopLocalPlayback(local_audio::Source* expected) {
    std::lock_guard<std::mutex> lock(generated_command_mutex_);
    const bool pending_match = generated_command_pending_ &&
                               (!expected || local_command_source_ == expected);
    const bool active_match = local_active_source_ &&
                              (!expected || local_active_source_ == expected);
    if (expected && !pending_match && !active_match)
        return "{\"state\":\"not_owned\"}";
    if (!expected || pending_match) generated_command_pending_ = false;
    if (!expected || active_match) generated_stop_pending_ = true;
    xEventGroupSetBits(event_group_, MAIN_EVENT_GENERATED_AUDIO);
    return "{\"state\":\"stop_requested\"}";
}
std::string Application::LocalPlaybackStatus(const local_audio::Source& source) {
    std::lock_guard<std::mutex> lock(generated_command_mutex_);
    if (generated_command_pending_ && local_command_source_ == &source)
        return "{\"state\":\"accepted\",\"request_id\":" + std::to_string(generated_request_id_) +
               "}";
    if (generated_stop_pending_ && local_active_source_ == &source)
        return "{\"state\":\"stop_requested\"}";
    if (!generated_error_.empty() &&
        (local_command_source_ == &source || local_active_source_ == &source))
        return "{\"state\":\"rejected\",\"error\":\"" + generated_error_ + "\"}";
    return audio_service_.LocalAudioStatus(source);
}
void Application::HandleLocalPlaybackEvent() {
    std::unique_lock<std::mutex> lock(generated_command_mutex_);
    if (generated_stop_pending_) {
        generated_stop_pending_ = false;
        if (local_active_source_) audio_service_.StopLocalAudio(local_active_source_);
    }
    if (generated_command_pending_) {
        generated_command_pending_ = false;
        generated_error_.clear();
        auto state = GetDeviceState();
        if (state == kDeviceStateConnecting && local_awaiting_user_.load() &&
            generated_resume_allowed_.load()) {
            // A queued local replacement supersedes our not-yet-opened resume.
            SetDeviceState(kDeviceStateIdle);
            state = kDeviceStateIdle;
        }
        if (state != kDeviceStateIdle && state != kDeviceStateListening &&
            state != kDeviceStateSpeaking) {
            generated_error_ = "device busy; try when idle or in conversation";
        } else {
            if (!generated_audio_session_.load()) {
                // Publish suppression before abort/reset, including queued TTS callbacks.
                generated_audio_epoch_.fetch_add(1);
                generated_audio_session_.store(true);
                generated_resume_allowed_.store(last_error_message_.empty());
                local_guard_text_ = last_recognized_turn_ == recognized_user_turn_.load()
                                        ? last_recognized_text_
                                        : 0;
                pending_listening_start_ = false;
                play_popup_on_listening_ = false;
                if (protocol_ && protocol_->IsAudioChannelOpened()) {
                    // STT already finalized an automatic server turn. Stopping it
                    // again may create a redundant empty recognition cycle.
                    if (state == kDeviceStateListening &&
                        audio_service_.IsAudioProcessorRunning() &&
                        finalized_user_turn_.load() != recognized_user_turn_.load()) {
                        protocol_->SendStopListening();
                    }
                    if ((state == kDeviceStateSpeaking ||
                         (state == kDeviceStateListening &&
                          finalized_user_turn_.load() == recognized_user_turn_.load()))) {
                        AbortSpeaking(kAbortReasonNone);
                    }
                }
                audio_service_.EnableVoiceProcessing(false);
                audio_service_.EnableWakeWordDetection(false);
                while (audio_service_.PopPacketFromSendQueue()) {
                }
                SetDeviceState(kDeviceStateSpeaking);
            }
            const bool started = local_command_source_ && audio_service_.ControlLocalAudio(
                *local_command_source_, [this]() {
                    return local_configure_ && local_configure_(generated_request_id_,
                        Board::GetInstance().GetAudioCodec()->output_sample_rate());
                });
            if (started) local_active_source_ = local_command_source_;
            if (!started) {
                generated_error_ = "audio service unavailable";
            }
        }
    }
    if (generated_audio_session_.load() && !audio_service_.IsLocalAudioActive()) {
        // Publish idle while both suppression guards still hold. Otherwise a
        // network callback could observe speaking with local ownership released.
        const auto state = GetDeviceState();
        const bool restore_idle = state == kDeviceStateSpeaking || state == kDeviceStateIdle ||
                                  state == kDeviceStateListening;
        if (restore_idle)
            SetDeviceState(kDeviceStateIdle);
        const bool failed = audio_service_.LocalAudioFailed();
        audio_service_.ReleaseLocalAudio(local_active_source_);
        local_active_source_ = nullptr;
        local_awaiting_user_.store(true);
        generated_audio_epoch_.fetch_add(1);
        generated_audio_session_.store(false);
        if (!restore_idle)
            return;
        // Stay quiet through queued/late speech while awaiting a new user utterance.
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.EnableWakeWordDetection(true);
        const bool resume = generated_resume_allowed_.load() && generated_error_.empty() &&
                            !failed;
        const uint32_t epoch = generated_audio_epoch_.load();
        lock.unlock();
        if (resume)
            Schedule([this, epoch]() { ResumeListeningAfterLocalPlayback(epoch); });
    }
    if (lock.owns_lock()) {
        if (!generated_audio_session_.load() && local_active_source_ &&
            !generated_command_pending_ && !audio_service_.IsLocalAudioActive()) {
            audio_service_.ReleaseLocalAudio(local_active_source_);
            local_active_source_ = nullptr;
        }
        lock.unlock();
    }
}
void Application::ResumeListeningAfterLocalPlayback(uint32_t epoch) {
    // Main-loop continuation. Never hold the command mutex across protocol open.
    {
        std::lock_guard<std::mutex> lock(generated_command_mutex_);
        if (generated_command_pending_ || generated_audio_session_.load())
            return;
    }
    if (!generated_resume_allowed_.load() || generated_audio_epoch_.load() != epoch ||
        GetDeviceState() != kDeviceStateIdle || !protocol_)
        return;
    const auto mode = GetDefaultListeningMode();
    if (!protocol_->IsAudioChannelOpened()) {
        SetDeviceState(kDeviceStateConnecting);
        Schedule([this, epoch, mode]() {
            {
                std::lock_guard<std::mutex> lock(generated_command_mutex_);
                if (generated_command_pending_ || generated_audio_session_.load())
                    return;
            }
            if (generated_audio_epoch_.load() == epoch && !generated_audio_session_.load() &&
                GetDeviceState() == kDeviceStateConnecting &&
                (!generated_resume_allowed_.load() || !protocol_)) {
                SetDeviceState(kDeviceStateIdle);
                audio_service_.EnableVoiceProcessing(false);
                audio_service_.EnableWakeWordDetection(true);
                return;
            }
            if (!generated_resume_allowed_.load() || generated_audio_epoch_.load() != epoch ||
                generated_audio_session_.load() || GetDeviceState() != kDeviceStateConnecting ||
                !protocol_)
                return;
            generated_auto_listening_transition_ = true;
            ContinueOpenAudioChannel(mode);
            generated_auto_listening_transition_ = false;
            if (!generated_resume_allowed_.load() && GetDeviceState() == kDeviceStateListening)
                SetDeviceState(kDeviceStateIdle);
            if (GetDeviceState() == kDeviceStateIdle) {
                audio_service_.EnableVoiceProcessing(false);
                audio_service_.EnableWakeWordDetection(true);
            }
        });
        return;
    }
    generated_auto_listening_transition_ = true;
    SetListeningMode(mode);
    generated_auto_listening_transition_ = false;
}

void Application::HandleLocalUserText(const std::string& text, uint32_t epoch) {
    if (generated_audio_session_.load() || generated_audio_epoch_.load() != epoch) return;
    const uint64_t key = local_audio::TextFingerprint(text);
    if (local_awaiting_user_.load()) {
        if (GetDeviceState() != kDeviceStateListening || !protocol_ ||
            !protocol_->IsAudioChannelOpened() || !audio_service_.IsAudioProcessorRunning() ||
            key == 0 || (local_guard_text_ != 0 && key == local_guard_text_ &&
                         local_fresh_voice_epoch_.load() != epoch)) return;
        local_awaiting_user_.store(false);
    }
    finalized_user_turn_.store(recognized_user_turn_.load());
    last_recognized_text_ = key;
    last_recognized_turn_ = recognized_user_turn_.load();
}

void Application::ObserveLocalVoiceActivity() {
    const auto epoch = generated_audio_epoch_.load();
    if (!generated_audio_session_.load() && local_awaiting_user_.load() &&
        audio_service_.IsAudioProcessorRunning())
        local_fresh_voice_epoch_.store(epoch);
}

void Application::ObserveLocalInputActivity(const std::vector<int16_t>& pcm) {
    // Audio-engine output callback is serialized on the input task. These
    // observer fields are never mutated from the main/protocol task.
    const auto epoch = generated_audio_epoch_.load();
    if (local_observed_input_epoch_ != epoch) {
        local_input_boundary_.Reset();
        local_observed_input_epoch_ = epoch;
    }
    if (generated_audio_session_.load() || !local_awaiting_user_.load() ||
        !audio_service_.IsAudioProcessorRunning()) return;
    if (local_input_boundary_.Observe(pcm.data(), pcm.size()) &&
        generated_audio_epoch_.load() == epoch)
        local_fresh_voice_epoch_.store(epoch);
}
