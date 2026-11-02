// SPDX-License-Identifier: MIT
#include "application.h"
#include "mcp_server.h"

void RegisterGeneratedAudioTools(McpServer& server) {
    server.AddTool(
        "self.audio_generator.play",
        "Start local sine tone, band-limited sawtooth, upward sine sweep, pink noise, brass note, or play Axel F easter "
        "egg. kind: "
        "tone/saw/sweep/pink/brass/axel_f/ocean. Ocean defaults to 60000 ms. Brass pitch 55..2000 Hz. Axel F is a local one-shot "
        "approximate riff. "
        "Tones/sweeps 20..11000 Hz at 24 kHz; lower sample rates "
        "enforce floor(sample_rate*11/24) Hz maximum and reject out-of-range requests. "
        "Duration optional: 0 uses 5000 ms for normal modes, the natural score end for axel_f. "
        "Positive 100..60000 ms caps the riff at its natural end; other modes use the requested "
        "duration. "
        "Omitted level or 0 uses current system volume. Explicit 1..100 is relative "
        "attenuation; honor the requested value. "
        "Accepted means queued, not finished. Replaces active generated sound with ramps. "
        "Microphone is paused; use action button to interrupt. Does not change speaker volume.",
        PropertyList({Property("kind", kPropertyTypeString),
                      Property("duration_ms", kPropertyTypeInteger, 0, 0, 60000),
                      Property("level_percent", kPropertyTypeInteger, 0, 0, 100),
                      Property("frequency_hz", kPropertyTypeInteger, 440, 20, 11000),
                      Property("end_frequency_hz", kPropertyTypeInteger, 4000, 20, 11000)}),
        [](const PropertyList& properties) -> ToolResult {
            tone_noise::Parameters p;
            const auto& kind = properties["kind"].value<std::string>();
            if (kind == "tone")
                p.kind = tone_noise::Kind::Tone;
            else if (kind == "saw")
                p.kind = tone_noise::Kind::Saw;
            else if (kind == "ocean")
                p.kind = tone_noise::Kind::Ocean;
            else if (kind == "sweep")
                p.kind = tone_noise::Kind::Sweep;
            else if (kind == "pink")
                p.kind = tone_noise::Kind::Pink;
            else if (kind == "brass")
                p.kind = tone_noise::Kind::Brass;
            else if (kind == "axel_f")
                p.kind = tone_noise::Kind::AxelF;
            else
                return std::unexpected("kind must be tone/saw/sweep/pink/brass/axel_f/ocean");
            p.frequency_hz = properties["frequency_hz"].value<int>();
            p.end_frequency_hz = properties["end_frequency_hz"].value<int>();
            p.duration_ms = properties["duration_ms"].value<int>();
            const auto requested_level = properties["level_percent"].value<int>();
            p.level_percent = requested_level == 0 ? 100 : requested_level;
            return Application::GetInstance().RequestGeneratedAudio(p);
        });
    server.AddTool("self.audio_generator.stop",
                   "Request a ramped stop of generated audio; does not stop conversation speech.",
                   PropertyList(), [](const PropertyList&) -> ToolResult {
                       return Application::GetInstance().StopGeneratedAudio();
                   });
    server.AddTool("self.audio_generator.status",
                   "Get accepted/pending/playing/stopping/completed/stopped state and request IDs. "
                   "Completion is after final driver write, not acoustic calibration.",
                   PropertyList(), [](const PropertyList&) -> ToolResult {
                       return Application::GetInstance().GeneratedAudioStatus();
                   });
}
