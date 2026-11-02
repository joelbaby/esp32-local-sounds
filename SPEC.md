# Public package acceptance contract

Extract public group-4 sounds as a reusable source add-on for pinned XiaoZhi
2.5.3 and ESP-IDF 6.1, independently of Otto and private features. Include sine,
sawtooth, upward frequency sweeps, pink noise, brass/Axel F and ocean; ranges,
relative levels, envelopes, checked output, cancellation and conversation resume.

Preserve 240-sample generated PCM, two waiting decoded PCM frames, upstream
compressed queue size and full-buffer conversion. Keep bounded generated-write
failure handling; do not bundle one-frame/chunked private conversion or platform
RX/microphone/decode/display tuning. Shared host output/session integration must
be independently installable without algorithms or private code.

Publish only small allowlisted source/docs/tests/tools. Retain licences, provenance
and separate composition-rights notice. Exclude private recordings, recognition,
reply retention, reserve commands/diagnostics, credentials, configuration secrets,
NVS/full flash, captures, conversation content, firmware history, toolchains and
build output. Preserve existing workspace work and do not flash a device.

Install/remove must check pinned source and conflicts, never blindly overwrite.
Test synthesis/commands/ranges/levels, stop/resume, stale callbacks, output failures,
source transaction roundtrips, no private includes/assets and two PCM frames.
Build sound-only using existing pinned SDK/tools and caches. Record exact versions,
limitations, compatibility review and disk usage. Label older/future and unbuilt
feature combinations untested. Review all public contents before publication.
