// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "tone_noise_generator.h"
using namespace tone_noise;
std::vector<int16_t> Render(Parameters p, unsigned chunk, uint32_t rate = 24000,
                            uint32_t seed = 12345) {
    Generator g;
    assert(g.Start(p, rate, seed));
    std::vector<int16_t> a, b(chunk);
    while (auto n = g.Render(b.data(), b.size()))
        a.insert(a.end(), b.begin(), b.begin() + n);
    return a;
}
int main() {
    Parameters p;
    p.kind = Kind::Ocean;
    auto full = Render(p, 240);
    assert(full.size() == 1440000 && full.front() == 0 && full.back() == 0);
    assert(full == Render(p, 17));
    assert(full != Render(p, 240, 24000, 98765));
    p.level_percent = 20;
    auto quiet = Render(p, 513);
    for (size_t i = 0; i < full.size(); ++i) {
        assert(std::abs(int(full[i])) <= 27852);
        assert(quiet[i] == int(full[i]) / 5);
    }
    for (auto rate : {16000u, 24000u, 44100u, 48000u}) {
        p.duration_ms = 100;
        assert(Render(p, 113, rate).size() == rate / 10);
    }
    p.duration_ms = 60001;
    assert(Generator::Validate(p, 24000));
    p.duration_ms = 99;
    assert(Generator::Validate(p, 24000));
    p.duration_ms = 1000;
    Generator g;
    assert(g.Start(p, 24000));
    int16_t b[240];
    g.Render(b, 240);
    g.Stop();
    assert(g.Render(b, 240) == 120 && b[119] == 0 && !g.active());
    Playback play;
    assert(play.Request(p, 24000, 1));
    play.Render(b, 240);
    play.Commit();
    p.kind = Kind::Tone;
    assert(play.Request(p, 24000, 2));
    while (play.active()) {
        play.Render(b, 240);
        play.Commit();
    }
    assert(play.status().request_id == 2 && play.status().state == Playback::State::Completed);
    p.kind = Kind::Ocean;
    assert(play.Request(p, 24000, 3));
    play.Fail();
    assert(!play.active() && play.status().state == Playback::State::Error);
    std::puts(
        "Ocean:60second default, bounds/gain, chunk/seed determinism, rate "
        "counts, duration validation, stop/replacement/error passed.");
}
