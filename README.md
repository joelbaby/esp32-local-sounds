# ESP32 Local Sounds for XiaoZhi

A reusable local sound generator and source integration for XiaoZhi firmware:
sine tones, band-limited sawtooth waves, upward frequency sweeps, pink noise,
brass synthesis, an Axel F Easter egg and ocean surf. Rendering runs on the
existing audio output task with 240-sample PCM blocks. No cloud audio or recorded
sound assets are needed.

This repository contains a small C++ library, focused tests, guarded source
patches and an optional Pocket C3 hardware example. Install it into your own
firmware source, rebuild, then follow that firmware's normal installation process.
It is not a complete firmware distribution. No flashing is performed by its tools.

- [Install, remove and build](docs/installation.md)
- [Commands, ranges and volume](docs/usage.md)
- [Shared playback and optional-feature integration](docs/architecture.md)
- [Validation and exact version pins](docs/validation.md), [troubleshooting](docs/troubleshooting.md)
- [Licences and provenance](THIRD_PARTY_NOTICES.md)

The tested XiaoZhi base is **2.5.3**, commit
`addb1ed59bd71e09a6a5e0c52ad2f1c474aa8ca1`, with the **ESP-IDF 6.1 development
snapshot** at `9a97f6c54ec638111ce55cd36581b3c192f15207`.
Older and future firmware integrations are **untested** and need an explicit port.

The public configuration retains **two waiting decoded PCM frames**, the upstream
20-packet compressed decode queue and full-buffer I2S conversion. Installation
does not change Wi-Fi RX settings, microphone gain/processing or display settings.
Otto and other add-ons are optional; see the tested installer-order guidance.

Software is MIT with retained XiaoZhi attribution. The separately marked short
musical phrase has composition rights outside the software licence; no musical
composition licence or clearance is granted. This is a community project.
