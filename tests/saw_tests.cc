// SPDX-License-Identifier: MIT
#include "tone_noise_generator.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace tone_noise;
std::vector<int16_t> Render(Parameters p, uint32_t rate, size_t chunk) {
    Generator g;
    assert(g.Start(p, rate));
    std::vector<int16_t> result, buffer(chunk);
    while (auto n = g.Render(buffer.data(), buffer.size()))
        result.insert(result.end(), buffer.begin(), buffer.begin() + n);
    return result;
}
double Bin(const std::vector<int16_t>& data, double hz) {
    double real = 0, imag = 0;
    // Interior one-second window, independently specified 24 kHz sampling.
    for (size_t i = 2400; i < 26400; ++i) {
        const double angle = 2 * 3.141592653589793 * hz * i / 24000;
        real += data[i] * std::cos(angle);
        imag += data[i] * std::sin(angle);
    }
    return std::hypot(real, imag);
}
int main() {
    Parameters p;
    p.kind = Kind::Saw;
    p.frequency_hz = 440;
    p.duration_ms = 1200;
    auto data = Render(p, 24000, 240);
    assert(data.size() == 28800 && data.front() == 0 && data.back() == 0);
    assert(data == Render(p, 24000, 37));
    const double fundamental = Bin(data, 440);
    const double second = Bin(data, 880) / fundamental;
    const double third = Bin(data, 1320) / fundamental;
    assert(second > .46 && second < .54);
    assert(third > .30 && third < .37);
    assert(Bin(data, 441) < fundamental * .03);
    p.level_percent = 20;
    auto quiet = Render(p, 24000, 240);
    for (size_t i = 0; i < data.size(); ++i)
        assert(std::abs(int(quiet[i]) * 5 - data[i]) <= 4);
    for (uint32_t rate : {16000u, 24000u, 48000u}) {
        p.frequency_hz = 20;
        assert(!Generator::Validate(p, rate));
        p.frequency_hz = std::min(11000u, rate * 11 / 24);
        assert(!Generator::Validate(p, rate));
        assert(!Render(p, rate, 240).empty());
        ++p.frequency_hz;
        assert(Generator::Validate(p, rate));
    }
    p.frequency_hz = 440;
    Generator g;
    assert(g.Start(p, 24000));
    int16_t buffer[240];
    assert(g.Render(buffer, 240) == 240);
    g.Stop();
    assert(g.Render(buffer, 240) == 120 && buffer[119] == 0 && !g.active());
    std::puts("Saw: 440 Hz harmonic ratios, deterministic chunks, volume, ceilings and 5 ms stop passed");
}
