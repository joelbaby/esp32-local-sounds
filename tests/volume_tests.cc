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
std::vector<int16_t> render(Parameters parameters) {
    Generator generator;
    CHECK(generator.Start(parameters, 24000, 12345));
    std::vector<int16_t> result;
    int16_t chunk[240];
    while (auto count = generator.Render(chunk, 240))
        result.insert(result.end(), chunk, chunk + count);
    return result;
}
int main() {
    CHECK(Parameters().level_percent == 100);
    for (Kind kind : {Kind::Tone, Kind::Sweep, Kind::Pink, Kind::Brass, Kind::AxelF}) {
        Parameters p;
        p.kind = kind;
        p.duration_ms = kind == Kind::AxelF ? 0 : 1000;
        auto full = render(p);
        p.level_percent = 20;
        auto quiet = render(p);
        CHECK(full.size() == quiet.size());
        CHECK(full.front() == 0 && full.back() == 0);
        CHECK(full.size() == (kind == Kind::AxelF ? 160000 : 24000));
        int peak = 0;
        for (size_t i = 0; i < full.size(); ++i) {
            // Independent gain relation detects wrapping in full-scale output;
            // the deterministic20% stream is also compared to the old release.
            CHECK(quiet[i] == static_cast<int>(full[i]) / 5);
            CHECK(std::abs(static_cast<int>(quiet[i])) <= 6553);
            peak = std::max(peak, std::abs(static_cast<int>(full[i])));
        }
        CHECK(peak > 10000 && peak <= 32768);
        if (kind == Kind::Tone) {
            double power = 0;
            for (size_t i = 120; i < full.size() - 120; ++i)
                power += double(full[i]) * full[i];
            const double rms = std::sqrt(power / (full.size() - 240));
            CHECK(std::abs(rms - 23169.8) < 10);
            std::printf("unity sine RMS=%.6f peak=%d\n", rms, peak);
        }
        Generator g;
        p.level_percent = 100;
        CHECK(g.Start(p, 24000));
        int16_t chunk[240];
        CHECK(g.Render(chunk, 240) == 240);
        g.Stop();
        CHECK(g.Render(chunk, 240) == 120);
        CHECK(chunk[119] == 0);
        CHECK(!g.active());
        std::printf(
            "mode=%u default100/explicit20 gain relation, ramps and bounds passed; peak=%d\n",
            static_cast<unsigned>(kind), peak);
    }
}
