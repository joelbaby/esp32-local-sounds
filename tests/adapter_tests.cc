// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include "application.h"
#include "board.h"
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
            std::abort();                                        \
        }                                                        \
    } while (0)
int main() {
    Application app;
    auto& system_codec = Board::GetInstance().codec;
    system_codec.volume = 37;
    system_codec.volume_set_calls = 0;
    unsigned releases = 0;
    app.audio_service_.on_release = [&]() {
        CHECK(app.generated_audio_session_);
        CHECK(app.state == kDeviceStateIdle);
        ++releases;
    };
    tone_noise::Parameters p;
    p.duration_ms = 100;
    CHECK(app.RequestGeneratedAudio(p).find("accepted") != std::string::npos);
    CHECK(app.audio_service_.mic);
    CHECK(!app.generated_playback_.active());
    CHECK(app.GeneratedAudioStatus().find("accepted") != std::string::npos);
    app.HandleGeneratedAudioEvent();
    CHECK(app.state == kDeviceStateSpeaking);
    CHECK(!app.audio_service_.mic && !app.audio_service_.wake);
    CHECK(app.aborts == 0);
    CHECK(app.generated_audio_epoch_ == 1);
    app.audio_service_.OutputChunk();
    CHECK(app.GeneratedAudioStatus().find("playing") != std::string::npos);
    app.RequestGeneratedAudio(p);
    app.RequestGeneratedAudio(p);
    app.HandleGeneratedAudioEvent();
    CHECK(app.aborts == 0);
    CHECK(app.generated_playback_.status().pending_id == 3);
    app.audio_service_.OutputChunk();
    app.audio_service_.OutputChunk();
    CHECK(app.generated_playback_.status().request_id == 3);
    CHECK(app.StopGeneratedAudio().find("stop_requested") != std::string::npos);
    app.HandleGeneratedAudioEvent();
    CHECK(app.generated_audio_session_);
    app.audio_service_.OutputChunk();
    app.HandleGeneratedAudioEvent();
    CHECK(!app.generated_audio_session_);
    CHECK(app.state == kDeviceStateIdle);
    CHECK(!app.audio_service_.mic && app.audio_service_.wake);
    CHECK(app.GeneratedAudioStatus().find("stopped") != std::string::npos);
    app.RequestGeneratedAudio(p);
    app.HandleGeneratedAudioEvent();
    while (app.audio_service_.IsLocalAudioActive())
        app.audio_service_.OutputChunk();
    app.HandleGeneratedAudioEvent();
    CHECK(app.state == kDeviceStateIdle);
    CHECK(app.audio_service_.wake);
    CHECK(app.GeneratedAudioStatus().find("completed") != std::string::npos);
    app.RequestGeneratedAudio(p);
    app.HandleGeneratedAudioEvent();
    app.generated_playback_.Fail();
    app.HandleGeneratedAudioEvent();
    CHECK(!app.generated_audio_session_);
    CHECK(app.audio_service_.wake && !app.audio_service_.mic);
    CHECK(app.GeneratedAudioStatus().find("error") != std::string::npos);
    CHECK(releases >= 3);
    // A non-conversation state must be preserved, while ownership is still released.
    app.audio_service_.on_release = [&]() {
        CHECK(app.state == kDeviceStateUpgrading);
        CHECK(app.generated_audio_session_);
    };
    app.RequestGeneratedAudio(p);
    app.HandleGeneratedAudioEvent();
    app.state = kDeviceStateUpgrading;
    app.generated_playback_.Fail();
    app.HandleGeneratedAudioEvent();
    CHECK(!app.generated_audio_session_);
    CHECK(app.state == kDeviceStateUpgrading);
    app.audio_service_.on_release = {};
    // Rejected/busy and unavailable service must not hold output or microphone.
    app.state = kDeviceStateUpgrading;
    app.RequestGeneratedAudio(p);
    app.HandleGeneratedAudioEvent();
    CHECK(app.GeneratedAudioStatus().find("rejected") != std::string::npos);
    CHECK(!app.audio_service_.IsLocalAudioActive());
    CHECK(app.state == kDeviceStateUpgrading);
    app.state = kDeviceStateIdle;
    app.audio_service_.available = false;
    app.RequestGeneratedAudio(p);
    app.HandleGeneratedAudioEvent();
    CHECK(!app.generated_audio_session_);
    CHECK(app.state == kDeviceStateIdle);
    CHECK(app.audio_service_.wake && !app.audio_service_.mic);
    // Cancel before command handling never starts output.
    app.audio_service_.available = true;
    auto resets = app.audio_service_.resets;
    app.RequestGeneratedAudio(p);
    app.StopGeneratedAudio();
    app.HandleGeneratedAudioEvent();
    CHECK(app.audio_service_.resets == resets);
    CHECK(!app.audio_service_.IsLocalAudioActive());
    // Recovery policy leaves idle, from which a normal button/listening action works.
    app.SetDeviceState(kDeviceStateListening);
    app.audio_service_.EnableVoiceProcessing(true);
    CHECK(app.audio_service_.mic && app.state == kDeviceStateListening);
    Application range_app;
    tone_noise::Parameters boundary;
    boundary.frequency_hz = 19;
    CHECK(range_app.RequestGeneratedAudio(boundary).find("rejected") != std::string::npos);
    boundary.frequency_hz = 11001;
    CHECK(range_app.RequestGeneratedAudio(boundary).find("rejected") != std::string::npos);
    for (uint32_t frequency : {20u, 11000u}) {
        boundary.frequency_hz = frequency;
        CHECK(range_app.RequestGeneratedAudio(boundary).find("accepted") != std::string::npos);
        range_app.HandleGeneratedAudioEvent();
        range_app.audio_service_.OutputChunk();
        range_app.StopGeneratedAudio();
        range_app.HandleGeneratedAudioEvent();
        range_app.audio_service_.OutputChunk();
        range_app.HandleGeneratedAudioEvent();
        CHECK(!range_app.generated_audio_session_);
        CHECK(range_app.state == kDeviceStateIdle);
    }
    CHECK(system_codec.output_volume() == 37 && system_codec.volume_set_calls == 0);
    std::puts("actual XiaoZhi adapter lifecycle passed (fake RTOS/board/protocol/output)");
}
