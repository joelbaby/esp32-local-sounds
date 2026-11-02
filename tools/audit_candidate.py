#!/usr/bin/env python3
"""Audit this explicitly allowlisted export; optionally write its checksum manifest."""
import argparse
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = set('''
.clang-format
.gitignore
CMakeLists.txt
LICENSE
LICENSES/Musical-Fixture-Review.txt
LICENSES/XiaoZhi-MIT.txt
README.md
SPEC.md
THIRD_PARTY_NOTICES.md
docs/architecture.md
docs/hardware-test.md
docs/installation.md
docs/troubleshooting.md
docs/usage.md
docs/validation.md
examples/pocket-c3/board.patch
examples/pocket-c3/config.h
examples/pocket-c3/config.json
examples/pocket-c3/patch-files.json
examples/pocket-c3/pocket_sounds_c3.cc
examples/render_pcm.cc
include/local_audio_session.h
include/local_audio_source.h
include/tone_noise_generator.h
integrations/xiaozhi/dependencies.lock
integrations/xiaozhi/generated_audio_adapter.cc
integrations/xiaozhi/generated_audio_tools.cc
integrations/xiaozhi/local_audio_adapter.cc
integrations/xiaozhi/patch-files.json
integrations/xiaozhi/shared-playback.patch
integrations/xiaozhi/sounds.patch
src/axel_f_phrase.h
src/brass_wavetables.h
src/tone_noise_generator.cc
tests/adapter_tests.cc
tests/analyze_brass.py
tests/analyze_ocean.py
tests/analyze_pcm.py
tests/check_callbacks.py
tests/check_installer.py
tests/check_level_edges.py
tests/check_shared_playback.py
tests/generator_tests.cc
tests/ocean_tests.cc
tests/resume_tests.cc
tests/saw_tests.cc
tests/support/application.h
tests/support/board.h
tests/support/mcp_server.h
tests/synth_tests.cc
tests/tools_tests.cc
tests/volume_tests.cc
tools/audit_candidate.py
tools/generate_brass_tables.py
tools/install_example_board.py
tools/install_xiaozhi.py
tools/test.py
versions.json
'''.split())
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--write-checksums', action='store_true')
args = parser.parse_args()
files = {str(p.relative_to(ROOT)) for p in ROOT.rglob('*') if (p.is_file() or p.is_symlink()) and '.git' not in p.relative_to(ROOT).parts}
expected = ALLOWED | ({'SHA256SUMS'} if 'SHA256SUMS' in files else set())
if files != expected:
    raise SystemExit('Unexpected/missing candidate paths: '+repr(sorted(files ^ expected)))
manifest = []
mac = re.compile(rb'(?i)(?<![0-9a-f])(?:[0-9a-f]{2}:){5}[0-9a-f]{2}(?![0-9a-f])')
private_roots = [b'/'+part.encode()+b'/' for part in ['home', 'Users', 'tmp']]
for name in sorted(ALLOWED):
    path = ROOT/name
    if path.is_symlink():
        raise SystemExit('Symlink refused: '+name)
    data = path.read_bytes()
    if b'\0' in data or mac.search(data) or any(prefix in data for prefix in private_roots):
        raise SystemExit('Binary, identifier or absolute private path in '+name)
    manifest.append(hashlib.sha256(data).hexdigest()+'  '+name)
text = '\n'.join(manifest)+'\n'
checksums = ROOT/'SHA256SUMS'
if args.write_checksums:
    checksums.write_text(text)
elif checksums.exists() and checksums.read_text() != text:
    raise SystemExit('Checksum manifest differs; review edits before regenerating.')
print('Allowlist/privacy shape passed: '+str(len(ALLOWED))+' source/docs files; no binaries, histories or private paths.')
