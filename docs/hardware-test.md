# Hardware acceptance still required

No device was flashed or exercised for this package. On the correct hardware and
firmware, separately verify:

1. Provisioning/activation and ordinary conversation before and after installation.
2. All seven sound kinds, safe levels, ranges and a normal speaker-volume change.
3. Physical stop during each kind, replacement, stop-before-start and final ramps.
4. Conversation resume after completion/stop, stale speech rejection, a new user
   request and manual listening after an identical repeated phrase.
5. Reset, network loss/reconnect, output failure and recovery without stuck input
   or an unsolicited upload. Test appropriate AEC and wake-in-listening policies.
6. Tool enumeration and play/stop/status on the actual memory-constrained device.
7. Optional display/no-display and any separately installed Otto/private feature.

Do not treat this checklist as completed by source tests. The generated PCM tests
measure digital samples, not acoustic loudness or hardware I2S latency.
