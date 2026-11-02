# Troubleshooting

- **Installer refuses source:** read the named file and compare your consumer diff.
  The pinned revision and exact before hashes are intentional. Save your edits and
  manually port the small integration if needed. Do not force-apply over them.
- **Installer says already installed:** retain its local records. Rebuild or remove
  the existing installation before updating it. Re-running does not overwrite it.
- **Removal refuses modified files:** preserve your modifications and review the
  reverse patch. Removal cannot decide which later consumer edits to discard.
- **Sound accepted but not playing:** acceptance means pending. Query status, check
  the output sample rate and current device state. Activation, upgrading and audio
  testing are busy states. Unsupported codecs return an error safely.
- **Write error/short write/timeout:** playback does not claim completion or restart
  microphone upload. Check I2S wiring, channel state and memory, then use the normal
  action/listening control to recover. Repeated failures need hardware diagnosis.
- **No automatic conversation resume:** errors, disconnect/reset and exceptional
  states intentionally disable it. Check the network and firmware listening state.
  A normal completion or user stop resumes through the normal listening mode.
- **Old speech stays suppressed after resume:** distinct new user STT admits the new
  response; a repeated phrase is admitted after fresh voice activity (AFE VAD, or
  the Lite quiet/activity observer). Noise or no quiet gap can keep it guarded;
  manual listening starts a fresh turn. No utterance-ID or new VAD protocol is added.
- **Unexpected volume:** `level_percent` attenuates PCM relative to existing system
  volume. Omitted/0 uses that volume; it is not silence. Start with a low explicit
  level. The package does not write the speaker volume setting.
- **High-frequency saw sounds smooth:** its harmonics are limited below Nyquist.
- **Ocean stops:** default duration is one minute; all playback is bounded.
- **Otto install refuses sound-first checkout:** remove sounds/shared glue, install
  Otto, then reinstall sounds. Do not modify the public Otto repository.
- **Build resolves new dependencies:** verify the exact SDK/firmware pins and C3
  lock. Reuse your existing cache; review any proposed download separately.

A successful host test or build does not verify audio hardware, cloud latency,
acoustic quality, microphone recovery or a physical stop button. No device was
flashed while preparing this release. Use your receiving firmware's backup and
recovery guidance before choosing to install a rebuilt image.
