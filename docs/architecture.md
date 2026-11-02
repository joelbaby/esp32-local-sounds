# Playback integration contract

## Portable algorithms

`Generator` renders bounded PCM into caller-owned storage without locks, device,
cloud, RTOS or duration-sized allocations. It keeps deterministic PRNG/filter/
oscillator state and uses 5 ms start/end/stop envelopes. `Playback` adds one pending
request, replacement and commit-after-write state. There is no background thread
in the library. The table generator uses mathematical finite Fourier sums; ocean
uses synthetic pink noise and a filtered swell envelope.

The exposed `saw` mode is new in this extraction. Earlier integrated firmware
exposed saw oscillators only inside brass synthesis. The package reuses those
original tables as a single oscillator, selects a harmonic count below 45% of the
sample rate, and tests pitch, harmonics, levels and sample-rate ceilings.

## Shared host glue, installed once

`shared-playback.patch`, `local_audio_source.h`, `local_audio_session.h` and
`local_audio_adapter.cc` have **no dependency on sound algorithms, private assets,
semantic recognition or an optional display implementation**. They can be installed
alone with `--part shared-playback`. The separate sound patch adds only the public
producer, its CMake entries, thin application adapter and three MCP tools.

`local_audio::Source` defines render/commit/stop/fail/activity/status. Sources must
have stable storage until ownership and any in-flight write are released. Render
must honor capacity, make progress while active and never block. All producer
state changes are serialized by the existing audio queue mutex.

The application-facing API is:

```cpp
std::string Application::RequestLocalPlayback(
    local_audio::Source& source,
    std::function<bool(uint32_t request_id, uint32_t output_rate)> configure);
std::string Application::StopLocalPlayback(local_audio::Source* expected = nullptr);
std::string Application::LocalPlaybackStatus(const local_audio::Source& source);
```

The configure callback executes under the audio queue mutex and must only update
the source, without calling application/protocol methods. It returns false when
admission fails. The same source can replace its pending request. A different
source is rejected while another is pending or owns the session; stop and release that session
before retrying. A future producer can call this API without including the sound
header or linking the synthesis implementation.

The low-level `AudioService::ControlLocalAudio` contract handles output ownership,
queue generation, stale decoded work and the sole output writer. Independent
features should use `RequestLocalPlayback`, so they share the application's pause,
callback suppression, cancellation and conversation-resume policy. Do not create a
second I2S writer or bypass this session with direct low-level calls.

The existing output task renders 240-sample blocks. It commits only after a full
successful write; failure discards pending work. Ownership persists through the
final write and application completion, keeping cloud decode/drain callbacks out
of that gap. Source epochs reject completion from a source reset/reacquired while
an old driver call returns. Expected-owner checks stop one producer from releasing
or stopping another. The public sound tool passes its own source; physical,
network and reset cancellation explicitly use the global null-owner form. No additional unbounded queue or RTOS task is added.

The shared application layer owns main-loop admission, request IDs, input/uplink
pause, epoch-guarded STT/TTS callbacks, physical stop, reset/network cancellation
and deferred listening resume. Error callbacks can run synchronously during a
protocol send: they disable resume and post an event, avoiding recursive locking.
Display status uses the existing generic Display interface and works with
`NoDisplay`; no Otto headers are included.

## Output contract and preserved settings

`AudioCodec::OutputGeneratedData` defaults to a safe unsupported result. A codec
must explicitly implement bounded checked output; ordinary speech `OutputData`
is unchanged. The tested `NoAudioCodec` implementation converts the **whole**
generated frame to a newly allocated int32 buffer, checks allocation, waits at
most 100 ms for its timed mutex, activates a disabled channel with checked errors under that same lock, then
uses the remaining deadline for **one** I2S write. Generated playback never calls
the ordinary unbounded/fatal `EnableOutput` path. In the pinned driver,
`i2s_channel_enable` has an internal mutex without a timeout; all runtime TX-handle
operations are serialized by the codec's outer timed mutex, so no other TX operation
can own that internal mutex after it is acquired. Channel-enable failure and a
deadline overrun return failure rather than aborting. Short/error/over-deadline results fail playback. There is no persistent
180-int32 scratch array or 720-byte chunking. Normal speech retains its original
full-buffer vector conversion and error policy; its mutex type is shared with the
bounded generated path.

Only `NoAudioCodec`-based hardware is currently supported by this adapter. Other
codecs fail generated output safely until a bounded implementation is supplied
and tested. A build for another codec is not proof of working generated playback.

Two waiting PCM frames and the upstream 20 compressed decode packets remain
unchanged. The installer touches no sdkconfig defaults, Wi-Fi settings, microphone
processor/gain or display source/settings. No group-2 platform fixes or tuning are
required by this build and none are bundled. Existing upstream MCP tool-list memory
behavior remains; device-level listing/admission on memory-constrained targets
still needs hardware validation.

## Optional features

The Otto package touches display code, Kconfig and a different CMake section.
Reviewed source installation/removal transactions preserve those changes. Its
installer requires pristine touched hashes, so use the documented remove/install/
reinstall order. Combined firmware and physical behavior remain untested.

A future private playback feature must reuse the shared source/session contract,
install its private producer/assets separately and coordinate shared-glue ownership.
It must own any feature-gated one-frame queue or chunked conversion changes and
restore the public two-frame/full-buffer configuration when disabled. The public
installer deliberately refuses unknown modifications to shared files. A future
feature needs a versioned integration update and combination tests; private
compatibility is an architecture review, not a tested installation claim. Public
sounds and a future recording producer must each remain usable without the other.

For older/future XiaoZhi, review event bits, state transitions, protocol callback
threads, listening/AEC behavior, codec timeout semantics, queue generations and
CMake registration against these seams. Re-run both transports and relevant board
builds after a port. No compatibility is claimed from matching file names alone.

## Fresh repeated user commands

After resume, equal STT text can be new speech. Existing AFE VAD onset marks the
current local playback epoch as fresh. On Lite/C3, a fixed-state observer reads
the existing 16 kHz engine output: 1920 quiet samples (mean absolute amplitude
<=128) followed by 960 active samples (mean >=512). Only the duplicate-text guard
uses this signal. No PCM is modified or retained, no input gain/processing setting
is changed, and no per-frame allocation is added. Epoch checks prevent an old
activity indication from unlocking a later local session. Silence/noise alone
never unlocks the guard. Thresholds and noise behavior need hardware validation;
continuous noise or a missing quiet interval may require manual fresh listening.
