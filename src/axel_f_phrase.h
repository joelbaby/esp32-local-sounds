// SPDX-License-Identifier: LicenseRef-Musical-Fixture-Review
// User-supplied Axel F-inspired pitches; corrected onset rhythm from licensed
// publisher preview. Musical material is outside the implementation MIT license.
// See LICENSES/Musical-Fixture-Review.txt for separate composition rights.
#pragma once
#include <cstdint>
namespace tone_noise::detail {
struct NoteEvent {
    uint16_t frequency_hz;
    uint8_t interval_ticks;
};
// Sixteenth-note ticks at117BPM. Ties are combined into the23 supplied pitches.
// https://www.notendownload.com/en/axel-f/7060424-pdf
// Last6ticks include an eighth tied to a quarter; no extra three-beat trailing rest.
constexpr NoteEvent kAxelPhrase[] = {{349, 4}, {415, 3}, {349, 2}, {349, 1}, {466, 2}, {349, 2},
                                     {311, 2}, {349, 4}, {523, 3}, {349, 2}, {349, 1}, {554, 2},
                                     {523, 2}, {415, 2}, {349, 2}, {523, 2}, {698, 2}, {349, 1},
                                     {311, 2}, {311, 1}, {261, 2}, {392, 2}, {349, 6}};
constexpr unsigned kAxelNoteCount = sizeof(kAxelPhrase) / sizeof(kAxelPhrase[0]);
constexpr uint32_t kAxelBpm = 117;
constexpr uint32_t kAxelTicksPerQuarter = 4;
constexpr uint32_t kAxelTotalTicks = 52;
constexpr uint32_t kAxelDurationMs = 6667;  // Display ceiling only; rendering uses exact ticks.
}  // namespace tone_noise::detail
