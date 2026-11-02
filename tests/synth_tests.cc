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
std::vector<int16_t> pcm(Parameters p, unsigned rate, unsigned chunk) {
    Generator g;
    CHECK(g.Start(p, rate));
    std::vector<int16_t> result;
    int16_t buffer[257];
    while (auto n = g.Render(buffer, chunk))
        result.insert(result.end(), buffer, buffer + n);
    CHECK(!g.active());
    return result;
}
int main() {
    Parameters p;
    p.kind = Kind::Brass;
    p.duration_ms = 1000;
    p.level_percent = 20;
    Generator g;
    p.frequency_hz = 54;
    CHECK(!g.Start(p, 24000));
    p.frequency_hz = 2001;
    CHECK(!g.Start(p, 24000));
    p.frequency_hz = 55;
    CHECK(g.Start(p, 16000));
    p.frequency_hz = 2000;
    CHECK(g.Start(p, 16000));
    p.frequency_hz = 440;
    auto brass = pcm(p, 24000, 17);
    CHECK(brass == pcm(p, 24000, 257));
    CHECK(brass.size() == 24000);
    CHECK(brass.front() == 0 && brass.back() == 0);
    for (auto sample : brass)
        CHECK(std::abs(sample) <= 6553);
    // Smooth note attack and natural release; no full-scale step on note onset.
    CHECK(*std::max_element(brass.begin(), brass.begin() + 24) < 700);
    CHECK(std::all_of(brass.end() - 1, brass.end(), [](int16_t x) { return x == 0; }));
    for (Kind kind : {Kind::Brass, Kind::AxelF}) {
        p.kind = kind;
        p.duration_ms = kind == Kind::AxelF ? 0 : 1000;
        CHECK(g.Start(p, 24000));
        int16_t out[240];
        CHECK(g.Render(out, 240) == 240);
        g.Stop();
        size_t tail = 0;
        while (auto n = g.Render(out, 240)) {
            tail += n;
            CHECK(std::abs(out[n - 1]) <= 6553);
        }
        CHECK(tail == 120);
        CHECK(out[119] == 0);
        Playback owner;
        CHECK(owner.Request(p, 24000, 1));
        owner.Render(out, 240);
        owner.Commit();
        CHECK(owner.Request(p, 24000, 2));
        owner.Render(out, 240);
        owner.Commit();
        owner.Render(out, 240);
        owner.Commit();
        CHECK(owner.status().request_id == 2);
        owner.Stop();
        owner.Render(out, 240);
        CHECK(owner.active());
        owner.Commit();
        CHECK(!owner.active());
        CHECK(owner.status().state == Playback::State::Stopped);
        CHECK(owner.Request(p, 24000, 3));
        owner.Render(out, 240);
        owner.Fail();
        CHECK(!owner.active());
        CHECK(owner.status().state == Playback::State::Error);
    }
    p.kind = Kind::AxelF;
    p.duration_ms = 0;
    p.frequency_hz = UINT32_MAX;
    auto tune = pcm(p, 24000, 1);
    CHECK(tune.size() == 160000);
    CHECK(tune == pcm(p, 24000, 257));
    CHECK(tune.front() == 0 && tune.back() == 0);
    for (auto sample : tune)
        CHECK(std::abs(sample) <= 6553);
    // Licensed-publisher timing:4sixteenths at117BPM to first Ab onset;
    // cumulative rational sample positions, with80% gates/10ms release and rests.
    CHECK(
        std::all_of(tune.begin() + 10085, tune.begin() + 12307, [](int16_t x) { return x == 0; }));
    CHECK(tune[12307] == 0);
    CHECK(
        std::all_of(tune.begin() + 19931, tune.begin() + 21538, [](int16_t x) { return x == 0; }));
    CHECK(std::any_of(tune.begin() + 12667, tune.begin() + 19000,
                      [](int16_t x) { return std::abs(x) > 100; }));
    const unsigned onsets[] = {0,  4,  7,  9,  10, 12, 14, 16, 20, 23, 25, 26,
                               28, 30, 32, 34, 36, 38, 39, 41, 42, 44, 46, 52};
    for (unsigned note = 0; note < 23; ++note) {
        const size_t begin = uint64_t(24000) * 60 * onsets[note] / 468;
        const size_t end = uint64_t(24000) * 60 * onsets[note + 1] / 468;
        CHECK(tune[begin] == 0);
        CHECK(std::any_of(tune.begin() + begin + 120, tune.begin() + end - 240,
                          [](int16_t x) { return std::abs(x) > 100; }));
        if (note < 22) {
            const size_t release_end = begin + (end - begin) * 4 / 5 + 240;
            CHECK(std::all_of(tune.begin() + release_end, tune.begin() + end,
                              [](int16_t x) { return x == 0; }));
        }
    }
    // Final note is held until the10ms release, rather than an80%-gate long rest.
    CHECK(std::any_of(tune.end() - 1200, tune.end() - 300,
                      [](int16_t x) { return std::abs(x) > 100; }));
    p.duration_ms = 200;
    auto capped = pcm(p, 24000, 239);
    CHECK(capped.size() == 4800);
    CHECK(capped.back() == 0);
    CHECK(std::equal(capped.begin(), capped.end() - 120, tune.begin()));
    p.duration_ms = 60000;
    CHECK(pcm(p, 24000, 240).size() == 160000);  // no padding or loop
    p.duration_ms = 0;
    CHECK(pcm(p, 44100, 239).size() == 294000);
    p.duration_ms = 1;
    CHECK(!g.Start(p, 24000));
    p.duration_ms = 99;
    CHECK(!g.Start(p, 24000));
    p.duration_ms = 60001;
    CHECK(!g.Start(p, 24000));
    CHECK(sizeof(Generator) <= 208);
    CHECK(sizeof(Playback) <= 256);
    std::printf(
        "brass/score lifecycle passed; natural=13beats@117BPM/160000 samples, Generator=%zu "
        "Playback=%zu\n",
        sizeof(Generator), sizeof(Playback));
}
