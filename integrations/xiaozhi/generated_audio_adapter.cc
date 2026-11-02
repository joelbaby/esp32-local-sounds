// SPDX-License-Identifier: MIT
#include "application.h"
#include "board.h"
std::string Application::RequestGeneratedAudio(const tone_noise::Parameters& parameters) {
    const auto rate = Board::GetInstance().GetAudioCodec()->output_sample_rate();
    if (const char* error = tone_noise::Generator::Validate(parameters, rate))
        return std::string("{\"state\":\"rejected\",\"error\":\"") + error + "\"}";
    return RequestLocalPlayback(generated_playback_, [this, parameters](uint32_t id, uint32_t rate) {
        return generated_playback_.Request(parameters, rate, id);
    });
}
std::string Application::StopGeneratedAudio() { return StopLocalPlayback(&generated_playback_); }
std::string Application::GeneratedAudioStatus() { return LocalPlaybackStatus(generated_playback_); }
