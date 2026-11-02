# Usage

The XiaoZhi adapter registers exactly three MCP tools:

| Tool | Behavior |
|---|---|
| `self.audio_generator.play` | Queue one sound; latest replacement wins with a release ramp |
| `self.audio_generator.stop` | Request a ramped stop of this sound producer; pending sounds are cancelled |
| `self.audio_generator.status` | Return state and request/pending IDs |

`play` properties:

| Property | Values |
|---|---|
| `kind` | `tone`, `saw`, `sweep`, `pink`, `brass`, `axel_f`, `ocean` |
| `frequency_hz` | Tone/saw/sweep start: 20..min(11000,floor(sample_rate×11/24)); default 440 |
| `end_frequency_hz` | Sweep end, strictly above start and within the same ceiling; default 4000 |
| `duration_ms` | 0 uses a bounded default; otherwise 100..60000 |
| `level_percent` | Omitted or 0 follows system volume; 1..100 attenuates PCM relative to that volume |

Brass pitch is 55..min(2000,sample_rate/8) Hz. Pitch fields are irrelevant to
noise, ocean and the fixed musical phrase. Portable rendering accepts sample
rates 16000..48000 Hz. At 24 kHz, the tone/saw/sweep ceiling is 11000 Hz.
Sweep is an upward sine sweep; a saw sweep is not implemented.

Duration 0 means 5000 ms for tone/saw/sweep/pink/brass, 60000 ms for ocean,
and the natural score end for `axel_f` (approximately 6.67 s). A positive
Axel F duration caps its natural end; it never loops. Ocean uses irregular
8..15-second swells and a brighter crest. Sawtooth is band-limited, so high
pitches deliberately contain fewer harmonics.

Example MCP argument objects:

```json
{"kind":"tone","frequency_hz":440,"duration_ms":1000,"level_percent":20}
{"kind":"saw","frequency_hz":220,"duration_ms":2000,"level_percent":15}
{"kind":"sweep","frequency_hz":200,"end_frequency_hz":4000,"duration_ms":3000}
{"kind":"ocean","duration_ms":60000,"level_percent":20}
{"kind":"axel_f"}
```

`accepted` means queued, not complete. Status distinguishes accepted, pending,
playing, stopping, completed, stopped, error and rejected states. The stop tool returns `not_owned` when another independently installed producer
owns or awaits playback, leaving that producer untouched. Status is also scoped to the public sound source.
Physical/network/reset
cancellation remains global. Request IDs
are nonzero and wrap past zero. Completion follows the final successful driver
write; it does not measure the speaker's acoustic tail.

During local playback, microphone processing/uplink and wake detection pause;
queued cloud speech is discarded. A normal completion or user stop schedules a
return to listening using the normal firmware listening mode. A failed write,
network error/disconnection, reset or incompatible device state disables automatic
resume and restores idle/wake recovery. Press the usual action button to stop a
sound. A pending command can be stopped before it starts.

After automatic resume, late TTS from the interrupted turn remains suppressed
until distinct recognized user text arrives. A canonical duplicate of the previous STT remains suppressed until fresh input
establishes a new utterance boundary. Existing AFE VAD speech onset expires that
guard. Lite/C3 has no VAD callback, so a read-only observer of its existing 16 kHz
input requires 120 ms of quiet followed by 60 ms of sustained activity. It changes
no PCM or microphone setting and retains no samples. This allows a genuine repeated
command while rejecting a late duplicate before new activity. The thresholds are
hardware-unverified and conservative: continuous noise or no quiet gap can leave
the guard active. Manual listening always starts a fresh turn. The guard is a
heuristic, not protocol-level utterance identity. No cloud protocol is changed.

For a non-XiaoZhi host, include `tone_noise_generator.h`, validate parameters,
call `Generator::Start` and render into caller-owned `int16_t` storage. Call
`Generator::Stop` to produce a final release within 5 ms of further rendering.
`Playback` adds one pending request, replacement and commit-after-write semantics.
Serialize it externally. Failed/short output must call `Fail`, never `Commit`.
