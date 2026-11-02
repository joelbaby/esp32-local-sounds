# Validation report — 2026-11-02

## Tested versions and build

- XiaoZhi **2.5.3**, `addb1ed59bd71e09a6a5e0c52ad2f1c474aa8ca1`.
- ESP-IDF **6.1 development snapshot**, `9a97f6c54ec638111ce55cd36581b3c192f15207`.
- Existing riscv32 toolchain `esp-15.2.0_20251204`, GCC 15.2.0;
  CMake 4.0.3, Ninja 1.12.1, Python 3.12.3. Native tests used GCC 13.3.0.
- Sound-only `pocket-sounds-c3-4mb`: ESP32-C3, 4 MiB, no PSRAM, en-US,
  hiesp, 24 kHz duplex I2S. No Otto or private playback producer/assets.
- Application **2,010,496 bytes**, fitting the 3,080,192-byte application partition
  with **1,069,696 bytes** free. This is a compile/source-validation image, not a
  published universal firmware or a hardware acceptance result.

A clean pinned source export was reconstructed using the optional board installer
and both public integration layers. Every touched source hash matched the reviewed
implementation, followed by a successful build using one reusable validation tree.
The existing SDK, compiler/Python environment and stock managed-component cache
were reused. No SDK, compiler, environment or dependency-cache download/copy was
needed. No ESP-IDF 5.x installation was used.

The shared existing SDK reports dirty because of a pre-existing optional lwIP
receive-diagnostics patch. Its guards are disabled for this board; no diagnostic
symbols are linked, and the build used unmodified cached esp-ml307 sources. The
package did not change SDK sources and has no dependency on that patch. This is
not a claim that a freshly downloaded SDK/cache was independently tested.

## Focused checks

All eight native suites passed with `-Wall -Wextra -Werror`, UBSan and float-cast
checks: generator, synth, volume, ocean, saw, adapter, tools and resume.

- Deterministic chunking, sample counts, ranges/sample-rate ceilings, envelope
  endpoints and 5 ms stop behavior; replacement/pending IDs and final-write commit.
- Brass detune/harmonic guards, score length/capping, all command defaults, invalid
  durations/levels/pitches/kinds, saw/ocean command admission and unchanged system
  volume. PCM gain endpoints include -32768/+32767 without wrap.
- New saw mode: 440 Hz pitch and second/third harmonic ratios approximately 1/2
  and 1/3, attenuation and rate-dependent high-frequency acceptance.
- Actual installed shared output/ownership methods compiled without any generator:
  owner exclusion, queue invalidation, final-write ownership, expected-owner guards,
  source epoch rejection after reset/reacquire, stop, failure and no-progress render.
- Actual full-buffer generated driver: one **960-byte conversion/write** for a
  240-sample frame, allocation failure, short/zero write, driver error, deadline
  overrun and bounded mutex failure. Initially disabled output, channel-enable errors,
  enable deadline overrun and lock contention are tested; generated output cannot
  reach the ordinary unbounded/fatal enable path. No chunked 720-byte path is present.
- Actual installed STT/TTS/error callback branches: stale queued speech, old speech
  after resume, distinct STT followed by an entire fresh TTS burst before main-loop
  execution, and synchronous network-error cancellation without recursive locking. Late
  duplicate STT stays blocked, while an equal repeated request plus its TTS is
  accepted after fresh Lite input or AFE VAD activity. Continuous noise alone does
  not expire the guard; activity is scoped to the current playback epoch.
- Application adapter: completion/user stop resume, request replacement, stale
  epochs, queued reconnect cancellation, output failure, unavailable service and
  existing error suppression. Public sound-stop leaves another producer
  pending/active untouched; explicitly global cancellation still stops it. Errors
  leave input disabled with wake recovery.
- Guarded install/remove roundtrips, repeated-install refusal, edited-source and
  edited-removal byte preservation, independently installed shared glue, optional
  board roundtrip, Otto-first preservation and sound-first Otto safe refusal with
  the documented remove/Otto/reinstall sequence.
- Upstream build-tool suite: **150 tests, 3 skipped**, no failures. Skips require
  optional numpy/Pillow/packaging on the host; no environment was installed for them.

Independent digital analysis used deterministic 60-second fixtures. Pink's measured
100..4000 Hz slope was **-10.29 dB/decade**. Ocean produced irregular complete
8..15-second swells; each tested crest was louder and spectrally brighter than its
retreat. Brass fundamental peaks measured 439.087/440.918 Hz against intended
439.141/440.859 Hz. No rendered previews are included in the source package.

Source/build audits confirm **two waiting PCM frames**, **20 compressed decode
packets**, 240-sample generation, full-buffer conversion and no private/Willow or
Otto sources, headers, assets, commands or linked symbols. Wi-Fi RX values in the
example build remain upstream defaults **3/6/3**; the installer adds no tuning.
Microphone processing/gain and display sources are unchanged by the sound layers.
No group-2 platform dependency was needed or bundled.

## Reproduce checks

With existing CMake, native compiler and Python on PATH:

```sh
python3 tools/test.py --build-dir /path/to/reusable-validation/host --checkout /path/to/installed-xiaozhi
python3 tests/analyze_pcm.py /path/to/reusable-validation/host/render_pcm
python3 tests/analyze_ocean.py /path/to/reusable-validation/host/render_pcm --output /path/to/reusable-validation/analysis
python3 tests/analyze_brass.py /path/to/reusable-validation/host/render_pcm
python3 tools/audit_candidate.py
```

Pass `--otto /path/to/existing/esp32-oled-animated-faces` to `tools/test.py` for the
optional installer-order replay. Installed-source checks use fake RTOS/protocol/
I2S boundaries and the consumer's existing cJSON dependency. They are not real-time
FreeRTOS, network or physical hardware tests. No extra dependency download occurs.

## Remaining limits

No device was flashed. Physical capture/playback, actual microphone/wake/AEC resume,
button timing, read-only Lite activity thresholds/noisy-room repeat admission,
network reconnect, MCP memory limits and acoustic quality remain
untested. Both transport callbacks use the same shared contract, but live MQTT/UDP
and WebSocket sessions were not exercised. Combined Otto firmware and any future
private feature are untested; only source composition/architectural seams were
reviewed. Other codecs deliberately reject generated playback. Older/future
XiaoZhi/SDK combinations require a reviewed port and fresh tests. Bit-identical
reproducibility across hosts/build timestamps is not claimed.

The source package is approximately 0.27 MB. Validation added approximately 0.54 GB
locally, mostly one firmware build; existing tools/caches were shared. The exact
local disk report accompanies the release preparation record. SDK/tool downloads:
**0 bytes**. Build/validation trees are excluded from publication.
