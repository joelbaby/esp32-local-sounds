// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "tone_noise_generator.h"
using namespace tone_noise;
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
            std::abort();                                        \
        }                                                        \
    } while (0)
std::vector<int16_t> render(Generator& g, size_t capacity = 239) {
    std::vector<int16_t> result;
    int16_t chunk[257];
    while (auto count = g.Render(chunk, capacity))
        result.insert(result.end(), chunk, chunk + count);
    return result;
}
int main() {
    Parameters p;
    Generator g;
    p.duration_ms = 0;
    CHECK(g.Start(p, 24000));
    CHECK(g.total_samples() == 120000);
    p.duration_ms = 60001;
    CHECK(!g.Start(p, 24000));
    p.duration_ms = 1000;
    p.frequency_hz = 19;
    CHECK(!g.Start(p, 24000));
    p.frequency_hz = 11001;
    CHECK(!g.Start(p, 24000));
    p.frequency_hz = 440;
    p.level_percent = 0;
    CHECK(!g.Start(p, 24000));
    p.level_percent = 21;
    CHECK(g.Start(p, 24000));
    p.level_percent = 101;
    CHECK(!g.Start(p, 24000));
    p.level_percent = UINT32_MAX;
    CHECK(!g.Start(p, 24000));
    p.level_percent = 20;
    CHECK(!g.Start(p, 0));
    p.kind = Kind::Sweep;
    p.end_frequency_hz = 439;
    CHECK(!g.Start(p, 24000));
    p.end_frequency_hz = 440;
    CHECK(!g.Start(p, 24000));
    p.kind = static_cast<Kind>(-1);
    CHECK(!g.Start(p, 24000));
    p.kind = Kind::Tone;
    CHECK(g.Start(p, 24000));
    auto pcm = render(g);
    CHECK(pcm.size() == 24000);
    CHECK(!g.active());
    CHECK(pcm.front() == 0 && pcm.back() == 0);
    CHECK(*std::max_element(pcm.begin(), pcm.end()) <= 6553);
    CHECK(*std::min_element(pcm.begin(), pcm.end()) >= -6553);
    unsigned crossings = 0;
    for (size_t i = 121; i < 23880; i++)
        if (pcm[i - 1] <= 0 && pcm[i] > 0)
            ++crossings;
    CHECK(crossings >= 435 && crossings <= 436);
    double power = 0;
    for (size_t i = 120; i < 23880; i++)
        power += double(pcm[i]) * pcm[i];
    CHECK(std::abs(std::sqrt(power / 23760) - 4633.8) < 5);
    for (unsigned i = 0; i < 120; i++)
        CHECK(std::abs(pcm[i]) <= int(6554 * i / 120));
    CHECK(g.Start(p, 24000));
    int16_t chunk[240];
    CHECK(g.Render(chunk, 240) == 240);
    g.Stop();
    auto release = render(g);
    CHECK(release.size() == 120);
    CHECK(release.back() == 0);
    for (unsigned i = 0; i < release.size(); i++)
        CHECK(std::abs(release[i]) <= int(6554 * (119 - i) / 119) + 1);
    for (uint32_t frequency : {20u, 11000u}) {
        Parameters boundary;
        boundary.frequency_hz = frequency;
        boundary.duration_ms = 5000;
        boundary.level_percent = 20;
        CHECK(g.Start(boundary, 24000));
        const auto boundary_pcm = render(g);
        CHECK(boundary_pcm.size() == 120000);
        CHECK(boundary_pcm.front() == 0 && boundary_pcm.back() == 0);
        for (int16_t sample : boundary_pcm)
            CHECK(std::abs(sample) <= 6553);
        unsigned positive_crossings = 0;
        double first_crossing = 0, last_crossing = 0;
        for (size_t i = 121; i < boundary_pcm.size() - 120; ++i) {
            if (boundary_pcm[i - 1] <= 0 && boundary_pcm[i] > 0) {
                const double crossing =
                    i - 1 + double(-boundary_pcm[i - 1]) / (boundary_pcm[i] - boundary_pcm[i - 1]);
                if (positive_crossings == 0)
                    first_crossing = crossing;
                last_crossing = crossing;
                ++positive_crossings;
            }
        }
        const double measured_hz =
            (positive_crossings - 1) * 24000.0 / (last_crossing - first_crossing);
        CHECK(std::abs(measured_hz - frequency) < 0.15);
        std::printf("boundary tone requested=%u measured=%.6f Hz samples=%zu\n", frequency,
                    measured_hz, boundary_pcm.size());
    }
    Parameters lower_rate;
    lower_rate.frequency_hz = 7333;
    CHECK(g.Start(lower_rate, 16000));
    lower_rate.frequency_hz = 7334;
    CHECK(!g.Start(lower_rate, 16000));
    lower_rate.frequency_hz = 8000;
    CHECK(!g.Start(lower_rate, 16000));
    lower_rate.frequency_hz = 11000;
    CHECK(!g.Start(lower_rate, 16000));
    lower_rate.frequency_hz = 12000;
    CHECK(!g.Start(lower_rate, 24000));
    lower_rate.kind = Kind::Sweep;
    lower_rate.frequency_hz = 20;
    lower_rate.end_frequency_hz = 11000;
    CHECK(!g.Start(lower_rate, 16000));
    lower_rate.end_frequency_hz = 11001;
    CHECK(!g.Start(lower_rate, 24000));
    p.kind = Kind::Sweep;
    p.frequency_hz = 20;
    p.end_frequency_hz = 11000;
    CHECK(g.Start(p, 24000));
    pcm = render(g);
    // Zero-crossing measurements at independently calculated linear-sweep times.
    for (int center : {2400, 12000, 21600}) {
        unsigned count = 0;
        for (int i = center - 600; i < center + 600; i++)
            if (pcm[i - 1] <= 0 && pcm[i] > 0)
                ++count;
        double measured = count * 20.;
        double expected = 20 + 10980. * center / 23999;
        std::printf("wide sweep at sample=%d measured=%.1f expected=%.3f Hz\n", center, measured,
                    expected);
        CHECK(std::abs(measured - expected) < 25);
    }
    Parameters pink;
    pink.kind = Kind::Pink;
    pink.duration_ms = 1000;
    pink.level_percent = 20;
    pink.frequency_hz = UINT32_MAX;
    pink.end_frequency_hz = UINT32_MAX;
    CHECK(g.Start(pink, 24000, 12345));
    auto a = render(g, 17);
    CHECK(g.Start(pink, 24000, 12345));
    auto b = render(g, 257);
    CHECK(a == b);
    for (auto sample : a)
        CHECK(std::abs(sample) <= 6553);
    for (uint32_t rate : {16000u, 24000u, 44100u, 48000u}) {
        Parameters short_tone;
        short_tone.duration_ms = 123;
        CHECK(g.Start(short_tone, rate));
        CHECK(render(g).size() == static_cast<uint64_t>(rate) * 123 / 1000);
    }
    // Memory is fixed independent of duration; all rendering storage is caller-owned.
    CHECK(sizeof(Generator) <= 208);
    CHECK(sizeof(Playback) <= 256);
    Playback playback;
    CHECK(playback.Request(pink, 24000, 1));
    CHECK(playback.active());
    CHECK(playback.status().state == Playback::State::Pending);
    auto n = playback.Render(chunk, 240);
    CHECK(n == 240);
    CHECK(playback.Render(chunk, 240) == 0);  // Cannot advance before output acknowledgement.
    CHECK(playback.Request(pink, 24000, 2));
    CHECK(playback.Request(pink, 24000, 3));
    CHECK(playback.status().pending_id == 3);
    playback.Commit();
    CHECK(playback.active());
    CHECK(playback.Render(chunk, 240) == 120);
    CHECK(chunk[119] == 0);
    playback.Commit();
    CHECK(playback.active());
    CHECK(playback.Render(chunk, 240) == 240);
    CHECK(chunk[0] == 0);
    CHECK(playback.status().request_id == 3);
    playback.Commit();
    playback.Stop();
    CHECK(playback.active());
    CHECK(playback.status().state == Playback::State::Stopping);
    CHECK(playback.Render(chunk, 240) == 120);
    CHECK(playback.active());
    playback.Commit();
    CHECK(!playback.active());
    CHECK(playback.status().state == Playback::State::Stopped);
    p.kind = Kind::Tone;
    p.duration_ms = 100;
    p.frequency_hz = 440;
    CHECK(playback.Request(p, 24000, 4));
    size_t total = 0;
    while (playback.active()) {
        total += playback.Render(chunk, 240);
        playback.Commit();
    }
    CHECK(total == 2400);
    CHECK(playback.status().state == Playback::State::Completed);
    CHECK(playback.Request(p, 24000, 5));
    playback.Stop();
    CHECK(playback.Render(chunk, 240) == 0);
    playback.Commit();
    CHECK(!playback.active());
    CHECK(playback.status().request_id == 5);
    CHECK(playback.Request(p, 24000, 6));
    playback.Render(chunk, 240);
    playback.Fail();
    CHECK(!playback.active());
    CHECK(playback.status().state == Playback::State::Error);
    CHECK(playback.Request(p, 24000, 7));
    CHECK(playback.active());
    std::printf("generator/lifecycle passed; Generator=%zu Playback=%zu bytes\n", sizeof(Generator),
                sizeof(Playback));
}
