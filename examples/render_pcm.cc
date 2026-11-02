// SPDX-License-Identifier: MIT
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "tone_noise_generator.h"
int main(int argc, char** argv) {
    tone_noise::Parameters p;
    p.duration_ms = 60000;
    if (argc > 1 && std::strcmp(argv[1], "pink") == 0)
        p.kind = tone_noise::Kind::Pink;
    else if (argc > 1 && std::strcmp(argv[1], "ocean") == 0)
        p.kind = tone_noise::Kind::Ocean;
    else if (argc > 1 && std::strcmp(argv[1], "saw") == 0)
        p.kind = tone_noise::Kind::Saw;
    else if (argc > 1 && std::strcmp(argv[1], "sweep") == 0) {
        p.kind = tone_noise::Kind::Sweep;
        p.frequency_hz = 200;
    } else if (argc > 1 && std::strcmp(argv[1], "brass") == 0) {
        p.kind = tone_noise::Kind::Brass;
        p.duration_ms = 5000;
    } else if (argc > 1 && std::strcmp(argv[1], "axel_f") == 0) {
        p.kind = tone_noise::Kind::AxelF;
        p.duration_ms = 0;
    } else if (argc > 1 && std::strcmp(argv[1], "tone") != 0)
        return 1;
    uint32_t rate = 24000;
    uint32_t* options[] = {&p.frequency_hz, &p.duration_ms, &p.level_percent, &rate};
    for (int i = 2; i < argc; i++) {
        if (i > 5)
            return 1;
        errno = 0;
        char* end = nullptr;
        const auto value = std::strtoul(argv[i], &end, 10);
        if (errno || !end || *end || end == argv[i] || value > UINT32_MAX)
            return 1;
        *options[i - 2] = static_cast<uint32_t>(value);
    }
    tone_noise::Generator generator;
    if (!generator.Start(p, rate, 12345))
        return 1;
    int16_t chunk[240];
    while (size_t count = generator.Render(chunk, 240))
        if (std::fwrite(chunk, sizeof(int16_t), count, stdout) != count)
            return 2;
}
